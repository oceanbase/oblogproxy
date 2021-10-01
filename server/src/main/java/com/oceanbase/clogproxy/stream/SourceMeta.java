package com.oceanbase.clogproxy.stream;

import com.oceanbase.clogproxy.common.packet.LogType;
import io.netty.channel.Channel;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

import java.util.Date;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-30
 */
public class SourceMeta {

    private LogType type;
    private String id; // sourceChannelId
    private ClientId clientId = new ClientId();
    private String processId;
    private String version;
    /**
     * last checkd alive time
     */
    private Date lastTime;

    public SourceMeta(LogType type) {
        this.type = type;
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
    }

    public LogType getType() {
        return type;
    }

    public String getId() {
        return id;
    }

    public void setId(Channel channel) {
        this.id = SourceChannelId.of(channel);
    }

    public ClientId getClientId() {
        return clientId;
    }

    public void setClientId(String clientId) {
        this.clientId.set(clientId);
    }

    public String getPid() {
        return processId;
    }

    public void setProcessId(String processId) {
        this.processId = processId;
    }

    public String getVersion() {
        return version;
    }

    public void setVersion(String version) {
        this.version = version;
    }

    public Date getLastTime() {
        return lastTime;
    }

    public void setLastTime(Date lastTime) {
        this.lastTime = lastTime;
    }
}
