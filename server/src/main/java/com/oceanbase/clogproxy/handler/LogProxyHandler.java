package com.oceanbase.clogproxy.handler;

import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import com.oceanbase.clogproxy.common.util.Decoder;
import com.oceanbase.clogproxy.common.util.NetworkUtil;
import com.oceanbase.clogproxy.obaccess.ClientObConf;
import com.oceanbase.clogproxy.obaccess.OBAccess;
import com.oceanbase.clogproxy.packet.ClientHandshakeResponse;
import com.oceanbase.clogproxy.packet.ClientMeta;
import com.oceanbase.clogproxy.packet.ResponseCode;
import com.oceanbase.clogproxy.stream.StreamManager;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufAllocator;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Date;

import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.ClientHandShake;
import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.PbPacket;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-14 Time: 13:50</p>
 *
 * @author jiyong.jy
 */
public class LogProxyHandler extends BaseHandler {

    protected LogProxyHandler() {
        inbuff = ByteBufAllocator.DEFAULT.buffer(Conf.CLIENT_BUFFER_INIT_BYTES);
        dispatcher.register(ProtocolVersion.V0, HeaderType.HANDSHAKE_REQUEST_CLIENT,
            HandlerDispatcher.wrap(this::handleHandshakeV0), aVoid -> this.resetState());
        dispatcher.register(ProtocolVersion.V1, HeaderType.HANDSHAKE_REQUEST_CLIENT,
            HandlerDispatcher.wrap(this::handleHandshakeV1), aVoid -> this.resetState());
    }

    public static LogProxyHandler newInstance() {
        return new LogProxyHandler();
    }

    private static final Logger logger = LoggerFactory.getLogger(LogProxyHandler.class);
    private static final String IP = NetworkUtil.getLocalIp();

    ///// protocol V0
    protected enum HandshakeState {
        LOG_TYPE,
        CLIENT_IP,
        CLIENT_ID,
        CLIENT_VERSION,
        CONFIGURATION,
        TO_REQUEST,
        COMPLETE
    }

    protected HandshakeState handshakeState = null;

    protected ClientMeta client = new ClientMeta();

    public boolean handleHandshakeV0(ChannelHandlerContext ctx) throws Exception {
        if (handshakeState == null) {
            handshakeState = HandshakeState.LOG_TYPE;
        }

        while (pollflag) {
            switch (handshakeState) {
                case LOG_TYPE:
                    if (inbuff.readableBytes() < Byte.BYTES) {
                        return false;
                    }
                    handleLogType(ctx, inbuff.readByte());
                    handshakeState = HandshakeState.CLIENT_IP;
                    break;

                case CLIENT_IP:
                    String clientIp = Decoder.decodeStringInt(inbuff);
                    if (clientIp == null) {
                        return false;
                    }
                    if (Conf.VERBOSE_PACKET) {
                        logger.info("income Client IP: {}", clientIp);
                    }
                    client.setIp(clientIp);
                    handshakeState = HandshakeState.CLIENT_ID;
                    break;

                case CLIENT_ID:
                    String clientId = Decoder.decodeStringInt(inbuff);
                    if (clientId == null) {
                        return false;
                    }
                    if (Conf.VERBOSE_PACKET) {
                        logger.info("income Client ID: {}", clientId);
                    }
                    client.setClientId(clientId);
                    handshakeState = HandshakeState.CLIENT_VERSION;
                    break;

                case CLIENT_VERSION:
                    String version = Decoder.decodeStringInt(inbuff);
                    if (version == null) {
                        return false;
                    }
                    if (Conf.VERBOSE_PACKET) {
                        logger.info("income version: {}", version);
                    }
                    client.setVersion(version);
                    handshakeState = HandshakeState.CONFIGURATION;
                    break;

                case CONFIGURATION:
                    String configuration = Decoder.decodeStringInt(inbuff);
                    if (configuration == null) {
                        return false;
                    }
                    client.setConfiguration(configuration);
                    logger.info("Client incoming: {}", client.toString());
                    handshakeState = HandshakeState.TO_REQUEST;
                    break;

                case TO_REQUEST:
                    authOb(ctx);
                    writeIpAndVersion(ctx.channel());
                    handshakeState = HandshakeState.COMPLETE;
                    break;

                case COMPLETE:
                    client.setId(ctx.channel());
                    client.setRegisterTime(new Date());
                    if (!StreamManager.instance().createStream(client, ctx.channel())) {
                        writeErrorPacket(ctx.channel(), ResponseCode.ERR_INIT.code(), "failed to create stream");
                        throw new RuntimeException("failed to create stream");
                    }
                    handshakeState = HandshakeState.LOG_TYPE;
                    resetState();
                    return true;
            }
        }
        return false;
    }

    public boolean handleHandshakeV1(ChannelHandlerContext ctx) throws Exception {
        // We don't care handshakeState because BaseHandler got all packet.
        PbPacket packet = ctx.channel().attr(PB_PACK_CTX_KEY).get();
//        if (packet.getCompressType() != CompressType.NONE.code()) {
            // TODO.. decompress
//        }
        ClientHandShake handshake = ClientHandShake.parseFrom(packet.getPayload());
        if (handshake == null) {
            writeErrorPacket(ctx.channel(), ResponseCode.ERR_PACKET.code(), "Invalid PB Packet of ClientHandShake");
            throw new RuntimeException("Invalid PB Packet of ClientHandShake");
        }
        if (Conf.VERBOSE_PACKET) {
            logger.info("Incoming Handshake: {}", handshake.toString().replace(Conf.OB_SYS_PASSWORD, "******"));
        }
        handleLogType(ctx, handshake.getLogType());
        client.setVersion(handshake.getClientVersion());
        client.setClientId(handshake.getClientId());
        client.setIp(handshake.getClientIp());
        client.setConfiguration(handshake.getConfiguration());
        client.setEnableMonitor(handshake.getEnableMonitor());
        client.setId(ctx.channel());
        client.setRegisterTime(new Date());

        authOb(ctx);
        writeIpAndVersion(ctx.channel());
        if (!StreamManager.instance().createStream(client, ctx.channel())) {
            writeErrorPacket(ctx.channel(), ResponseCode.ERR_INIT.code(), "failed to create stream");
            throw new RuntimeException("failed to create stream");
        }

        resetState();
        return true;
    }

    private void handleLogType(ChannelHandlerContext ctx, int code) throws Exception {
        LogType logType = LogType.fromCode(code);
        if (logType == null) {
            resetState();
            writeErrorPacket(ctx.channel(), ResponseCode.ERR_PACKET.code(), "invalid log type");
            throw new RuntimeException("invalid income log type: " + code);
        }
        if (Conf.VERBOSE_PACKET) {
            logger.info("income logtype: {}", logType);
        }
        client.setType(logType);
    }

    private void authOb(ChannelHandlerContext ctx) throws Exception {
        // parse configuration and replace user account to sys user.
        ClientObConf conf = new ClientObConf();
        String sysConfiguration = ClientObConf.fromConfiguration(client.getConfiguration(), conf);
        if (StringUtils.isEmpty(sysConfiguration)) {
            writeErrorPacket(ctx.channel(), ResponseCode.ERR_CONF.code(), "invalid configuration");
            throw new RuntimeException("invalid client configuration: " + client.getConfiguration());
        }

        // authentication first
        if (client.getType() == LogType.OCEANBASE && Conf.AUTH_USER && !Conf.ALLOW_ALL_TENANT) {
            logger.info("start to auth Client Conf: {}", conf.toString());
            if (!OBAccess.instance().authUserTenants(conf)) {
                writeErrorPacket(ctx.channel(), ResponseCode.NO_AUTH.code(), "failed to auth");
                throw new RuntimeException("failed to auth client");
            }
            logger.info("SUCC to auth Client Conf: {}", conf.toString());
        }

        client.setConfiguration(sysConfiguration);
    }

    private void writeIpAndVersion(Channel channel) throws Exception {
        ClientHandshakeResponse response = new ClientHandshakeResponse();
        response.setCode(ResponseCode.SUCC.code());
        response.setIp(IP);
        response.setVersion(Conf.VERSION);
        channel.writeAndFlush(response.serialize()).addListener((ChannelFutureListener) channelFuture -> {
            if (channelFuture.isSuccess()) {
                logger.info("write version success, ClientId: {}, version: {}", client.getClientId(), response.getCode());
            } else {
                logger.error("failed to write version, ClientId: {}, ", client.getClientId(), channelFuture.cause());
                throw new RuntimeException("failed to write version to Client");
            }
        }).sync();
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        pollflag = true;
    }

    @Override
    public void channelRead(ChannelHandlerContext ctx, Object msg) throws Exception {
        if (msg instanceof ByteBuf) {
            ByteBuf byteBuf = (ByteBuf) msg;
            int readableSize = byteBuf.readableBytes();
            int writableSize = inbuff.writableBytes();
            if (readableSize > writableSize) {
                inbuff.capacity(inbuff.capacity() + readableSize - writableSize);
            }
            inbuff.writeBytes(byteBuf);
            byteBuf.release();
        }
        if (Conf.VERBOSE_PACKET) {
            logger.info("{}, byte buffer fired, size: {}", hashCode(), inbuff.readableBytes());
        }
        pool(ctx);
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        pollflag = false;
        resetState();

        if (!ignore) {
            logger.error("Connection Broken of Client: {}", client.toString());
            StreamManager.instance().stopStreamByClient(ctx.channel());
        }
        releaseBuff();

        ctx.channel().close();
        ctx.disconnect();
        ctx.close();
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        pollflag = false;
        resetState();

        if (!ignore) {
            logger.error("Exception occured of Client: {}, ", client.toString(), cause);
            StreamManager.instance().stopStreamByClient(ctx.channel());
        }
        releaseBuff();

        ctx.channel().close();
        ctx.disconnect();
        ctx.close();
    }
}
