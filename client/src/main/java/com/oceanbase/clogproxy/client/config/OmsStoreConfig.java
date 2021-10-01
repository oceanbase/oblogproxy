package com.oceanbase.clogproxy.client.config;

import com.oceanbase.clogproxy.client.util.Validator;
import com.oceanbase.clogproxy.common.packet.LogType;

import java.util.Map;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 09:49</p>
 *
 * @author jiyong.jy
 */
public class OmsStoreConfig extends AbstractConnectionConfig {

    private static final ConfigItem<String> MANAGER_HOST = new ConfigItem<>("manager.host", "");
    private static final ConfigItem<String> SUB_TOPIC = new ConfigItem<>("subtopic", "");
    private static final ConfigItem<Long> TIMESTAMP = new ConfigItem<>("timestamp", 0L);

    public OmsStoreConfig(Map<String, String> configMap) {
        super(configMap);
    }

    @Override
    public LogType getLogType() {
        return LogType.OMS_STORE;
    }

    @Override
    public boolean valid() {
        try {
            Validator.notEmpty(MANAGER_HOST.val, "invalid managerHost");
            Validator.notEmpty(SUB_TOPIC.val, "invalid subTopic");
            if (TIMESTAMP.val.equals(0L)) {
                throw new IllegalArgumentException("invalid startTimestamp");
            }
            return true;
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    @Override
    public String generateConfigurationString() {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, ConfigItem<Object>> entry : configs.entrySet()) {
            sb.append(entry.getKey()).append("=").append(entry.getValue().val.toString()).append(" ");
        }

        for (Map.Entry<String, String> entry : extraConfigs.entrySet()) {
            sb.append(entry.getKey()).append("=").append(entry.getValue()).append(" ");
        }
        return sb.toString();
    }

    @Override
    public void updateCheckpoint(String checkpoint) {
        try {
            TIMESTAMP.set(Long.parseLong(checkpoint));
        } catch (NumberFormatException e) {
            // do nothing
        }
    }

    @Override
    public String toString() {
        return "manager_host=" + MANAGER_HOST + ", sub_topic=" + SUB_TOPIC + ", timestamp=" + TIMESTAMP;
    }
}
