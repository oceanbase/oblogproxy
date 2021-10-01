package com.oceanbase.clogproxy.client.config;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 09:30</p>
 *
 * @author jiyong.jy
 */
public interface ConnectionConfig {
    String generateConfigurationString();

    void updateCheckpoint(String checkpoint);

    String toString();
}
