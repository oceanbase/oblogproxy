package com.oceanbase.clogproxy.common.util;

import io.netty.buffer.ByteBufUtil;
import io.netty.buffer.Unpooled;
import org.apache.commons.codec.DecoderException;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-02
 */
public final class Hex {
    public static String dump(byte[] array) {
        return dump(array, 0, array.length);
    }

    public static String dump(byte[] bytes, int offset, int length) {
        return ByteBufUtil.prettyHexDump(Unpooled.wrappedBuffer(bytes, offset, length));
    }

    public static String str(byte[] bytes) {
        return org.apache.commons.codec.binary.Hex.encodeHexString(bytes, false);
    }

    public static byte[] toBytes(String hexstr) {
        try {
            return org.apache.commons.codec.binary.Hex.decodeHex(hexstr);
        } catch (DecoderException e) {
            return null;
        }
    }
}