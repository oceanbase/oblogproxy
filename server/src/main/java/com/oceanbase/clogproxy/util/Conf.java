package com.oceanbase.clogproxy.util;

import com.oceanbase.clogproxy.common.config.ShareConf;
import com.oceanbase.clogproxy.common.util.TypeTrait;
import com.google.common.collect.Lists;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-14
 * <p>
 * singleton global conf
 */
public class Conf extends ShareConf {
    private static Logger logger = LoggerFactory.getLogger(Conf.class);
    public static final String VERSION = "1.0.0";

    public static int PROXY_SERVICE_PORT = 8890;
    public static int CAPTURE_SERVICE_PORT = 8891;
    public static int ENCODE_THREADPOOL_SIZE = 8;
    public static int ENCODE_QUEUE_SIZE = 50000;
    public static int SEND_QUEUE_SIZE = 50000;
    public static int WAIT_NUM = 35000;
    public static int WAIT_TIME_MS = 2000;
    public static int NETTY_ACCEPT_THREADPOOL_SIZE = 1;
    public static int NETTY_WORKER_THREADPOOL_SIZE = 10;
    public static int NETTY_DISCARD_AFTER_READ = 5;
    public static int MAX_PACKET_BYTES = 1024 * 1024 * 8;   // 8MB
    public static int CLIENT_BUFFER_INIT_BYTES = 2048;
    public static int COMMAND_TIMEOUT_S = 10;
    public static int CLIENT_INIT_TIMEOUT_S = 300;          // 5 mins
    public static int DETECT_INTERVAL_S = 120;              // 2 mins
    public static int METRIC_INTERVAL_S = 10;
    public static boolean MONINTOR = false;
    public static String MONINTOR_REGION = "";
    public static String MONINTOR_ENDPOINT = "";
    public static String MONINTOR_GROUP_ID;
    public static String MONINTOR_AK = "";
    public static String MONINTOR_SK = "";
    public static int OB_LOGREADER_PATH_RETAIN_HOUR = 168;  // 7 Days
    public static int OB_LOGREADER_LEASE_S = 300;           // 5 mins
    public static String OB_LOGREADER_PATH = "/home/ds/logreader";
    public static String OB_LOGREADER_RUN_SCRIPT = "/home/ds/logreader/bin/logreader_new.sh";
    public static String STORE_LOGCAPTURE_SCRIPT = "./bin/run_store_logcapture.sh";
    public static boolean ALLOW_ALL_TENANT = false;
    public static boolean AUTH_USER = true;
    public static boolean AUTH_USE_RS = false;
    public static boolean AUTH_ALLOW_SYS_USER = false;
    public static String OB_SYS_USERNAME = "";
    public static String OB_SYS_PASSWORD = "";

    // debug related
    public static boolean ADVANCED_LEAK_DETECT = false;     // enable netty leak detect advanced
    public static boolean DEBUG = false;                    // enable debug mode
    public static boolean VERBOSE = false;                  // print more log
    public static boolean VERBOSE_PACKET = false;           // print data packet info
    public static boolean READONLY = false;                 // only read from LogReader, use for test
    public static boolean COUNT_RECORD = false;

    public static boolean load(String filename) {
        Map<String, Object> confs;
        try {
            confs = JsonHint.load(filename);
        } catch (Exception e) {
            logger.error("failed to load config: {}, e", filename, e);
            return false;
        }

        Map<String, Field> fields = new HashMap<>();
        List<Field> reflectFields = Lists.newArrayList(Conf.class.getDeclaredFields());
        reflectFields.addAll(Arrays.asList(ShareConf.class.getDeclaredFields()));
        try {
            for (Field field : reflectFields) {
                field.setAccessible(true);
                if (Modifier.isStatic(field.getModifiers())) {
                    fields.put(field.getName().toLowerCase(), field);
                }
            }

            Conf inst = Conf.class.newInstance();
            for (Map.Entry<String, Object> conf : confs.entrySet()) {
                String key = conf.getKey();
                if (!fields.containsKey(key)) {
                    continue;
                }

                if (TypeTrait.isSameLooseType(conf.getValue(), fields.get(key))) {
                    fields.get(key).set(inst, conf.getValue());
                }
                logger.info("{}: {}", key, fields.get(key).get(inst));
            }
            return true;

        } catch (Exception e) {
            logger.error("failed to load config: {}, ", filename, e);
            return false;
        }
    }
}
