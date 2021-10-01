package com.oceanbase.clogproxy.obaccess.handshake;

import org.apache.commons.lang3.Conversion;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedInputStream;
import java.io.IOException;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-08-17
 */

/*   3bytes   1 byte
 * ------------------
 * |   len  |  seq  |
 * ------------------
 */
public abstract class BaseOBMysqlPacket {
    private Logger logger = LoggerFactory.getLogger(BaseOBMysqlPacket.class);

    public static int HEADER_LEN = 4;
    protected int len;
    protected byte seq;

    public boolean decode(BufferedInputStream is) throws IOException {
        byte[] buf = new byte[HEADER_LEN];
        if (is.read(buf, 0, 4) != 4) {
            logger.error("failed to read mysql packet header");
            return false;
        }
        len = Conversion.byteArrayToInt(buf, 0, 0, 0, 3);
        seq = buf[3];

        buf = new byte[len];
        int index = 0;
        int readlen = 0;
        while (index < len - 1 && (readlen = is.read(buf, index, len - index)) != -1) {
            if (index >= len) {
                break;
            }
            index += readlen;
        }

        if (index < len - 1) {
            logger.error("failed to decode mysql initial handshake packet, too small packet");
            return false;
        }

        return decode(buf);
    }

    public abstract boolean decode(byte[] buf) throws IOException;

    public byte getSeq() {
        return seq;
    }
}
