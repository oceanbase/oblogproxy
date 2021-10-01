package com.oceanbase.clogproxy.packet;

import com.google.common.collect.Lists;
import com.oceanbase.clogproxy.common.packet.CompressType;
import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufAllocator;
import io.netty.buffer.CompositeByteBuf;
import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 11:10</p>
 *
 * @author jiyong.jy
 */
public class PacketsEncoder {

    public static class Result {
        private int count;
        private int byteSize;
        private CompositeByteBuf buf;
        private long encodeCost;

        public Result(int count, int byteSize, CompositeByteBuf buf, long encodeCost) {
            this.count = count;
            this.byteSize = byteSize;
            this.buf = buf;
            this.encodeCost = encodeCost;
        }

        public Result(int count, int byteSize, CompositeByteBuf buf) {
            this(count, byteSize, buf, 0);
        }

        @Override
        public String toString() {
            return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
        }

        public int getCount() {
            return count;
        }

        public int getByteSize() {
            return byteSize;
        }

        public CompositeByteBuf getBuf() {
            return buf;
        }

        public long getEncodeCost() {
            return encodeCost;
        }

        public void setEncodeCost(long encodeCost) {
            this.encodeCost = encodeCost;
        }
    }

    private LZ4Compressor compressor = LZ4Factory.fastestInstance().fastCompressor();

    /**
     * @link https://yuque.antfin-inc.com/antdrc/ove6yh/communicate-protocol
     *
     * <pre>
     *
     *      --------------------------
     *      | [2]Protocol Version    |
     *      --------------------------
     *      | [4]Header Code         |
     *      --------------------------
     *      | [4]Packet Length       |
     *      --------------------------
     *      | [1]Compress Type       |
     *      --------------------------
     *      | [4]Original Length     |
     *      --------------------------
     *      | [4]Compressed Length   |
     *      --------------------------
     *      | [+]Data List Buffer    |
     *      --------------------------
     *
     * </pre>
     */
    public List<Result> encodeV0(List<Packet> packets) {
        return encodeLogReaderCompressed(packets);
    }

    private List<Result> encodeLogReaderCompressed(List<Packet> packets) {
        List<Result> results = new ArrayList<>();

        for (Packet packet : packets) {
            long start = System.currentTimeMillis();

            int packetLength = packet.getRecordsLength();
            CompositeByteBuf cbuf = newCBufHeader(packetLength);

//            System.out.println("compress type: " + packet.getRecords().readByte() + ", original len: " +
//                    packet.getRecords().readInt() + ", compress len: " + packet.getRecords().readInt());
            packet.getRecords().readerIndex(0);
            cbuf.addComponent(true, 1, packet.getRecords());

            long end = System.currentTimeMillis();
            results.add(new Result(packets.size(), PacketConstants.RESPONSE_HEADER_LEN +
                packetLength, cbuf, end - start));
        }
        return results;
    }

    private CompositeByteBuf newCBufHeader(int length) {
        CompositeByteBuf cbuf = ByteBufAllocator.DEFAULT.compositeBuffer(2);
        ByteBuf buf = ByteBufAllocator.DEFAULT.buffer(PacketConstants.RESPONSE_HEADER_LEN);
        // header
        buf.writeShort(ProtocolVersion.V0.code());
        buf.writeInt(HeaderType.DATA_CLIENT.code());

        buf.writeInt(length);
        cbuf.addComponent(true, 0, buf);
        return cbuf;
    }

    /**
     * <pre>
     *
     *      --------------------------
     *      | [2]Protocol Version    |
     *      --------------------------
     *      | [4]PB Packet Length    |
     *      --------------------------
     *      | PbPacket               |
     *      | ...                    |
     *      | Fields                 |
     *      | ...                    |
     *      | PbPacket.RuntimeStatus |
     *      --------------------------
     *
     * </pre>
     */
    public List<Result> encodeRuntimeStatusV1(LogProxyProto.RuntimeStatus status) {
        LogProxyProto.PbPacket packet = LogProxyProto.PbPacket.newBuilder().
            setType(HeaderType.STATUS.code()).
            setCompressType(CompressType.NONE.code()).
            setPayload(status.toByteString()).build();

        byte[] packetBytes = packet.toByteArray();
        int bufSize = 2 + 4 + packetBytes.length;
        CompositeByteBuf cbuf = ByteBufAllocator.DEFAULT.compositeBuffer(1);
        ByteBuf buf = ByteBufAllocator.DEFAULT.buffer(bufSize);
        // header
        buf.writeShort(ProtocolVersion.V1.code());

        buf.writeInt(packetBytes.length);
        buf.writeBytes(packet.toByteArray());
        cbuf.addComponent(true, 0, buf);

        return Collections.singletonList(new Result(1, bufSize, cbuf));
    }
}
