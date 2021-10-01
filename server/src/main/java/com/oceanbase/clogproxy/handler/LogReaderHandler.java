package com.oceanbase.clogproxy.handler;

import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import com.oceanbase.clogproxy.common.util.Decoder;
import com.oceanbase.clogproxy.metric.StreamMetric;
import com.oceanbase.clogproxy.metric.MetricConstants;
import com.oceanbase.clogproxy.packet.LogReaderHandshakeResp;
import com.oceanbase.clogproxy.packet.Packet;
import com.oceanbase.clogproxy.packet.ResponseCode;
import com.oceanbase.clogproxy.stream.SourceMeta;
import com.oceanbase.clogproxy.stream.StreamManager;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.buffer.ByteBuf;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.handler.codec.ByteToMessageDecoder.Cumulator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Date;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 14:37</p>
 *
 * @author jiyong.jy
 * <p>
 */
public class LogReaderHandler extends BaseHandler {

    private static final Logger logger = LoggerFactory.getLogger(LogReaderHandler.class);

    private LogReaderHandler() {
        dispatcher.register(ProtocolVersion.V0, HeaderType.HANDSHAKE_REQUEST_LOGREADER,
                HandlerDispatcher.wrap(this::handleHandshake), aVoid -> this.resetState());

        dispatcher.register(ProtocolVersion.V0, HeaderType.DATA_LOGREADER,
                HandlerDispatcher.wrap(this::handleDataStream), aVoid -> this.resetState());
    }

    public static LogReaderHandler newInstance() {
        return new LogReaderHandler();
    }

    // netty buffer related
    private Cumulator cumulator = ByteToMessageDecoder.MERGE_CUMULATOR;
    private int numReads = 0;
    private boolean first;

    /**
     * protocol bytes defines:
     * <p></p>
     * <pre>
     *
     * varstr:
     *      --------------------------------------
     *      | [4]length | variable lenght string |
     *      --------------------------------------
     *
     * shakehand - IN
     *      --------------------------
     *      | [4]header code         |
     *      --------------------------
     *      | [2]protocol version    |    request handshake version
     *      --------------------------
     *      | [varstr]client_id      |
     *      --------------------------
     *      | [varstr]reader version |
     *      --------------------------
     *      | [varchar] process id   |
     *      --------------------------
     *
     * shakehand - OUT
     *      --------------------------
     *      | [varstr]version        |
     *      --------------------------
     *
     * </pre>
     */
    private enum HandshakeState {
        CLIENT_ID,
        LOGREADER_VERSION,
        PROCESS_ID,
        TO_REQUEST,
        COMPLETE
    }

    private HandshakeState handshakeState = null;
    private SourceMeta logreader = new SourceMeta(LogType.OCEANBASE);

    private StreamManager stream = StreamManager.instance();

    private long lastTimestamp = 0;

    public boolean handleHandshake(ChannelHandlerContext ctx) throws Exception {
        if (handshakeState == null) {
            handshakeState = HandshakeState.CLIENT_ID;
        }

        while (pollflag) {
            switch (handshakeState) {
                case CLIENT_ID:
                    String clientId = Decoder.decodeStringInt(inbuff);
                    if (clientId == null) {
                        return false;
                    }
                    logreader.setClientId(clientId);
                    handshakeState = HandshakeState.LOGREADER_VERSION;
                    break;

                case LOGREADER_VERSION:
                    String readerVersion = Decoder.decodeStringInt(inbuff);
                    if (readerVersion == null) {
                        return false;
                    }
                    logreader.setVersion(readerVersion);
                    handshakeState = HandshakeState.PROCESS_ID;
                    break;

                case PROCESS_ID:
                    String processId = Decoder.decodeStringInt(inbuff);
                    if (processId == null) {
                        return false;
                    }
                    logreader.setProcessId(processId);
                    logreader.setId(ctx.channel());
                    logreader.setLastTime(new Date());
                    if (!StreamManager.instance().registerLogReader(logreader)) {
                        throw new RuntimeException("failed to register LogReader");
                    }
                    handshakeState = HandshakeState.TO_REQUEST;
                    break;

                case TO_REQUEST:
                    logger.info("LogReader handshake request: {}", logreader.toString());
                    writeVersion(ctx.channel());
                    handshakeState = HandshakeState.COMPLETE;
                    break;

                case COMPLETE:
                    handshakeState = HandshakeState.CLIENT_ID;
                    initMetric();
                    // mark to parse other packet
                    resetState();
                    return true;
            }
        }
        return false;
    }

    private void writeVersion(Channel channel) throws Exception {
        LogReaderHandshakeResp response = new LogReaderHandshakeResp();
        response.setCode(ResponseCode.SUCC.code());
        response.setVersion(Conf.VERSION);
        channel.writeAndFlush(response.serialize(HeaderType.HANDSHAKE_RESPONSE_LOGREADER)).addListener((ChannelFutureListener) channelFuture -> {
            if (channelFuture.isSuccess()) {
                logger.info("write version to LogReader success, client id: {}, response: {}", logreader.getClientId(), response.toString());
            } else {
                logger.error("failed to write version to LogReader, client id: {}", logreader.getClientId(), channelFuture.cause());
                throw new RuntimeException("failed to write version to LogReader");
            }
        }).sync();
    }

    private boolean handleDataStream(ChannelHandlerContext ctx) throws Exception {
        while (pollflag) {
            if (inbuff.readableBytes() < 4) {
                return false;
            }

            inbuff.markReaderIndex();
            int dataLength = inbuff.readInt();
            if (dataLength < Packet.minLength()) {
                logger.info("too small packet, length: {}", dataLength);
                throw new RuntimeException("invalid data stream packet");
            }

            if (inbuff.readableBytes() < dataLength) {
                inbuff.resetReaderIndex();
                return false;
            }

            Packet packet = Packet.decode(dataLength, inbuff);
            // FIXME... exception occured from here, release packet
            if (Conf.DEBUG && Conf.VERBOSE_PACKET) {
                logger.info(packet.debug());
            }

            int count = 1;
            if (Conf.COUNT_RECORD) {
                count = packet.recordCount();
                if (count == -1) {
                    logger.error("!!!failed to calculate record count!!!");
                    count = 0;
                }
            }

            if (!stream.push(logreader.getClientId(), packet)) {
                logger.error("failed to push packet to queue");
                throw new RuntimeException("failed to push packet to queue");
            }

            lastTimestamp = packet.getSafeTimestamp();
            stream.check(ctx.channel());
            metric(dataLength, count);
            resetState();
        }

        return true;
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        pollflag = true;
        resetState();

        logger.info("LogReader channel active");
    }

    @Override
    public void channelRead(ChannelHandlerContext ctx, Object msg) throws Exception {
        if (msg instanceof ByteBuf) {
            try {
                dataNotEnough = false;
                ByteBuf data = (ByteBuf) msg;
                first = (inbuff == null);
                if (first) {
                    inbuff = data;
                } else {
                    inbuff = cumulator.cumulate(ctx.alloc(), inbuff, data);
                }

                pool(ctx);

            } finally {
                if (inbuff != null && !inbuff.isReadable()) {
                    numReads = 0;
                    inbuff.release();
                    inbuff = null;
                } else if (++numReads >= Conf.NETTY_DISCARD_AFTER_READ) {
                    numReads = 0;
                    discardSomeReadBytes();
                }
            }

        } else {
            ctx.fireChannelRead(msg);
        }
    }

    protected final void discardSomeReadBytes() {
        if (inbuff != null && !first && inbuff.refCnt() == 1) {
            // discard some bytes if possible to make more room in the
            // buffer but only if the refCnt == 1  as otherwise the user may have
            // used slice().retain() or duplicate().retain().
            //
            // See:
            // - https://github.com/netty/netty/issues/2327
            // - https://github.com/netty/netty/issues/1764
            inbuff.discardSomeReadBytes();
        }
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        pollflag = false;
        resetState();

        logger.info("Connection Broken of LogReader: {}", logreader.toString());

        releaseBuff();

        cleanMetric();
        stream.stopStreamByLogReader(ctx.channel());

        ctx.channel().close();
        ctx.disconnect();
        ctx.close();
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        pollflag = false;
        resetState();

        logger.error("Exception occured of LogReader: {}, ", logreader.toString(), cause);

        releaseBuff();

        cleanMetric();
        stream.stopStreamByLogReader(ctx.channel());

        ctx.channel().close();
        ctx.disconnect();
        ctx.close();
    }

    private void metric(int length) {
        metric(length, 1);
    }

    private void metric(int length, int count) {
        StreamMetric.instance().rtps(logreader.getClientId().get(), count);
        StreamMetric.instance().rios(logreader.getClientId().get(), length);
    }

    private void initMetric() {
        StreamMetric.instance().addGauge(MetricConstants.METRIC_DELAY, logreader.getClientId().get(), () -> () -> System.currentTimeMillis() / 1000 - lastTimestamp);
    }

    private void cleanMetric() {
        StreamMetric.instance().remove(logreader.getClientId().get());
        if (Conf.READONLY) {
            StreamMetric.instance().remove(MetricConstants.METRIC_READONLY_CHANNEL_ID);
        }
    }
}
