package com.oceanbase.clogproxy.packet;

import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.PooledByteBufAllocator;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public class ClientHandshakeResponse {
    private int code;
    private String ip;
    private String version;

    public ByteBuf serialize() {
        ByteBuf byteBuf = PooledByteBufAllocator.DEFAULT.buffer(PacketConstants.RESPONSE_HEADER_LEN + 1 + ip.length() + 1 + version.length());
        byteBuf.writeShort(ProtocolVersion.V0.code());
        byteBuf.writeInt(HeaderType.HANDSHAKE_RESPONSE_CLIENT.code());
        byteBuf.writeInt(code);
        byteBuf.writeByte(ip.length());
        byteBuf.writeBytes(ip.getBytes());
        byteBuf.writeByte(version.length());
        byteBuf.writeBytes(version.getBytes());
        return byteBuf;
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SIMPLE_STYLE);
    }

    public int getCode() {
        return code;
    }

    public void setCode(int code) {
        this.code = code;
    }

    public String getIp() {
        return ip;
    }

    public void setIp(String ip) {
        this.ip = ip;
    }

    public String getVersion() {
        return version;
    }

    public void setVersion(String version) {
        this.version = version;
    }
}
