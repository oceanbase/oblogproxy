package com.oceanbase.clogproxy.common.packet;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public enum CompressType {
    /**
     * no compress
     */
    NONE(0),

    /**
     * lz4 compress
     */
    LZ4(1);

    private int code;

    CompressType(int code) {
        this.code = code;
    }

    public static CompressType codeOf(int code) {
        for (CompressType v : values()) {
            if (v.code == code) {
                return v;
            }
        }
        return null;
    }

    public int code() {
        return code;
    }
}
