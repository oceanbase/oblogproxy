package com.oceanbase.clogproxy.capture;

import com.oceanbase.clogproxy.util.Conf;
import org.apache.commons.lang3.StringUtils;
import org.junit.Assert;
import org.junit.Test;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-28 Time: 15:30</p>
 *
 * @author jiyong.jy
 */
public class LogReaderInvokeTest {

    @Test
    public void testShellScript() {
        Conf.CAPTURE_SERVICE_PORT = 1234;
        String clientId = "testShellScript";
        String configuration = "aaa=bbb ccc=ddd eee=fff";

        String expected = "sh " + Conf.OB_LOGREADER_RUN_SCRIPT + " " + clientId + " " + Conf.CAPTURE_SERVICE_PORT + " " + StringUtils.replace(configuration, " ", ",");

        LogReaderInvoke processHandler = new LogReaderInvoke();
        String shellScript = processHandler.getStartCommand(clientId, configuration);
        Assert.assertEquals(expected, shellScript);

    }
}
