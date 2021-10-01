package com.oceanbase.clogproxy.common.packet;

import java.util.HashMap;
import java.util.Map;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-07 Time: 11:14</p>
 *
 * @author jiyong.jy
 */
public enum LogType {

    /**
     * LogProxy OceanBase LogReader
     */
    OCEANBASE(0),

    /**
     * DRC store
     */
    OMS_STORE(1);

    private final int code;

    private static final Map<Integer, LogType> CODE_TYPES = new HashMap<>(values().length);

    static {
        for (LogType logCaptureType : values()) {
            CODE_TYPES.put(logCaptureType.code, logCaptureType);
        }
    }

    LogType(int code) {
        this.code = code;
    }

    public int getCode() {
        return this.code;
    }

    public static LogType fromString(String string) {
        if (string == null) {
            throw new NullPointerException("logTypeString is null");
        }
        return valueOf(string.toUpperCase());
    }

    public static LogType fromCode(int code) {
        if (CODE_TYPES.containsKey(code)) {
            return CODE_TYPES.get(code);
        }
        return null;
    }
}
