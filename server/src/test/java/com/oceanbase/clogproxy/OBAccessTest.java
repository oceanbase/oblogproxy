package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.obaccess.handshake.OBMysqlAuth;
import com.oceanbase.clogproxy.common.util.CryptoUtil;
import org.apache.commons.lang3.Conversion;
import org.junit.Assert;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-08-18
 */
public class OBAccessTest {
    private static Logger logger = LoggerFactory.getLogger(OBAccessTest.class);

    @Test
    public void testByteConvertion() {
        int ii3 = 65535;
        byte[] buf = new byte[4];
        Conversion.intToByteArray(ii3, 0, buf, 0, 2);
        int expected = Conversion.byteArrayToInt(buf, 0, 0, 0, 2);
        Assert.assertEquals(ii3, expected);

        ii3 = 65537;
        buf = new byte[4];
        Conversion.intToByteArray(ii3, 0, buf, 0, 2);
        expected = Conversion.byteArrayToInt(buf, 0, 0, 0, 2);
        Assert.assertEquals(1, expected);
    }

    @Test
    public void testMysqlHandshake() {
        OBMysqlAuth obMysqlAuth;
        obMysqlAuth = new OBMysqlAuth("127.0.0.1", 3306, "fankux", CryptoUtil.sha1("fffast"), "mysql");
        Assert.assertFalse(obMysqlAuth.auth());

        obMysqlAuth = new OBMysqlAuth("127.0.0.1", 3306, "fankux", CryptoUtil.sha1("fast"), "mysql");
        Assert.assertTrue(obMysqlAuth.auth());

        obMysqlAuth = new OBMysqlAuth("127.0.0.1", 3306, "fankux", CryptoUtil.sha1("fast"), "");
        Assert.assertTrue(obMysqlAuth.auth());

        obMysqlAuth = new OBMysqlAuth("obproxy.ocp2.alipay.net", 2883, "obvip_cp_1:cifgz0_2415:drctest", CryptoUtil.sha1("dddrctest"), "transtest");
        Assert.assertFalse(obMysqlAuth.auth());

        obMysqlAuth = new OBMysqlAuth("obproxy.ocp2.alipay.net", 2883, "obvip_cp_1:cifgz0_2415:drctest", CryptoUtil.sha1("drctest"), "transtest");
        Assert.assertTrue(obMysqlAuth.auth());

        obMysqlAuth = new OBMysqlAuth("obproxy.ocp2.alipay.net", 2883, "obvip_cp_1:cifgz0_2415:drctest", CryptoUtil.sha1("drctest"), "");
        Assert.assertTrue(obMysqlAuth.auth());
    }
}
