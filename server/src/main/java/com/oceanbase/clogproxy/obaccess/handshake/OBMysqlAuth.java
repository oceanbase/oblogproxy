package com.oceanbase.clogproxy.obaccess.handshake;

import com.oceanbase.clogproxy.common.util.CryptoUtil;
import com.oceanbase.clogproxy.common.util.Hex;
import com.oceanbase.clogproxy.util.Conf;
import org.apache.commons.lang3.Conversion;
import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.net.Socket;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-08-16
 * <p>
 * MySQL validation logic:
 * Server stores the 'stage2' hash, which is cleartext password calc-ed sha1 twice.
 * When login, server sends random scramble str.
 * Client gets the scramble, use this logic to generate login rsp:
 * stage1 = sha1(passwd) done by client,
 * stage2 = sha1(stage1_hex)
 * scrambled_stage2 = sha1(scramble_got_from_server, stage2_hex)
 * login_rsp = xor(stage1_hex, scrambled_stage2)
 * <p>
 * stage1_hex's size is 20B, contain hex num(20B)
 * it is unreadable for user
 */
public class OBMysqlAuth {
    private static Logger logger = LoggerFactory.getLogger(OBMysqlAuth.class);

    private String host;
    private int port;
    private String username;
    private byte[] passwordStage1;
    private String database;

    public OBMysqlAuth(String host, int port, String username, byte[] passwordStage1, String database) {
        this.host = host;
        this.port = port;
        this.username = username;
        this.passwordStage1 = passwordStage1;
        this.database = database;
    }

    public boolean auth() {
        Socket socket = null;

        try {
            socket = new Socket(host, port);

            BufferedInputStream is = new BufferedInputStream(socket.getInputStream());
            OBMysqlInitHandshake handshake = new OBMysqlInitHandshake();
            if (!handshake.decode(is)) {
                return false;
            }
            byte[] scramble = handshake.getScrambleBuff();
            if (Conf.DEBUG) {
                logger.debug("scramble: [{}]{}", scramble.length, Hex.str(scramble));
            }

            byte[] stage2 = CryptoUtil.sha1(passwordStage1);
            if (Conf.DEBUG) {
                logger.debug("stage2: [{}]{}", stage2.length, Hex.str(stage2));
            }

            byte[] combined = new byte[scramble.length + stage2.length];
            System.arraycopy(scramble, 0, combined, 0, scramble.length);
            System.arraycopy(stage2, 0, combined, scramble.length, stage2.length);

            byte[] scrambledStage2 = CryptoUtil.sha1(combined);
            if (Conf.DEBUG) {
                logger.debug("scrambledStage2: [{}]{}", scrambledStage2.length, Hex.str(scrambledStage2));
            }

            if (passwordStage1.length != scrambledStage2.length) {
                logger.error("failed to auth mysql due to unmatched length of password stage1 and scrambled stage2");
                return false;
            }

            byte[] authResponseBytes = new byte[scrambledStage2.length];
            xor(passwordStage1, scrambledStage2, scrambledStage2.length, authResponseBytes);
            if (Conf.DEBUG) {
                logger.debug("authResponseBytes: [{}]{}", authResponseBytes.length, Hex.str(authResponseBytes));
            }

            if (!responseHandshake(new BufferedOutputStream(socket.getOutputStream()),
                    authResponseBytes, (byte) (handshake.getSeq() + 1))) {
                return false;
            }
            return authResponse(is);

        } catch (IOException e) {
            logger.error("failed to auth mysql: ", e);
            return false;
        } finally {
            if (socket != null) {
                try {
                    socket.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private void xor(byte[] s1, byte[] s2, int len, byte[] to) {
        int i = 0;
        int end = i + len;
        while (i < end) {
            to[i] = (byte) (s1[i] ^ s2[i]);
            ++i;
        }
    }

    private boolean responseHandshake(BufferedOutputStream os, byte[] authResponseBytes, byte seq) throws IOException {
        byte[] buf = new byte[OBMysqlHandshakeResp.RESPONSE_BUFFER_SIZE];
        OBMysqlHandshakeResp response = new OBMysqlHandshakeResp();
        response.setUsername(username);
        response.setAuthResponse(authResponseBytes);
        response.setFlag(OBMysqlHandshakeResp.CapabilityFlag.CLIENT_COMPRESS, false);
        response.setFlag(OBMysqlHandshakeResp.CapabilityFlag.CLIENT_SUPPORT_ORACLE_MODE, true);
        if (StringUtils.isNotEmpty(database)) {
            response.setFlag(OBMysqlHandshakeResp.CapabilityFlag.CLIENT_CONNECT_WITH_DB, true);
            response.setDatabase(database);
        } else {
            response.setFlag(OBMysqlHandshakeResp.CapabilityFlag.CLIENT_CONNECT_WITH_DB, false);
        }
        int realLen = response.serialize(buf, 4);

        Conversion.intToByteArray(realLen, 0, buf, 0, 3);
        buf[3] = seq;

        realLen += 4;
//        logger.info("send handshake response buffer: [{}]{}", realLen, Hex.str(buf));

        os.write(buf, 0, realLen);
        os.flush();
        return true;
    }

    private boolean authResponse(BufferedInputStream is) throws IOException {
        OBMysqlOkPacket okPacket = new OBMysqlOkPacket();
        return okPacket.decode(is);
    }


}
