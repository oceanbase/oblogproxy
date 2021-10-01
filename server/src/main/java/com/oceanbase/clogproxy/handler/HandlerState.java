package com.oceanbase.clogproxy.handler;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-17
 */
public enum HandlerState {
    MAGIC_NUMBER,
    PROTOCOL_VERSION,
    HEADER_CODE,
    PROTOBUF_PREFIX,
    PROTOBUF,
    PARSE
}
