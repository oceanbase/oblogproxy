package com.oceanbase.clogproxy.client.config;

import com.oceanbase.clogproxy.common.config.ShareConf;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-06
 */
public class ClientConf extends ShareConf {
    public static final String VERSION = "1.1.0";

    public static int TRANSFER_QUEUE_SIZE = 20000;
    public static int CONNECT_TIMEOUT_MS = 5000;
    public static int READ_WAIT_TIME_MS = 2000;
    public static int RETRY_INTERVAL_S = 2;
    /**
     * max retry time when disconnect
     */
    public static int MAX_RECONNECT_TIMES = -1;
    public static int IDLE_TIMEOUT_S = 15;  // if not data income lasting IDLE_TIMEOUT_S, a reconnect we be trigger
    public static int NETTY_DISCARD_AFTER_READS = 16;
    /**
     * set user defined userid,
     * for inner use only
     */
    public static String USER_DEFINED_CLIENTID = "";

    /**
     * ignore unkown or unsupport record type with a warning log instead throwing an exception
     */
    public static boolean IGNORE_UNKNOWN_RECORD_TYPE = false;
}
