package com.oceanbase.clogproxy.handler;

import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto;
import com.oceanbase.clogproxy.packet.ErrorResponse;
import com.oceanbase.clogproxy.packet.PacketConstants;
import com.oceanbase.clogproxy.packet.ResponseCode;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.buffer.ByteBuf;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelId;
import io.netty.channel.ChannelInboundHandlerAdapter;
import io.netty.util.AttributeKey;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-17
 */
public class BaseHandler extends ChannelInboundHandlerAdapter {
    private static final Logger logger = LoggerFactory.getLogger(BaseHandler.class);

    protected HandlerDispatcher dispatcher = HandlerDispatcher.newInstance();

    protected boolean pollflag = true;
    protected ByteBuf inbuff = null;
    protected boolean dataNotEnough = false;

    protected HandlerState state = HandlerState.MAGIC_NUMBER;
    protected HeaderType type = null;
    protected ProtocolVersion version = null;
    protected int protocolV1Length = 0;

    public static final AttributeKey<LogProxyProto.PbPacket> PB_PACK_CTX_KEY = AttributeKey.valueOf("PB_PACKET");

    /**
     * If there comes no-LogProxyClient packet, like SLB heartbeat or unexpected connect,
     * we should ignore it. Only when LogProxyClient with a magic number we treat it as a available packet.
     */
    boolean ignore = true;

    protected boolean resetState() {
        state = HandlerState.MAGIC_NUMBER;
        type = null;
        return true;
    }

    protected void releaseBuff() {
        if (inbuff == null) {
            return;
        }
        int count = inbuff.refCnt();
        if (count > 0) {
            inbuff.release();
            logger.info("Release Inbuff of {}, refCount from {} to {}", Thread.currentThread().getName(), count, inbuff.refCnt());
        }
    }

    protected void writeErrorPacket(Channel channel, int code, String message) throws Exception {
        ChannelId id = channel.id();
        ErrorResponse response = new ErrorResponse(code, message);
        channel.writeAndFlush(response.serialize()).addListener((ChannelFutureListener) channelFuture -> {
            if (!channelFuture.isSuccess()) {
                logger.error("failed to error response, client id: {}", id.asShortText(), channelFuture.cause());
            } else {
                logger.info("write error response to client id: {}, {}", id.asShortText(), response.toString());
            }
        }).sync();
    }

    protected boolean decodeMagicString() {
        if (inbuff.readableBytes() < PacketConstants.MAGIC_STRING.length) {
            return false;
        }
        byte[] bytes = new byte[PacketConstants.MAGIC_STRING.length];
        inbuff.readBytes(bytes);
        if (!Arrays.equals(bytes, PacketConstants.MAGIC_STRING)) {
            resetState();
            logger.info("unexpected packet magic number, expected:{}, income: {}", PacketConstants.MAGIC_STRING, bytes);
            throw new RuntimeException("unexpected packet magic number");
        }
        if (Conf.VERBOSE_PACKET) {
            logger.info("income magic number: {}", bytes);
        }
        return true;
    }

    protected boolean decodeProtocolVersion(ChannelHandlerContext ctx) throws Exception {
        if (inbuff.readableBytes() < Short.BYTES) {
            return false;
        }
        short code = inbuff.readShort();
        version = ProtocolVersion.codeOf(code);
        if (version == null) {
            resetState();
            logger.info("invalid income protocol version: {}", code);
            writeErrorPacket(ctx.channel(), ResponseCode.ERR_PACKET.code(), "invalid protocol version");
            throw new RuntimeException("invalid income protocol version");
        }
        if (Conf.VERBOSE_PACKET) {
            logger.info("income version: {}", version);
        }
        return true;
    }

    protected boolean decodeHeaderType(ChannelHandlerContext ctx) throws Exception {
        if (inbuff.readableBytes() < Integer.BYTES) {
            return false;
        }
        handleHeaderType(ctx, inbuff.readInt());
        return true;
    }

    protected void handleHeaderType(ChannelHandlerContext ctx, int code) throws Exception {
        type = HeaderType.codeOf(code);
        if (type == null) {
            resetState();
            logger.info("invalid income header type: {}", code);
            writeErrorPacket(ctx.channel(), ResponseCode.ERR_PACKET.code(), "invalid header type");
            throw new RuntimeException("invalid income header type");
        }
        if (Conf.VERBOSE_PACKET) {
            logger.info("income header type: {}", type);
        }
    }

    protected void pool(ChannelHandlerContext ctx) throws Exception {
        dataNotEnough = false;
        while (pollflag && inbuff.isReadable() && !dataNotEnough) {
            switch (state) {
                case MAGIC_NUMBER:
                    if (!decodeMagicString()) {
                        dataNotEnough = true;
                    } else {
                        ignore = false; // available packet
                        state = HandlerState.PROTOCOL_VERSION;
                    }
                    break;

                case PROTOCOL_VERSION:
                    if (!decodeProtocolVersion(ctx)) {
                        dataNotEnough = true;
                    } else {
                        switch (version) {
                            case V1:
                                state = HandlerState.PROTOBUF_PREFIX;
                                break;
                            case V0:
                            default:
                                state = HandlerState.HEADER_CODE;
                        }
                    }
                    break;

                case PROTOBUF_PREFIX:
                    if (inbuff.readableBytes() < Integer.BYTES) {
                        dataNotEnough = true;
                    } else {
                        protocolV1Length = inbuff.readInt();
                        state = HandlerState.PROTOBUF;
                    }
                    break;

                case HEADER_CODE:
                    if (!decodeHeaderType(ctx)) {
                        dataNotEnough = true;
                    } else {
                        state = HandlerState.PARSE;
                    }
                    break;

                case PROTOBUF:
                    if (inbuff.readableBytes() < protocolV1Length) {
                        dataNotEnough = true;
                        break;
                    } else {
                        byte[] buff = new byte[protocolV1Length];
                        inbuff.readBytes(buff, 0, protocolV1Length);
                        LogProxyProto.PbPacket packet = LogProxyProto.PbPacket.parseFrom(buff);
                        handleHeaderType(ctx, packet.getType());
                        ctx.channel().attr(PB_PACK_CTX_KEY).set(packet);
                        state = HandlerState.PARSE;
                    }
                    // We don't break here, call dispatch directly since we got all packet
                case PARSE:
                    if (dispatcher.dispatch(version, type, ctx)) { // request complete
                        state = HandlerState.MAGIC_NUMBER;
                        resetState();
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                default:
                    throw new RuntimeException("!!! Internal Error, Invalid dispatcher state: " + this.state);
            }
        }
    }
}
