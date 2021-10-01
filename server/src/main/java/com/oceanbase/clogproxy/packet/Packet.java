package com.oceanbase.clogproxy.packet;

import com.oceanbase.oms.store.client.message.DataMessage;
import com.oceanbase.oms.store.client.message.drcmessage.DrcNETBinaryRecord;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufUtil;
import net.jpountz.lz4.LZ4Factory;
import org.apache.commons.codec.digest.DigestUtils;
import org.apache.commons.lang3.Conversion;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 14:33</p>
 *
 * @author jiyong.jy
 */
public class Packet {

    private static Logger logger = LoggerFactory.getLogger(Packet.class);

    /**
     * bytes length of buff
     */
    private int length;

    private long timestamp;

    private long safeTimestamp;

    /**
     * netty buff which ownship transfered to here
     */
    private int recordsLength;
    private ByteBuf records;

    /**
     * <pre>
     * Compressed
     *      --------------------------
     *      | HEADER                 |
     *      --------------------------
     *      | [4]length              |    packet length after(execlude) itself
     *      --------------------------
     *      | [8]packet index        |
     *      --------------------------
     *      | [4]timestamp           |
     *      --------------------------
     *      | [4]Checkpoint          |
     *      --------------------------
     *      | [1]Compress Type       |   ---
     *      --------------------------     |
     *      | [4]Original Length     |     |
     *      --------------------------     | -->  We put all this of ByteBuf slice without interpret into Packet
     *      | [4]Compress Length     |     |      that only transfer to Clients.
     *      --------------------------     |
     *      | drcmessage bytes:      |     |
     *      |  [4]idx1               |     |
     *      |  [4]len1               |     |
     *      |  [len1]record1         |     |
     *      |  [4]idx2               |     |
     *      |  [4]len2               |     |
     *      |  [len2]record2         |     |
     *      |  ...                   |     |
     *      |  [4]idxN               |     |
     *      |  [4]lenN               |     |
     *      |  [lenN]recordN         |     |
     *      --------------------------   ---
     * </pre>
     */
    public static Packet decode(int length, ByteBuf inbuff) {
        Packet packet = new Packet();
        packet.length = length;

        packet.timestamp = inbuff.readLong();
        packet.safeTimestamp = inbuff.readLong();
        packet.recordsLength = length - 16;

        // transfer ownship
        packet.records = inbuff.retainedSlice(inbuff.readerIndex(), packet.recordsLength);
        inbuff.readerIndex(inbuff.readerIndex() + packet.recordsLength);
        return packet;
    }

    public static int minLength() {
        return (8 + 8 + 1 + 4 + 4);
    }

    public void release() {
        if (records != null && records.refCnt() > 0) {
            records.release();
        }
    }

    /**
     * very HEAVY method that should be invoked only when test
     */
    public String debug() {
        ByteBuf buf = null;
        try {
            buf = records.copy();
            int len = recordsLength;
            byte[] bytes = new byte[100];
            StringBuilder lineSb = new StringBuilder();

            while (len > 4) {
                byte compressCode = buf.readByte();
                int realRecoredLength = buf.readInt();
                if (realRecoredLength > bytes.length) {
                    bytes = new byte[realRecoredLength];
                }
                int compressedLength = buf.readInt();
                byte[] compressed = new byte[compressedLength];
                buf.readBytes(compressed, 0, compressedLength);

                LZ4Factory.fastestInstance().fastDecompressor().decompress(compressed, 0, bytes, 0, realRecoredLength);
                len -= (1 + 4 + 4 + compressedLength);
                logger.info("packet compressed MD5: ({}){}, uncompressed: ({}){}", compressed.length,
                    DigestUtils.md5Hex(compressed), realRecoredLength, DigestUtils.md5Hex(bytes));

//                logger.info("hexdump: \n{}", HexDump.format(bytes));

                int offset = 0;
                while (offset < realRecoredLength) {
                    int index = Conversion.byteArrayToInt(bytes, offset, 0, 0, 4);
                    index = ByteBufUtil.swapInt(index);
                    offset += 4;
                    int recordLength = Conversion.byteArrayToInt(bytes, offset, 0, 0, 4);
                    recordLength = ByteBufUtil.swapInt(recordLength);
                    offset += 4;

                    DrcNETBinaryRecord record = new DrcNETBinaryRecord(false);
                    byte[] recordbuf = new byte[recordLength];
                    System.arraycopy(bytes, offset, recordbuf, 0, recordLength);
                    record.parse(recordbuf);

                    String tm = record.getSafeTimestamp();
                    if (tm == null) {
                        tm = record.getTimestamp();
                    }
                    Long delay = (System.currentTimeMillis() / 1000) - Long.parseLong(tm);

                    StringBuilder fieldSb = new StringBuilder();
                    List<DataMessage.Record.Field> fields = record.getFieldList();
                    if (fields != null) {
                        for (DataMessage.Record.Field field : fields) {
                            fieldSb.append(field.toString());
                        }
                    }
                    lineSb.append(String.format("[delay:%d][%d][%s.%s(%s):%s:%s]", delay, index, record.getOpt(),
                        record.getDbname(), record.getTablename(), record.getTimestamp(), fieldSb.toString())).append('\n');

                    offset += recordLength;
                }
            }
            return lineSb.toString();

        } catch (Exception e) {
            return "!!!Packet Debug Error: " + e.getMessage();
        } finally {
            if (buf != null) {
                buf.release();
            }
        }
    }

    /**
     * very HEAVY method that should be invoked only when test
     */
    public int recordCount() {
        ByteBuf buf = null;
        try {
            buf = records.copy();
            int len = recordsLength;
            byte[] bytes = new byte[100];

            int count = 0;
            while (len > 4) {
                byte compressCode = buf.readByte();
                int realRecoredLength = buf.readInt();
                if (realRecoredLength > bytes.length) {
                    bytes = new byte[realRecoredLength];
                }
                int compressedLength = buf.readInt();
                byte[] compressed = new byte[compressedLength];
                buf.readBytes(compressed, 0, compressedLength);

                LZ4Factory.fastestInstance().fastDecompressor().decompress(compressed, 0, bytes, 0, realRecoredLength);
                len -= (1 + 4 + 4 + compressedLength);
//                logger.info("packet compressed MD5: ({}){}, uncompressed: ({}){}", compressed.length,
//                        DigestUtils.md5Hex(compressed), realRecoredLength, DigestUtils.md5Hex(bytes));

                int offset = 0;
                while (offset < realRecoredLength) {
                    int recordLength = Conversion.byteArrayToInt(bytes, offset + 4, 0, 0, 4);
                    recordLength = ByteBufUtil.swapInt(recordLength);
                    offset += (8 + recordLength);

                    ++count;
                }
            }
            return count;

        } catch (Exception e) {
            return -1;
        } finally {
            if (buf != null) {
                buf.release();
            }
        }
    }

    public int getLength() {
        return length;
    }

    public long getTimestamp() {
        return timestamp;
    }

    public long getSafeTimestamp() {
        return safeTimestamp;
    }

    public int getRecordsLength() {
        return recordsLength;
    }

    public ByteBuf getRecords() {
        return records;
    }
}
