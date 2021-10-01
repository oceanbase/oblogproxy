package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.packet.Packet;
import com.oceanbase.clogproxy.packet.PacketsEncoder;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufAllocator;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-14
 */
public class PacketsEncoderTest {

    private List<Packet> createPacket(int count, int recordLength) {
        List<Packet> packets = new ArrayList<>();

        for (int i = 0; i < count; ++i) {
            ByteBuf byteBuf = ByteBufAllocator.DEFAULT.buffer();
            byteBuf.writeInt(111); // index
            byteBuf.writeBytes(new byte[recordLength]);

            Packet packet = Packet.decode(recordLength + 4, byteBuf);
            byteBuf.release();
            packets.add(packet);
        }

        return packets;
    }

    @Test
    public void testEncoderNoCompress() throws Exception {
        Conf.MAX_PACKET_BYTES = 20;
        // each Packet append 8 bytes field
        PacketsEncoder encoder = new PacketsEncoder();

        List<Packet> packets = createPacket(1, 8);
        List<PacketsEncoder.Result> results = encoder.encodeV0(packets);
        assertEquals(results.size(), 1);
        results.get(0).getBuf().release();

        packets = createPacket(2, 1);
        results = encoder.encodeV0(packets);
        assertEquals(results.size(), 1);
        results.get(0).getBuf().release();

        packets = createPacket(2, 12);
        results = encoder.encodeV0(packets);
        assertEquals(results.size(), 2);
        results.get(0).getBuf().release();
    }

}
