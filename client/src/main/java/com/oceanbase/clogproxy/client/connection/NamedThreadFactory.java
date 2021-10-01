package com.oceanbase.clogproxy.client.connection;

import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 22:14</p>
 *
 * @author jiyong.jy
 */
public class NamedThreadFactory implements ThreadFactory {

    private static final AtomicInteger POOL_NUMBER = new AtomicInteger(1);
    private final AtomicInteger        threadNumber = new AtomicInteger(1);
    private final ThreadGroup          group;
    private final String               namePrefix;
    private final boolean              isDaemon;

    public NamedThreadFactory() {
        this("ThreadPool");
    }

    public NamedThreadFactory(String name) {
        this(name, false);
    }

    public NamedThreadFactory(String preffix, boolean daemon) {
        SecurityManager s = System.getSecurityManager();
        group = (s != null) ? s.getThreadGroup() : Thread.currentThread().getThreadGroup();
        namePrefix = preffix + "-" + POOL_NUMBER.getAndIncrement() + "-thread-";
        isDaemon = daemon;
    }

    /**
     * Create a thread.
     *
     * @see ThreadFactory#newThread(Runnable)
     */
    @Override
    public Thread newThread(Runnable r) {
        Thread t = new Thread(group, r, namePrefix + threadNumber.getAndIncrement(), 0);
        t.setDaemon(isDaemon);
        if (t.getPriority() != Thread.NORM_PRIORITY) {
            t.setPriority(Thread.NORM_PRIORITY);
        }
        return t;
    }

}