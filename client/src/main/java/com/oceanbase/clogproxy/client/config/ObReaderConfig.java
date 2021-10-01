package com.oceanbase.clogproxy.client.config;

import com.google.common.collect.Maps;
import com.oceanbase.clogproxy.client.util.Validator;
import com.oceanbase.clogproxy.common.config.ShareConf;
import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.common.util.CryptoUtil;
import com.oceanbase.clogproxy.common.util.Hex;

import java.util.Map;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 09:50</p>
 *
 * @author jiyong.jy
 * <p>
 * TODO... merge configs to common
 */
public class ObReaderConfig extends AbstractConnectionConfig {
    private static final ConfigItem<String> CLUSTER_URL = new ConfigItem<>("cluster_url", "");
    private static final ConfigItem<String> CLUSTER_USER = new ConfigItem<>("cluster_user", "");
    private static final ConfigItem<String> CLUSTER_PASSWORD = new ConfigItem<>("cluster_password", "");
    private static final ConfigItem<String> TABLE_WHITE_LIST = new ConfigItem<>("tb_white_list", "");
    private static final ConfigItem<Long> START_TIMESTAMP = new ConfigItem<>("first_start_timestamp", 0L);

    public ObReaderConfig() {
        super(Maps.newHashMap());
    }

    public ObReaderConfig(Map<String, String> allConfigs) {
        super(allConfigs);
    }

    @Override
    public LogType getLogType() {
        return LogType.OCEANBASE;
    }

    @Override
    public boolean valid() {
        try {
            Validator.notEmpty(CLUSTER_URL.val, "invalid clusterUrl");
            Validator.notEmpty(CLUSTER_USER.val, "invalid clusterUser");
            Validator.notEmpty(CLUSTER_PASSWORD.val, "invalid clusterPassword");
            if (START_TIMESTAMP.val.equals(0L)) {
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
            String value = entry.getValue().val.toString();
            if (CLUSTER_PASSWORD.key.equals(entry.getKey()) && ShareConf.AUTH_PASSWORD_HASH) {
                value = Hex.str(CryptoUtil.sha1(value));
            }
            sb.append(entry.getKey()).append("=").append(value).append(" ");
        }

        for (Map.Entry<String, String> entry : extraConfigs.entrySet()) {
            sb.append(entry.getKey()).append("=").append(entry.getValue()).append(" ");
        }
        return sb.toString();
    }

    @Override
    public void updateCheckpoint(String checkpoint) {
        try {
            START_TIMESTAMP.set(Long.parseLong(checkpoint));
        } catch (NumberFormatException e) {
            // do nothing
        }
    }

    @Override
    public String toString() {
        // TODO... refactor this
        return "cluster_url=" + CLUSTER_URL + ", cluster_user=" + CLUSTER_USER + ", cluster_password=******, " +
                "tb_white_list=" + TABLE_WHITE_LIST + ", start_timestamp=" + START_TIMESTAMP;
    }

    /**
     * 设置 集群ID，会自动组装集群连接地址
     */
    public void setInstanceId(String instanceId) {
        CLUSTER_URL.set("http://configserver.alibaba-inc.com/services?Action=ObRootServiceInfo&User_ID=alibaba&UID=OMS&ObRegion=" + instanceId);
    }

    /**
     * 设置自定义集群地址
     */
    public void setInstanceUrl(String clusterUrl) {
        CLUSTER_URL.set(clusterUrl);
    }

    /**
     * 设置连接OB用户名
     */
    public void setUsername(String clusterUser) {
        CLUSTER_USER.set(clusterUser);
    }

    /**
     * 设置连接OB密码
     */
    public void setPassword(String clusterPassword) {
        CLUSTER_PASSWORD.set(clusterPassword);
    }

    /**
     * 配置过滤规则，由租户.库.表3个维度组成，每一段 * 表示任意，如：A.foo.bar，B.foo.*，C.*.*，*.*.*
     */
    public void setTableWhiteList(String tableWhiteList) {
        TABLE_WHITE_LIST.set(tableWhiteList);
    }

    /**
     * 设置起始订阅的 UNIX时间戳，0表示从当前，通常不要早于1小时
     */
    public void setStartTimestamp(Long startTimestamp) {
        START_TIMESTAMP.set(startTimestamp);
    }
}
