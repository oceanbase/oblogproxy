package com.oceanbase.clogproxy.capture;

import com.oceanbase.clogproxy.util.CommandExecutor;
import com.oceanbase.clogproxy.util.Conf;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-28 Time: 15:12</p>
 *
 * @author jiyong.jy
 */
public abstract class BaseShellSourceInvoke implements SourceInvoke {
    private Logger logger = LoggerFactory.getLogger(BaseShellSourceInvoke.class);

    @Override
    public boolean start(String clientId, String configuration) {
        // FIXME.. password encode
        final String shellScript = getStartCommand(clientId, configuration);
//        logger.info("Start execute shell script {}", shellScript);
        CommandExecutor.execute(shellScript, r -> {  // never blocking main loop
            String rForLog = r.toString().replace(Conf.OB_SYS_PASSWORD, "******");
            String scriptForLog = shellScript.replace(Conf.OB_SYS_PASSWORD, "******");
            if (!r.success()) {
                logger.error("Failed to execute shell script: {} of: {}", rForLog, scriptForLog);
                return;
            }
            logger.info("Success to execute shell script, result: {}", rForLog);
        });
        return true;
    }

    protected abstract String getStartCommand(String clientId, String configuration);
}
