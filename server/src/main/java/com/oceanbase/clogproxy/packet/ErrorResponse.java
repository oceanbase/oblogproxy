package com.oceanbase.clogproxy.packet;

import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.PooledByteBufAllocator;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public class ErrorResponse {
    private int code;
    private String message;

    public ErrorResponse(int code, String message) {
        this.code = code;
        this.message = message;
    }

    public ByteBuf serialize() {
        ByteBuf byteBuf = PooledByteBufAllocator.DEFAULT.buffer(PacketConstants.RESPONSE_HEADER_LEN + 4 + Conf.VERSION.length());
        byteBuf.writeShort(ProtocolVersion.V0.code());
        byteBuf.writeInt(HeaderType.ERROR_RESPONSE.code());
        byteBuf.writeInt(code);
        byteBuf.writeInt(message.length());
        byteBuf.writeBytes(message.getBytes());
        return byteBuf;
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SIMPLE_STYLE);
    }

    public int getCode() {
        return code;
    }

    public String getMessage() {
        return message;
    }
}
