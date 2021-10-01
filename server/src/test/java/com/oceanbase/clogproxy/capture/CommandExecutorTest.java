package com.oceanbase.clogproxy.capture;

import java.io.IOException;

import com.oceanbase.clogproxy.util.CommandExecutor;
import com.oceanbase.clogproxy.util.CommandExecutor.Result;
import org.junit.Assert;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-21 Time: 15:15</p>
 *
 * @author jiyong.jy
 */
public class CommandExecutorTest {
    private static final Logger logger = LoggerFactory.getLogger(CommandExecutorTest.class);

    @Test
    public void testSync() {
        Result result = CommandExecutor.execute("echo 'hello world'");
        logger.info(result.toString());
        Assert.assertEquals(0, result.getExitCode());
        Assert.assertEquals("hello world", result.getResult());

        result = CommandExecutor.execute("aabbcc");
        Assert.assertEquals(result, result.sync());
        logger.info(result.toString());
        Assert.assertEquals(127, result.getExitCode());
        Assert.assertEquals("/bin/bash: aabbcc: command not found", result.getResult());
    }

    @Test
    public void testAsync() {
        Result result = CommandExecutor.execute("echo 'hello world'", r -> {
            logger.info(r.toString());
            Assert.assertEquals(0, r.getExitCode());
            Assert.assertEquals("hello world", r.getResult());
        });
        Assert.assertEquals(result, result.sync());
        logger.info(result.toString());
        Assert.assertEquals(0, result.getExitCode());
        Assert.assertEquals("hello world", result.getResult());

        result = CommandExecutor.execute("aabbcc", r -> {
            logger.info(r.toString());
            Assert.assertEquals(127, r.getExitCode());
            Assert.assertEquals("/bin/bash: aabbcc: command not found", r.getResult());
        });
        Assert.assertEquals(result, result.sync());
        logger.info(result.toString());
        Assert.assertEquals(127, result.getExitCode());
        Assert.assertEquals("/bin/bash: aabbcc: command not found", result.getResult());
    }

    @Test
    public void testExecuteStoreLogCapture() throws IOException, InterruptedException {
        String current = new java.io.File(".").getCanonicalPath();
        String path = current + "/src/main/resources/run_store_logcapture.sh";
        String parameter = " \"aabb 8891 manager.host=http://11.161.17.81:8088 subtopic=u_da_dev_mysql-5-1 timestamp=1587435107\"";
        Result result = CommandExecutor.execute("sh " + path + parameter);
        System.out.println(result.getExitCode());
        System.out.println(result.getResult());
    }
}
