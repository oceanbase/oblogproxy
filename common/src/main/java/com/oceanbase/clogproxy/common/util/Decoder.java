package com.oceanbase.clogproxy.common.util;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.PooledByteBufAllocator;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 21:54</p>
 *
 * @author jiyong.jy
 */
public class Decoder {

    public static String decodeStringInt(ByteBuf buffer) {
        if (buffer.readableBytes() < Integer.BYTES) {
            return null;
        }
        buffer.markReaderIndex();
        int length = buffer.readInt();
        if (buffer.readableBytes() < length) {
            buffer.resetReaderIndex();
            return null;
        }
        byte[] bytes = new byte[length];
        buffer.readBytes(bytes);
        String str = new String(bytes);
        if (str.isEmpty()) {
            throw new RuntimeException("decode string is null or empty");
        }
        return str;
    }

    public static String decodeStringByte(ByteBuf buffer) {
        if (buffer.readableBytes() < Byte.BYTES) {
            return null;
        }
        buffer.markReaderIndex();
        short length = buffer.readByte();
        if (buffer.readableBytes() < length) {
            buffer.resetReaderIndex();
            return null;
        }
        byte[] bytes = new byte[length];
        buffer.readBytes(bytes);
        String str = new String(bytes);
        if (str.isEmpty()) {
            throw new RuntimeException("decode string is null or empty");
        }
        return str;
    }

    public static ByteBuf encodeStringInt(String string) {
        if (string == null || string.length() == 0) {
            throw new RuntimeException("encode string is null or empty");
        }
        ByteBuf byteBuf = PooledByteBufAllocator.DEFAULT.buffer(4 + string.length());
        byteBuf.writeInt(string.length());
        byteBuf.writeBytes(string.getBytes());
        return byteBuf;
    }
}
