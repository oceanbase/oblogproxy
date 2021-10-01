package com.oceanbase.clogproxy.obaccess.handshake;

import org.apache.commons.lang3.Conversion;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-08-17
 * <p>
 * including OK, ERROR, EOF packet decode
 */
public class OBMysqlOkPacket extends BaseOBMysqlPacket {
    private static byte MYSQL_OK_PACKET_TYPE = 0x00;
    private static byte MYSQL_ERR_PACKET_TYPE = (byte) 0xFF;
    // the EOF packet may appear in places where a Protocol::LengthEncodedInteger
    // may appear. You must check whether the packet length is less than 9 to
    // make sure that it is a EOF packet.
    private static byte MYSQL_EOF_PACKET_TYPE = (byte) 0xFE;

    private Logger logger = LoggerFactory.getLogger(OBMysqlOkPacket.class);

    // protocol defined:
    // https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
    @Override
    public boolean decode(byte[] buf) throws IOException {
        int index = 0;
        byte pktType = buf[index];
        index += 1;

        int pktLength = buf.length + 4; // including header outter.

        if (pktType == MYSQL_OK_PACKET_TYPE && pktLength > 7) {
            return true;
        }

        if (pktType == MYSQL_ERR_PACKET_TYPE) {
            short errcode = Conversion.byteArrayToShort(buf, index, (short) 0, 0, 2);
            index += 2;
            index += 1; // skip SQL State Marker '#'

            String sqlstate = new String(buf, index, 5);
            index += 5;

            String message = "";
            if (index < buf.length) {
                message = new String(buf, index, buf.length - index);
            }
            logger.error("failed to auth mysql: {}, {}, {}", errcode, sqlstate, message);
            return false;
        }

        if (pktType == MYSQL_EOF_PACKET_TYPE) {
            if (pktLength < 9) {
                return true;
            }
            logger.error("failed to auth mysql due to EOF packet");
            return false;
        }

        logger.error("failed to auth mysql due to unknown response pkt type: {}", pktType);
        return false;
    }
}
