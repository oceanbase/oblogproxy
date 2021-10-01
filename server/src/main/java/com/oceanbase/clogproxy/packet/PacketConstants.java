package com.oceanbase.clogproxy.packet;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-16
 */
public class PacketConstants {

    public static final byte[] MAGIC_STRING = new byte[]{'x', 'i', '5', '3', 'g', ']', 'q'};

    /**
     * 2 - protocol version
     * 4 - header code
     */
    public static final int PACKET_HEADER_LEN = 6;


    /**
     * 2 - protocol version
     * 4 - Header Code
     * 4 - Response Code
     */
    public static final int RESPONSE_HEADER_LEN = 10;
}
