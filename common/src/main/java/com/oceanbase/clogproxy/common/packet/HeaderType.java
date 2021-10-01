package com.oceanbase.clogproxy.common.packet;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-15
 */
public enum HeaderType {
    /**
     * error response
     */
    ERROR_RESPONSE(-1),

    /**
     * client request handshake
     */
    HANDSHAKE_REQUEST_CLIENT(1),

    /**
     * response to client handshake
     */
    HANDSHAKE_RESPONSE_CLIENT(2),

    /**
     * logreader request handshake
     */
    HANDSHAKE_REQUEST_LOGREADER(3),

    /**
     * logreader response handshake
     */
    HANDSHAKE_RESPONSE_LOGREADER(4),

    /**
     * logreader data stream
     */
    DATA_LOGREADER(5),

    /**
     * client data stream
     */
    DATA_CLIENT(6),

    /**
     * status info of server runtime
     */
    STATUS(7),

    /**
     * status info of LogReader
     */
    STATUS_LOGREADER(8),
    ;

    private final int code;

    HeaderType(int code) {
        this.code = code;
    }

    public static HeaderType codeOf(int code) {
        for (HeaderType t : values()) {
            if (t.code == code) {
                return t;
            }
        }
        return null;
    }

    public int code() {
        return code;
    }
}
