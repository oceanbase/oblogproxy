package com.oceanbase.clogproxy.client.config;

import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.common.util.TypeTrait;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 09:29</p>
 *
 * @author jiyong.jy
 */
public abstract class AbstractConnectionConfig implements ConnectionConfig {

    /**
     * defined sturcture configurations
     */
    protected static Map<String, ConfigItem<Object>> configs = new HashMap<>();

    /**
     * extra configurations provided by liboblog
     */
    protected final Map<String, String> extraConfigs = new HashMap<>();

    @SuppressWarnings("unchecked")
    protected static class ConfigItem<T> {
        protected String key;
        protected T val;

        public ConfigItem(String key, T val) {
            this.key = key;
            this.val = val;
            configs.put(key, (ConfigItem<Object>) this);
        }

        public void set(T val) {
            this.val = val;
        }

        public void fromString(String val) {
            this.val = TypeTrait.fromString(val, this.val.getClass());
        }

        @Override
        public String toString() {
            return val.toString();
        }
    }

    public AbstractConnectionConfig(Map<String, String> allConfigs) {
        if (allConfigs != null) {
            for (Entry<String, String> entry : allConfigs.entrySet()) {
                if (!configs.containsKey(entry.getKey())) {
                    extraConfigs.put(entry.getKey(), entry.getValue());
                } else {
                    set(entry.getKey(), entry.getValue());
                }
            }
        }
//        logger.info("structure config: {}", configs);
//        logger.info("extra configs: {}", extraConfigs);
    }

    public abstract LogType getLogType();

    public void setExtraConfigs(Map<String, String> extraConfigs) {
        this.extraConfigs.putAll(extraConfigs);
    }

    void set(String key, String val) {
        ConfigItem<Object> cs = configs.get(key);
        if (cs != null) {
            cs.fromString(val);
        }
    }

    /**
     * validate if defined configurations
     *
     * @return True or False
     */
    public abstract boolean valid();
}
