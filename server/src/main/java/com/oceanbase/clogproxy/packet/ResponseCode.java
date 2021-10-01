package com.oceanbase.clogproxy.packet;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public enum ResponseCode {
    SUCC(0),
    ERR_PACKET(1),
    ERR_CONF(2),
    NO_AUTH(3),
    ERR_INIT(4);

    int code;

    ResponseCode(int code) {
        this.code = code;
    }

    public int code() {
        return code;
    }
}
