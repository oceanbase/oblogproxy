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
public class LogReaderHandshakeResp {
    private int code;
    private String version;

    public void setCode(int code) {
        this.code = code;
    }

    public void setVersion(String version) {
        this.version = version;
    }

    public int getCode() {
        return code;
    }

    public String getVersion() {
        return version;
    }

    public ByteBuf serialize(HeaderType type) {
        ByteBuf byteBuf = PooledByteBufAllocator.DEFAULT.buffer(PacketConstants.RESPONSE_HEADER_LEN + 4 + version.length());
        byteBuf.writeShort(ProtocolVersion.V0.code());
        byteBuf.writeInt(type.code());
        byteBuf.writeInt(code);
        byteBuf.writeInt(version.length());
        byteBuf.writeBytes(version.getBytes());
        return byteBuf;
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
    }
}
