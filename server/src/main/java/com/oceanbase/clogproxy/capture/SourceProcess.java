package com.oceanbase.clogproxy.capture;

import com.oceanbase.clogproxy.stream.SourceMeta;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-16
 */
public class SourceProcess {
    String pid;
    String path;
    long startTime;

    public static SourceProcess of(SourceMeta meta) {
        SourceProcess source = new SourceProcess();
        source.pid = meta.getPid();
        return source;
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
    }

    public String getPid() {
        return pid;
    }

    public String getPath() {
        return path;
    }

    public long getStartTime() {
        return startTime;
    }
}
