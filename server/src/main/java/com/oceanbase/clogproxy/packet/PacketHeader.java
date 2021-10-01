package com.oceanbase.clogproxy.packet;

import org.apache.commons.lang3.builder.ReflectionToStringBuilder;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public class PacketHeader {
    private String magicNumber;
    private int protocolVersion;
    private int code;

    public String getMagicNumber() {
        return magicNumber;
    }

    public void setMagicNumber(String magicNumber) {
        this.magicNumber = magicNumber;
    }

    public int getProtocolVersion() {
        return protocolVersion;
    }

    public void setProtocolVersion(int protocolVersion) {
        this.protocolVersion = protocolVersion;
    }

    public int getCode() {
        return code;
    }

    public void setCode(int code) {
        this.code = code;
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this);
    }
}
