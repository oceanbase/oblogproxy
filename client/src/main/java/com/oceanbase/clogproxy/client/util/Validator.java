package com.oceanbase.clogproxy.client.util;

import java.util.Map;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-29 Time: 14:18</p>
 *
 * @author jiyong.jy
 */
public class Validator {

    private static final int MINIMAL_VALID_PORT = 1;
    private static final int MAXIMAL_VALID_PORT = 65535;

    public static <T> T notNull(T obj, String message) {
        if (obj == null) {
            throw new NullPointerException(message);
        }
        return obj;
    }

    public static int validatePort(int port, String message) {
        if (port < MINIMAL_VALID_PORT || port >= MAXIMAL_VALID_PORT) {
            throw new IllegalArgumentException(message);
        }
        return port;
    }

    public static String notEmpty(String val, String message) {
        if (val == null || val.isEmpty()) {
            throw new IllegalArgumentException(message);
        }
        return val;
    }

    public static Map<String, String> notEmpty(Map<String, String> map, String message) {
        if (map == null || map.isEmpty()) {
            throw new IllegalArgumentException(message);
        }
        return map;
    }
}
