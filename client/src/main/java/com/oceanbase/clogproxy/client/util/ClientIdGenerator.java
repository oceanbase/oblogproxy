package com.oceanbase.clogproxy.client.util;

import com.oceanbase.clogproxy.common.util.NetworkUtil;

import java.lang.management.ManagementFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-09
 */
public class ClientIdGenerator {
    /**
     * LocalIP_PID_currentTimestamp
     * pattern may be change, nerver depend on the content of this
     */
    public static String generate() {

        return NetworkUtil.getLocalIp() + "_" + getProcessId() + "_" + (System.currentTimeMillis() / 1000);
    }

    private static String getProcessId() {
        // Note: may fail in some JVM implementations
        // therefore fallback has to be provided

        // something like '<pid>@<hostname>', at least in SUN / Oracle JVMs
        final String jvmName = ManagementFactory.getRuntimeMXBean().getName();
        final int index = jvmName.indexOf('@');

        if (index < 1) {
            return "NOPID";
        }

        try {
            return Long.toString(Long.parseLong(jvmName.substring(0, index)));
        } catch (NumberFormatException e) {
            return "NOPID";
        }
    }
}
