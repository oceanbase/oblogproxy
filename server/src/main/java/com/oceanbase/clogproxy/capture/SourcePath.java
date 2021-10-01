package com.oceanbase.clogproxy.capture;

import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-16
 */
public class SourcePath {
    String path;
    /**
     * last modify time of log path
     */
    int lastTime;

    /**
     * ClientId extract from path
     */
    String clientId = "";

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
    }

    public String getPath() {
        return path;
    }

    public int getLastTime() {
        return lastTime;
    }

    public String getClientId() {
        return clientId;
    }
}
