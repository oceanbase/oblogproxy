package com.oceanbase.clogproxy.util;

import com.google.common.collect.ImmutableSet;

import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-16
 * <p>
 * common use methods
 */
public class Inc {

    public static void sleep(long timeMs) {
        try {
            TimeUnit.MILLISECONDS.sleep(timeMs);
        } catch (InterruptedException e) {
            // do nothing
        }
    }

    public static final Set<String> FORBIDDEN_PATHS = ImmutableSet.<String>builder().
            add("/").add("/tmp").add("/home").add("/mnt").add("/root").build();

    public static final Set<String> FORBIDDEN_PREFIXS = ImmutableSet.<String>builder().
            add("/bin").add("/cgroup").add("/dev").add("/lib").add("/lost+found").add("/proc").add("/sbin").
            add("/service").add("/var").add("/boot").add("/etc").add("/lib64").add("/media").add("/opt").
            add("/selinux").add("/srv").add("/sys").add("/usr").build();

}
