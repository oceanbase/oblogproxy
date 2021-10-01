package com.oceanbase.clogproxy.packet;

import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.stream.ClientId;
import com.oceanbase.clogproxy.stream.SinkChannelId;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.channel.Channel;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

import java.util.Date;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public class ClientMeta {
    private LogType type;
    private String id;  // SinkChannelId
    private String ip;
    private ClientId clientId = new ClientId();
    private String version;
    private String configuration;
    private Date registerTime;
    private Boolean enableMonitor = false;

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE).replace(Conf.OB_SYS_PASSWORD, "******");
    }

    public String getId() {
        return id;
    }

    public void setId(Channel channel) {
        this.id = SinkChannelId.of(channel);
    }

    public LogType getType() {
        return type;
    }

    public void setType(LogType type) {
        this.type = type;
    }

    public String getIp() {
        return ip;
    }

    public void setIp(String ip) {
        this.ip = ip;
    }

    public ClientId getClientId() {
        return clientId;
    }

    public void setClientId(String clientId) {
        this.clientId.set(clientId);
    }

    public String getVersion() {
        return version;
    }

    public void setVersion(String version) {
        this.version = version;
    }

    public String getConfiguration() {
        return configuration;
    }

    public void setConfiguration(String configuration) {
        this.configuration = configuration;
    }

    public Date getRegisterTime() {
        return registerTime;
    }

    public void setRegisterTime(Date registerTime) {
        this.registerTime = registerTime;
    }

    public Boolean getEnableMonitor() {
        return enableMonitor;
    }

    public void setEnableMonitor(Boolean enableMonitor) {
        this.enableMonitor = enableMonitor;
    }
}
