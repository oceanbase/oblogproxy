package com.oceanbase.clogproxy.client.connection;

import com.google.protobuf.InvalidProtocolBufferException;
import com.oceanbase.clogproxy.client.ErrorCode;
import com.oceanbase.clogproxy.client.LogProxyClientException;
import com.oceanbase.clogproxy.client.config.ClientConf;
import com.oceanbase.clogproxy.client.packet.HandshakeResponse;
import com.oceanbase.clogproxy.common.packet.CompressType;
import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import com.oceanbase.clogproxy.common.util.Decoder;
import com.oceanbase.clogproxy.common.util.NetworkUtil;
import com.oceanbase.oms.record.oms.OmsRecord;
import com.oceanbase.oms.store.client.message.drcmessage.DrcNETBinaryRecord;
import com.oceanbase.oms.store.client.message.drcmessage.OmsRecordTransfer;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufAllocator;
import io.netty.buffer.ByteBufUtil;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.handler.codec.ByteToMessageDecoder.Cumulator;
import io.netty.handler.timeout.IdleStateEvent;
import net.jpountz.lz4.LZ4Factory;
import net.jpountz.lz4.LZ4FastDecompressor;
import org.apache.commons.lang3.Conversion;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.BlockingQueue;

import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.ClientHandShake;
import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.PbPacket;
import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.RuntimeStatus;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 13:55</p>
 *
 * @author jiyong.jy
 */
public class ClientHandler extends ChannelInboundHandlerAdapter {

    private static final Logger logger = LoggerFactory.getLogger(ClientHandler.class);

    private static final byte[] MAGIC_STRING = new byte[]{'x', 'i', '5', '3', 'g', ']', 'q'};
    private static final String CLIENT_IP = NetworkUtil.getLocalIp();

    private ClientStream stream;
    private ConnectionParams params;
    private BlockingQueue<StreamContext.TransferPacket> recordQueue;

    enum HandshakeState {
        /**
         * data transfer protocol version
         */
        PROTOCOL_VERSION,
        HEADER_CODE,
        RESPONSE_CODE,
        MESSAGE,
        LOGPROXY_IP,
        LOGPROXY_VERSION,
        STREAM
    }

    private HandshakeState state = HandshakeState.PROTOCOL_VERSION;
    HandshakeResponse handshake = new HandshakeResponse();

    private final Cumulator cumulator = ByteToMessageDecoder.MERGE_CUMULATOR;
    ByteBuf buffer;
    private boolean poolflag = true;
    private boolean first;
    private int numReads = 0;
    private boolean dataNotEnough = false;

    LZ4Factory factory = LZ4Factory.fastestInstance();
    LZ4FastDecompressor fastDecompressor = factory.fastDecompressor();

    public ClientHandler() { }

    protected void resetState() {
        state = HandshakeState.PROTOCOL_VERSION;
    }

    @Override
    public void channelRead(ChannelHandlerContext ctx, Object msg) throws Exception {
        if (msg instanceof ByteBuf) {
            dataNotEnough = false;
            ByteBuf data = (ByteBuf) msg;
            first = buffer == null;
            if (first) {
                buffer = data;
            } else {
                buffer = cumulator.cumulate(ctx.alloc(), buffer, data);
            }
        } else if (msg instanceof IdleStateEvent) {
            if (stream != null) {
                stream.triggerReconnect();
            }
            return;
        } else {
            return;
        }

        while (poolflag && buffer.isReadable() && !dataNotEnough) {
            switch (state) {
                case PROTOCOL_VERSION:
                    if (buffer.readableBytes() >= Short.BYTES) {
                        int code = buffer.readShort();
                        ProtocolVersion version = ProtocolVersion.codeOf(code);
                        if (version == null) {
                            resetState();
                            logger.error("unsupport protocol version: {}", code);
                            throw new LogProxyClientException(ErrorCode.E_PROTOCOL, "unsupport protocol version: " + code);
                        }
                        state = HandshakeState.HEADER_CODE;
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                case HEADER_CODE:
                    if (buffer.readableBytes() >= Integer.BYTES) {
                        int code = buffer.readInt();

                        if ((code != HeaderType.HANDSHAKE_RESPONSE_CLIENT.code()) &&
                            (code != HeaderType.ERROR_RESPONSE.code())) {
                            resetState();
                            logger.error("unexpected Header Type, expected: {}({}), income: {}",
                                HeaderType.HANDSHAKE_RESPONSE_CLIENT.code(),
                                HeaderType.HANDSHAKE_RESPONSE_CLIENT.name(), code);
                            throw new LogProxyClientException(ErrorCode.E_HEADER_TYPE, "unexpected Header Type: " + code);
                        }
                        state = HandshakeState.RESPONSE_CODE;
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                case RESPONSE_CODE:
                    if (buffer.readableBytes() >= 4) {
                        int code = buffer.readInt();
                        if (code != 0) {
                            state = HandshakeState.MESSAGE;
                        } else {
                            state = HandshakeState.LOGPROXY_IP;
                        }
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                case MESSAGE:
                    String message = Decoder.decodeStringInt(buffer);
                    if (message != null) {
                        resetState();
                        logger.error("LogProxy refused handshake request: {}", message);
                        throw new LogProxyClientException(ErrorCode.NO_AUTH, "LogProxy refused handshake request: " + message);
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                case LOGPROXY_IP:
                    String logProxyIp = Decoder.decodeStringByte(buffer);
                    if (logProxyIp != null) {
                        handshake.setIp(logProxyIp);
                        this.state = HandshakeState.LOGPROXY_VERSION;
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                case LOGPROXY_VERSION:
                    String logProxyVersion = Decoder.decodeStringByte(buffer);
                    if (logProxyVersion != null) {
                        handshake.setVersion(logProxyVersion);
                        logger.info("Connected to LogProxy: {}", handshake.toString());
                        this.state = HandshakeState.STREAM;
                    } else {
                        dataNotEnough = true;
                    }
                    break;

                case STREAM:
                    parseData();
                    dataNotEnough = true;
                    break;

                default:
                    throw new LogProxyClientException(ErrorCode.E_INNER, "netty state error, unexpected");
            }
        }

        if (buffer != null && !buffer.isReadable()) {
            numReads = 0;
            buffer.release();
            buffer = null;
        } else if (++numReads >= ClientConf.NETTY_DISCARD_AFTER_READS) {
            numReads = 0;
            discardSomeReadBytes();
        }
    }

    private void parseData() throws LogProxyClientException {
        // TODO... parse data exception handle
        while (poolflag && buffer.readableBytes() >= 2) {
            buffer.markReaderIndex();

            int code = buffer.readShort();
            ProtocolVersion version = ProtocolVersion.codeOf(code);
            if (version == null) {
                resetState();
                logger.error("unsupport protocol version: {}", code);
                throw new LogProxyClientException(ErrorCode.E_PROTOCOL, "unsupport protocol version: " + code);
            }
            boolean go;
            switch (version) {
                case V1:
                    go = parseDataV1();
                    break;
                case V0:
                default:
                    go = parseDataV0();
            }

            if (!go) {
                break;
            }
        }
    }

    private boolean parseDataV0() {
        if (buffer.readableBytes() < 8) {
            buffer.resetReaderIndex();
            return false;
        }
        int code = buffer.readInt();
        if (code != HeaderType.DATA_CLIENT.code()) {
            resetState();
            logger.error("unexpected Header Type, expected: {}({}), income: {}",
                HeaderType.DATA_CLIENT.code(), HeaderType.DATA_CLIENT.name(), code);
            throw new LogProxyClientException(ErrorCode.E_HEADER_TYPE, "unexpected Header Type: " + code);
        }

        int dataLength = buffer.readInt();
        if (buffer.readableBytes() < dataLength) {
            buffer.resetReaderIndex();
            return false;
        }

        code = buffer.readByte();
        if (CompressType.codeOf(code) == null) {
            throw new LogProxyClientException(ErrorCode.E_COMPRESS_TYPE, "unexpected Compress Type: " + code);
        }

        int totalLength = buffer.readInt();
        int rawDataLength = buffer.readInt();
        byte[] rawData = new byte[rawDataLength];
        buffer.readBytes(rawData);
        if (code == CompressType.LZ4.code()) {
            byte[] bytes = new byte[totalLength];
            int decompress = fastDecompressor.decompress(rawData, 0, bytes, 0, totalLength);
            if (decompress != rawDataLength) {
                throw new LogProxyClientException(ErrorCode.E_LEN, "decompressed length [" + decompress
                    + "] is not expected [" + rawDataLength + "]");
            }
            parseRecord(bytes);
        } else {
            parseRecord(rawData);
        }
        // complete
        return true;
    }

    private boolean parseDataV1() {
        if (buffer.readableBytes() < 4) {
            buffer.resetReaderIndex();
            return false;
        }
        int length = buffer.readInt();
        if (buffer.readableBytes() < length) {
            buffer.resetReaderIndex();
            return false;
        }
        byte[] buff = new byte[length];
        buffer.readBytes(buff, 0, length);
        try {
            PbPacket packet = PbPacket.parseFrom(buff);

            if (packet.getCompressType() != CompressType.NONE.code()) {
                // TODO..
                throw new LogProxyClientException(ErrorCode.E_COMPRESS_TYPE, "Unsupport Compress Type: " + packet.getCompressType());
            }
            if (packet.getType() != HeaderType.STATUS.code()) {
                // TODO.. header type dispatcher
                throw new LogProxyClientException(ErrorCode.E_HEADER_TYPE, "Unsupport Header Type: " + packet.getType());
            }
            RuntimeStatus status = RuntimeStatus.parseFrom(packet.getPayload());
            if (status == null) {
                throw new LogProxyClientException(ErrorCode.E_PARSE, "Failed to read PB packet, empty Runtime Status");
            }

            while (true) {
                try {
                    recordQueue.put(new StreamContext.TransferPacket(status));
                    break;
                } catch (InterruptedException e) {
                    // do nothing
                }
            }

        } catch (InvalidProtocolBufferException e) {
            throw new LogProxyClientException(ErrorCode.E_PARSE, "Failed to read PB packet", e);
        }
        return true;
    }

    private void parseRecord(byte[] bytes) throws LogProxyClientException {
        int offset = 0;
        while (offset < bytes.length) {
            int index = Conversion.byteArrayToInt(bytes, offset, 0, 0, 4);
            index = ByteBufUtil.swapInt(index);

            int dataLength = Conversion.byteArrayToInt(bytes, offset + 4, 0, 0, 4);
            dataLength = ByteBufUtil.swapInt(dataLength);

            OmsRecord record;
            try {
                /*
                 * We must copy a byte array and call parse after then,
                 * or got a !!!RIDICULOUS FUCKING EXCEPTION!!!,
                 * if we wrap a upooled buffer with offset and call setByteBuf just as same as `parse` function do.
                 */
                DrcNETBinaryRecord drcRecord = new DrcNETBinaryRecord(false);
                byte[] data = new byte[dataLength];
                System.arraycopy(bytes, offset + 8, data, 0, data.length);
                drcRecord.parse(data);
                if (ClientConf.IGNORE_UNKNOWN_RECORD_TYPE && !OmsRecordTransfer.isValidType(drcRecord)) {
                    // unsupport type, ignore
                    logger.debug("Unsupport record type: {}", drcRecord.toString());
                    offset += (8 + dataLength);
                    continue;
                }
                record = (OmsRecord) OmsRecordTransfer.transfer(drcRecord);

            } catch (Exception e) {
                throw new LogProxyClientException(ErrorCode.E_PARSE, e);
            }

            while (true) {
                try {
                    recordQueue.put(new StreamContext.TransferPacket(record));
                    break;
                } catch (InterruptedException e) {
                    // do nothing
                }
            }

            offset += (8 + dataLength);
        }
    }

    protected final void discardSomeReadBytes() {
        if (buffer != null && !first && buffer.refCnt() == 1) {
            // discard some bytes if possible to make more room in the
            // buffer but only if the refCnt == 1  as otherwise the user may have
            // used slice().retain() or duplicate().retain().
            //
            // See:
            // - https://github.com/netty/netty/issues/2327
            // - https://github.com/netty/netty/issues/1764
            buffer.discardSomeReadBytes();
        }
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        poolflag = true;

        StreamContext context = ctx.channel().attr(ConnectionFactory.CONTEXT_KEY).get();
        stream = context.stream();
        params = context.getParams();
        recordQueue = context.recordQueue();

        logger.info("ClientId: {} connecting LogProxy: {}", params.info(), NetworkUtil.parseRemoteAddress(ctx.channel()));
        ctx.channel().writeAndFlush(generateConnectRequest(params.getProtocolVersion())).sync();
    }

    public ByteBuf generateConnectRequestV1() {

        ClientHandShake handShake = ClientHandShake.newBuilder().
            setLogType(params.getLogType().getCode()).
            setClientIp(CLIENT_IP).
            setClientId(params.getClientId()).
            setClientVersion(ClientConf.VERSION).
            setEnableMonitor(params.isEnableMonitor()).
            setConfiguration(params.getConfigurationString()).
            build();

        PbPacket packet = PbPacket.newBuilder().
            setType(HeaderType.HANDSHAKE_REQUEST_CLIENT.code()).
            setCompressType(CompressType.NONE.code()).
            setPayload(handShake.toByteString()).build();

        byte[] packetBytes = packet.toByteArray();
        ByteBuf byteBuf = ByteBufAllocator.DEFAULT.buffer(MAGIC_STRING.length + 2 + 4 + packetBytes.length);
        byteBuf.writeBytes(MAGIC_STRING);
        byteBuf.writeShort(ProtocolVersion.V1.code());
        byteBuf.writeInt(packetBytes.length);
        byteBuf.writeBytes(packetBytes);
        return byteBuf;
    }

    public ByteBuf generateConnectRequest(ProtocolVersion version) {
        if (version == ProtocolVersion.V1) {
            return generateConnectRequestV1();
        }

        ByteBuf byteBuf = ByteBufAllocator.DEFAULT.buffer(MAGIC_STRING.length);
        byteBuf.writeBytes(MAGIC_STRING);

        // header
        byteBuf.capacity(byteBuf.capacity() + 2 + 4 + 1);
        byteBuf.writeShort(ProtocolVersion.V0.code());
        byteBuf.writeInt(HeaderType.HANDSHAKE_REQUEST_CLIENT.code());
        byteBuf.writeByte(params.getLogType().getCode());

        // body
        int length = CLIENT_IP.length();
        byteBuf.capacity(byteBuf.capacity() + length + 4);
        byteBuf.writeInt(length);
        byteBuf.writeBytes(CLIENT_IP.getBytes());

        length = params.getClientId().length();
        byteBuf.capacity(byteBuf.capacity() + length + 4);
        byteBuf.writeInt(length);
        byteBuf.writeBytes(params.getClientId().getBytes());

        length = ClientConf.VERSION.length();
        byteBuf.capacity(byteBuf.capacity() + length + 4);
        byteBuf.writeInt(length);
        byteBuf.writeBytes(ClientConf.VERSION.getBytes());

        length = params.getConfigurationString().length();
        byteBuf.capacity(byteBuf.capacity() + length + 4);
        byteBuf.writeInt(length);
        byteBuf.writeBytes(params.getConfigurationString().getBytes());

//        logger.info("Handshake: Magic: {}, IP: {}, ID: {}, Version: {}, Configuration: {}",
//                MAGIC_STRING, CLIENT_IP, params.getClientId(), ClientConf.VERSION, params.getConfigurationString());

        return byteBuf;
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        poolflag = false;

        logger.info("Connect broken of ClientId: {} with LogProxy: {}", params.info(), NetworkUtil.parseRemoteAddress(ctx.channel()));
        ctx.channel().disconnect();
        ctx.close();

        if (stream != null) {
            stream.triggerReconnect();
        }
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        poolflag = false;
        resetState();

        logger.error("Exception occured ClientId: {}, with LogProxy: {}", params.info(), NetworkUtil.parseRemoteAddress(ctx.channel()), cause);
        ctx.channel().disconnect();
        ctx.close();

        if (stream != null) {
            if (cause instanceof LogProxyClientException) {
                if (((LogProxyClientException) cause).needStop()) {
                    stream.stop();
                    stream.triggerException((LogProxyClientException) cause);
                }

            } else {
                stream.triggerReconnect();
            }
        }
    }
}
