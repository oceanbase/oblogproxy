package com.oceanbase.clogproxy.common.util;

import org.apache.commons.codec.digest.DigestUtils;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-14
 */
public class CryptoUtil {

    private static final String KEY = "LogProxy123*";

    private static final int AES_KEY_SIZE = 256;
    private static final int GCM_TAG_LENGTH = 16;

    public static Encryptor newEncryptor(String key) {
        return new Encryptor(key);
    }

    public static Encryptor newEncryptor() {
        return new Encryptor(KEY);
    }

    public static class Encryptor {
        private Cipher cipher = null;   // not thread-safe
        private byte[] key = new byte[AES_KEY_SIZE / 16];
        private byte[] iv = new byte[12];

        private Encryptor(String cipherKey) {
            try {
                cipher = Cipher.getInstance("AES/GCM/NoPadding");
                byte[] cipherBytes = cipherKey.getBytes();
                System.arraycopy(cipherBytes, 0, key, 0, Math.min(key.length, cipherBytes.length));
                System.arraycopy(cipherBytes, 0, iv, 0, Math.min(iv.length, cipherBytes.length));

            } catch (NoSuchAlgorithmException | NoSuchPaddingException e) {
                System.out.println("failed to init AES key generator, exit!!! : " + e);
                System.exit(-1);
            }
        }

        public byte[] encrypt(String text) {
            SecretKeySpec keySpec = new SecretKeySpec(key, "AES");
            GCMParameterSpec gcmParameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH * 8, iv);

            try {
                cipher.init(Cipher.ENCRYPT_MODE, keySpec, gcmParameterSpec);
                return cipher.doFinal(text.getBytes());
            } catch (InvalidKeyException | InvalidAlgorithmParameterException | IllegalBlockSizeException | BadPaddingException e) {
                System.out.println("failed to encrypt AES 256 GCM: " + e);
                return null;
            }
        }

        public String decrypt(byte[] cipherText) {
            SecretKeySpec keySpec = new SecretKeySpec(key, "AES");
            GCMParameterSpec gcmParameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH * 8, iv);
            try {
                cipher.init(Cipher.DECRYPT_MODE, keySpec, gcmParameterSpec);
                byte[] decryptedText = cipher.doFinal(cipherText);
                return new String(decryptedText);
            } catch (InvalidKeyException | InvalidAlgorithmParameterException | IllegalBlockSizeException | BadPaddingException e) {
                System.out.println("failed to decrypt AES 256 GCM: " + e);
                return "";
            }
        }
    }

    public static byte[] sha1(byte[] bytes) {
        return DigestUtils.sha1(bytes);
    }

    public static byte[] sha1(String text) {
        return DigestUtils.sha1(text);
    }
}
