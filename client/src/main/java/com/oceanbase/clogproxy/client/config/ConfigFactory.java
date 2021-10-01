package com.oceanbase.clogproxy.client.config;

import com.oceanbase.clogproxy.common.packet.LogType;

import java.util.Map;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-07 Time: 11:24</p>
 *
 * @author jiyong.jy
 */
public class ConfigFactory {

    public static ConnectionConfig getConfig(LogType logType, Map<String, String> configMap) {
        switch (logType) {
            case OCEANBASE:
                return new ObReaderConfig(configMap);
            case OMS_STORE:
                return new OmsStoreConfig(configMap);
            default:
                throw new IllegalArgumentException("unknown log type " + logType);
        }
    }
}
