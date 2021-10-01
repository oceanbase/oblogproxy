package com.oceanbase.clogproxy.client;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-06
 * <p>
 * TODO... error code
 */
public class LogProxyClientException extends RuntimeException {
    private ErrorCode code = ErrorCode.NONE;

    public LogProxyClientException(ErrorCode code, String message) {
        super(message);
        this.code = code;
    }

    public LogProxyClientException(ErrorCode code, Exception exception) {
        super(exception.getMessage(), exception.getCause());
        this.code = code;
    }

    public LogProxyClientException(ErrorCode code, String message, Throwable throwable) {
        super(message, throwable);
        this.code = code;
    }

    public boolean needStop() {
        return (code == ErrorCode.E_MAX_RECONNECT) || (code == ErrorCode.E_PROTOCOL) ||
                (code == ErrorCode.E_HEADER_TYPE) || (code == ErrorCode.NO_AUTH) ||
                (code == ErrorCode.E_COMPRESS_TYPE) || (code == ErrorCode.E_LEN) ||
                (code == ErrorCode.E_PARSE);
    }

    public ErrorCode getCode() {
        return code;
    }
}
