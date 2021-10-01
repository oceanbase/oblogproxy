package com.oceanbase.clogproxy.metric;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-26 Time: 13:44</p>
 *
 * @author jiyong.jy
 */
public class MetricConstants {

    // SUFFIX
    public static final String AVG = "_avg";
    public static final String P99 = "_p99";
    public static final String P95 = "_p95";
    public static final String P75 = "_p75";
    public static final String MEDIAN = "_median";

    // METRIC NAMES
    public static final String METRIC_DELAY = "delay";
    public static final String METRIC_READ_TPS = "RTPS";
    public static final String METRIC_READ_IOS = "RIOS";
    public static final String METRIC_WRITE_TPS = "WTPS";
    public static final String METRIC_WRITE_IOS = "WIOS";
    public static final String METRIC_ENCODE_QUEUE_SIZE = "encode_Q_size";
    public static final String METRIC_WRITE_QUEUE_SIZE = "send_Q_size";

    public static final String METRIC_READONLY_CHANNEL_ID = "READONLY-CHANNEL";

}
