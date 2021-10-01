package com.oceanbase.clogproxy.capture;

import com.oceanbase.clogproxy.util.CommandExecutor;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.util.Inc;
import com.google.common.base.CharMatcher;
import com.google.common.base.Splitter;
import com.google.common.collect.Lists;
import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-28 Time: 15:09</p>
 *
 * @author jiyong.jy
 */
public class LogReaderInvoke extends BaseShellSourceInvoke {
    private static final Logger logger = LoggerFactory.getLogger(LogReaderInvoke.class);

    private final Splitter splitterRow = Splitter.on('\n').omitEmptyStrings().trimResults();
    private final Splitter splitterCol = Splitter.on(CharMatcher.anyOf(" \t")).omitEmptyStrings().trimResults().limit(3);

    @Override
    public boolean init() {
        return true;
    }

    @Override
    public boolean isAlive(SourceProcess source) {
        // TODO... socket check alive packet
        CommandExecutor.Result result = CommandExecutor.execute(
                String.format("sh ./bin/logreader_status.sh %s '%s'", source.getPid(), source.getPath()));
        return result.success();
    }

    @Override
    public void stop(SourceProcess source) {
        if (StringUtils.isNotEmpty(source.getPid())) {
            CommandExecutor.Result result = CommandExecutor.execute("kill -9 " + source.getPid());
            logger.warn("try to stop LogReader: {}, execute result: {}", source, result);
        }
    }

    @Override
    public void clean(SourcePath path) {
        if (Inc.FORBIDDEN_PATHS.contains(path.getPath())) {
            logger.warn("!!!Dangerous LogReader path: {}", path.getPath());
            return;
        }
        // Not performance sensitive here, iterator is OK. Safety is first.
        for (String prefix : Inc.FORBIDDEN_PREFIXS) {
            if (path.getPath().startsWith(prefix)) {
                logger.warn("!!!Dangerous LogReader path: {}", path.getPath());
                return;
            }
        }

        CommandExecutor.Result result = CommandExecutor.execute("rm -rf " + path.getPath());
        if (!result.success()) {
            logger.error("failed to clean LogReader path: {}, result: {}", path, result);
        }
    }

    @Override
    public List<SourceProcess> listProcesses() {
        List<SourceProcess> processes = Lists.newArrayList();
        String path = Conf.OB_LOGREADER_PATH + "/run";

        /*
            Output pattern:
            PID1    run_path1   startTime1
            PID2    run_path2   startTime2
            ...
         */
        CommandExecutor.Result result = CommandExecutor.execute("sh ./bin/list_logreader_process.sh " + path);
        if (!result.success()) {
            logger.error("failed to list LogReader process: {}", result);
            return processes;
        }

        List<String> rows = splitterRow.splitToList(result.getResult());
        for (String row : rows) {
            SourceProcess info = new SourceProcess();
            List<String> cols = splitterCol.splitToList(row);
            if (cols.size() > 0) {
                info.pid = cols.get(0);
            }
            if (cols.size() > 1) {
                info.path = cols.get(1);
            }
            if (cols.size() > 2) {
                try {
                    info.startTime = Long.parseLong(cols.get(2));
                } catch (NumberFormatException e) {
                    info.startTime = 0;
                }
            }
            processes.add(info);
        }
        return processes;
    }

    @Override
    public List<SourcePath> listPaths() {
        // TODO... use fs API
        List<SourcePath> paths = Lists.newArrayList();
        String path = Conf.OB_LOGREADER_PATH + "/run";

        /*
            Output pattern:
            last_time1    path1
            last_time2    path2
            ...
         */
        CommandExecutor.Result result = CommandExecutor.execute("sh ./bin/list_logreader_path.sh " + path);
        if (!result.success()) {
            logger.error("failed to list LogReader path: {}", result);
            return paths;
        }

        List<String> rows = splitterRow.splitToList(result.getResult());
        for (String row : rows) {
            SourcePath sourcePath = new SourcePath();
            List<String> cols = splitterCol.splitToList(row);
            if (cols.size() > 0) {
                try {
                    sourcePath.lastTime = Integer.parseInt(cols.get(0));
                } catch (Exception e) {
                    sourcePath.lastTime = Integer.MAX_VALUE; // unexpected, better to make sure not be deleted
                }
            }
            if (cols.size() > 1) {
                sourcePath.path = cols.get(1);
            }
            if (StringUtils.isNotEmpty(sourcePath.path)) {
                int pos = sourcePath.path.lastIndexOf('/');
                try {
                    sourcePath.clientId = sourcePath.path.substring(pos);
                } catch (Exception e) {
                    sourcePath.clientId = "";
                }
            }
            sourcePath.clientId = StringUtils.strip(sourcePath.clientId, "/");
            paths.add(sourcePath);
        }
        return paths;
    }

    @Override
    protected String getStartCommand(String clientId, String configuration) {
        configuration = StringUtils.replace(configuration, " ", ",");
        return "bash " + Conf.OB_LOGREADER_RUN_SCRIPT + " '" + clientId + "' '" + clientId +
                "' '" + configuration + "' " + Conf.CAPTURE_SERVICE_PORT + " 127.0.0.1";
    }
}
