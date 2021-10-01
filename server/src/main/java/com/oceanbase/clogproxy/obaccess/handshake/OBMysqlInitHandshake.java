package com.oceanbase.clogproxy.obaccess.handshake;

import org.apache.commons.lang3.Conversion;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-08-17
 */
public class OBMysqlInitHandshake extends BaseOBMysqlPacket {
    private Logger logger = LoggerFactory.getLogger(OBMysqlInitHandshake.class);

    private byte protocolVersion;
    private String serverVersion;
    private int threadId;
    private byte filler;
    private byte serverLanguage;
    private short serverStatus;
    private byte authPluginDataLen;
    private byte[] reservered; // 10 bytes

    private String authPluginName;

    private byte[] serverCapabilitiesBytes = new byte[4];
    private byte[] scrambleBuff = new byte[20];

    // protocol defined:
    // https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::HandshakeV10
    @Override
    public boolean decode(byte[] buf) throws IOException {
        int index = 0;
        protocolVersion = buf[index];
        index += 1;

        int q = index;
        while (buf[q] != '\0') {
            ++q;
        }
        serverVersion = new String(buf, index, q - index);
        index = q + 1;

        threadId = Conversion.byteArrayToInt(buf, index, 0, 0, 4);
        index += 4;

        System.arraycopy(buf, index, scrambleBuff, 0, 8);
        index += 8;

        filler = buf[index];
        index += 1;

        // Lower 2 bytes of serverCapabilities
        System.arraycopy(buf, index, serverCapabilitiesBytes, 0, 2);
        index += 2;

        if (index >= buf.length) {
            return true;
        }

        serverLanguage = buf[index];
        index += 1;

        serverStatus = Conversion.byteArrayToShort(buf, index, (short) 0, 0, 2);
        index += 2;

        // Upper 2 bytes of serverCapabilities
        System.arraycopy(buf, index, serverCapabilitiesBytes, 2, 2);
        index += 2;

        int capabilities = Conversion.byteArrayToInt(serverCapabilitiesBytes, 0, 0, 0, 4);

        authPluginDataLen = buf[index];
        index += 1;

        // skip reserved
        index += 10;

        if ((capabilities & OBMysqlHandshakeResp.CapabilityFlag.CLIENT_SECURE_CONNECTION.ordinal()) == 0) {
            logger.error("failed to decode mysql initial handshake packet, server unsupport CLIENT_SECURE_CONNECTION");
            return false;
        }
        int tmpLen = Math.max(13, authPluginDataLen - 8);
        if (tmpLen != 13) {
            logger.error("failed to decode mysql initial handshake packet, auth plugin data2 len must be 13");
            return false;
        }
        System.arraycopy(buf, index, scrambleBuff, 8, 12); // 13 - 1
        index += 13;

//        if ((capabilities & HandshakeResponse.CapabilityFlag.CLIENT_PLUGIN_AUTH.ordinal()) != 0) {
//            if (index < buf.length - 1) {
//                q = index;
//                while (buf[q] != '\0') {
//                    ++q;
//                }
//                authPluginName = new String(buf, index, q - index);
//            }
//        }
        return true;
    }


    public byte[] getScrambleBuff() {
        return scrambleBuff;
    }
}
