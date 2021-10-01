package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.common.util.CryptoUtil;
import com.oceanbase.clogproxy.common.util.CryptoUtil.Encryptor;
import com.oceanbase.clogproxy.common.util.Hex;
import org.junit.Assert;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-14
 */
public class CryptoUtilTest {
    private static Logger logger = LoggerFactory.getLogger(CryptoUtilTest.class);

    private String text = "the quick brown fox jumps over the lazy dog";
    private String cipherKey = "testkey*";

    @Test
    public void testEncrypt() {
        logger.info("original text: {}", text);

        Encryptor encryptor = CryptoUtil.newEncryptor(cipherKey);
        byte[] encrypts = encryptor.encrypt(text);
        if (encrypts == null) {
            throw new RuntimeException("");
        }
        String hexEncrypts = Hex.str(encrypts);
        logger.info("encrpts hex: {}", hexEncrypts);

        Encryptor decryptor = CryptoUtil.newEncryptor(cipherKey);
        byte[] decryptBytes = Hex.toBytes(hexEncrypts);
        if (decryptBytes == null) {
            throw new RuntimeException("failed to convert cipher string to bytes");
        }
        Assert.assertArrayEquals(encrypts, decryptBytes);

        String decrypt = decryptor.decrypt(decryptBytes);
        logger.info("decrypt text: {}", text);
        Assert.assertEquals(decrypt, text);

        Encryptor errorDecryptor = CryptoUtil.newEncryptor("aabbcc");
        decryptBytes = Hex.toBytes(hexEncrypts);
        if (decryptBytes == null) {
            throw new RuntimeException("failed to convert cipher string to bytes");
        }
        decrypt = errorDecryptor.decrypt(decryptBytes);
        Assert.assertNotEquals(decrypt, text);
    }

    @Test
    public void testSha1() {
        byte[] sha1 = CryptoUtil.sha1("123456");
        Assert.assertEquals(sha1.length, 20);

        Assert.assertEquals(Hex.str(sha1), "7C4A8D09CA3762AF61E59520943DC26494F8941B");
    }
}
