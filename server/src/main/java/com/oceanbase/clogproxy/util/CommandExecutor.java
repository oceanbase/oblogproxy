package com.oceanbase.clogproxy.util;

import com.oceanbase.clogproxy.common.util.TaskExecutor;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.concurrent.TimeUnit;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-21 Time: 13:52</p>
 *
 * @author jiyong.jy
 */
public class CommandExecutor {

    private static final int SUCCESS = 0;

    public static class Result {

        private String result;
        private int exitCode;
        private TaskExecutor.Task<Void> task = null;

        public Result sync() {
            if (task != null) {
                task.get();
                task = null;
            }
            return this;
        }

        @Override
        public String toString() {
            return exitCode + ":" + result;
        }

        public String getResult() {
            return result;
        }

        public int getExitCode() {
            return exitCode;
        }

        public boolean success() {
            return exitCode == SUCCESS;
        }
    }

    public interface Callback {
        void done(Result result);
    }

    public static Result execute(String cmd) {
        return execute(cmd, null);
    }

    /**
     * If {@code cb} not null, call {@code cmd} Async, else Sync
     */
    public static Result execute(String cmd, Callback cb) {
        final Result result = new Result();
        result.task = TaskExecutor.instance().async(() -> {
            Process process = null;
            BufferedReader bufferedReader = null;
            try {
                String[] args = new String[]{"/bin/bash", "-c", cmd};
                ProcessBuilder processBuilder = new ProcessBuilder(args);
                process = processBuilder.redirectErrorStream(true).start();
                bufferedReader = new BufferedReader(new InputStreamReader(process.getInputStream()));
                String line;
                StringBuilder stringBuilder = new StringBuilder();
                while ((line = bufferedReader.readLine()) != null) {
                    stringBuilder.append(line).append('\n');
                }
                if (process.waitFor(Conf.COMMAND_TIMEOUT_S, TimeUnit.SECONDS)) {
                    result.exitCode = process.exitValue();
                } else {
                    result.exitCode = -1;
                }
                result.result = stringBuilder.toString();
            } catch (Exception e) {
                result.exitCode = -1;
                result.result = e.getMessage();
            } finally {
                if (cb != null) {
                    cb.done(result);
                }

                if (bufferedReader != null) {
                    try {
                        bufferedReader.close();
                    } catch (IOException e) {
                        // do nothing
                    }
                }
                if (process != null) {
                    process.destroy();
                }
            }
            return null;
        }, (e) -> {
            result.exitCode = -1;
            result.result = e.getMessage();
        });
        if (cb == null) {
            return result.sync();
        }
        return result;
    }
}
