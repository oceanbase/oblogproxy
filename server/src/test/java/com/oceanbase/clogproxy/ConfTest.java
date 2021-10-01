package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.util.Conf;
import org.junit.Assert;
import org.junit.Test;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-14
 */
public class ConfTest {

    @Test
    public void testLoadConf() {
        Assert.assertFalse(Conf.load("error.json"));
        Assert.assertTrue(Conf.load(getClass().getResource("/conf.json").getPath()));
        Assert.assertEquals(Conf.PROXY_SERVICE_PORT, 8234);
        Assert.assertEquals(Conf.CAPTURE_SERVICE_PORT, 8235);
        Assert.assertEquals(Conf.ENCODE_THREADPOOL_SIZE, 8);
        Assert.assertEquals(Conf.ENCODE_QUEUE_SIZE, 20000);
        Assert.assertEquals(Conf.SEND_QUEUE_SIZE, 20000);
        Assert.assertEquals(Conf.WAIT_NUM, 20000);
        Assert.assertEquals(Conf.WAIT_TIME_MS, 2000);
        Assert.assertEquals(Conf.NETTY_DISCARD_AFTER_READ, 16);
        Assert.assertTrue(Conf.DEBUG);
        Assert.assertTrue(Conf.VERBOSE);
        Assert.assertEquals(Conf.METRIC_INTERVAL_S, 3);
        Assert.assertEquals(Conf.OB_LOGREADER_RUN_SCRIPT, "/home/yuqi.fy/dev/logreader/logreader_new.sh");

        Assert.assertTrue(Conf.AUTH_PASSWORD_HASH);
    }
}
