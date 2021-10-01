package com.oceanbase.clogproxy.common.packet;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public enum ProtocolVersion {
    /**
     * v0 version
     */
    V0(0),
    V1(1);

    private final int code;

    ProtocolVersion(int code) {
        this.code = code;
    }

    public static ProtocolVersion codeOf(int code) {
        for (ProtocolVersion v : values()) {
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
