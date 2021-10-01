package com.oceanbase.clogproxy.client;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-06
 */
public enum ErrorCode {
    ////////// 0~499: process error ////////////
    /**
     * general error
     */
    NONE(0),

    /**
     * inner error
     */
    E_INNER(1),

    /**
     * failed to connect
     */
    E_CONNECT(2),

    /**
     * exceed max retry connect count
     */
    E_MAX_RECONNECT(3),

    /**
     * user callback throws exception
     */
    E_USER(4),

    ////////// 500~: recv data error ////////////
    /**
     * unknown data protocol
     */
    E_PROTOCOL(500),

    /**
     * unknown header type
     */
    E_HEADER_TYPE(501),

    /**
     * failed to auth
     */
    NO_AUTH(502),

    /**
     * unknown compress type
     */
    E_COMPRESS_TYPE(503),

    /**
     * length not match
     */
    E_LEN(504),

    /**
     * failed to parse data
     */
    E_PARSE(505);

    int code;

    ErrorCode(int code) {
        this.code = code;
    }
}
