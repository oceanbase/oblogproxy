package com.oceanbase.clogproxy.capture;

import com.oceanbase.clogproxy.util.CommandExecutor;
import com.oceanbase.clogproxy.util.Conf;
import com.google.common.collect.Lists;
import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.util.List;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 16:28</p>
 *
 * @author jiyong.jy
 */
public class DrcStoreInvoke extends BaseShellSourceInvoke {
    private static Logger logger = LoggerFactory.getLogger(DrcStoreInvoke.class);

    public DrcStoreInvoke() {}

    @Override
    public boolean init() {
        if (Conf.STORE_LOGCAPTURE_SCRIPT.isEmpty()) {
            return true;
        }

        File script = FileUtils.getFile(Conf.STORE_LOGCAPTURE_SCRIPT);
        if (!script.exists() || !script.isFile()) {
            logger.error("failed to find log capture script: {}", Conf.STORE_LOGCAPTURE_SCRIPT);
            return false;
        }

        // bash ./logcapture_script.sh check, 0: succ, -1: failed
        String cmd = "bash " + Conf.STORE_LOGCAPTURE_SCRIPT + " check";
        logger.info("check store log capture: {}", cmd);

        CommandExecutor.Result result;
        try {
            result = CommandExecutor.execute(cmd);
            if (!result.success()) {
                throw new IllegalArgumentException("failed to run log capture script check: " + cmd + ", exit: " +
                        result.getExitCode() + ", message: " + result.getResult());
            }
            return true;

        } catch (Exception e) {
            logger.error("failed to run log capture script check: {}", cmd, e);
            return false;
        }
    }

    @Override
    public boolean isAlive(SourceProcess source) {
        return false;
    }

    @Override
    public void stop(SourceProcess pid) { }

    @Override
    public void clean(SourcePath path) { }

    @Override
    public List<SourceProcess> listProcesses() {
        return Lists.newArrayList();
    }

    @Override
    public List<SourcePath> listPaths() {
        return Lists.newArrayList();
    }

    @Override
    protected String getStartCommand(String clientId, String configuration) {
        return "bash " + Conf.STORE_LOGCAPTURE_SCRIPT + " start '" + clientId + "' '" + Conf.CAPTURE_SERVICE_PORT + "' '" + configuration + "' ";
    }
}
