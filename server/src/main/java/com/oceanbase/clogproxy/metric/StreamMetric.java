package com.oceanbase.clogproxy.metric;

import com.oceanbase.clogproxy.common.util.TaskExecutor;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.util.Inc;
import io.dropwizard.metrics5.Gauge;
import io.dropwizard.metrics5.MetricName;
import io.dropwizard.metrics5.MetricRegistry;
import io.dropwizard.metrics5.MetricRegistry.MetricSupplier;
import io.dropwizard.metrics5.Sampling;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;

import static com.oceanbase.clogproxy.metric.MetricConstants.AVG;
import static com.oceanbase.clogproxy.metric.MetricConstants.MEDIAN;
import static com.oceanbase.clogproxy.metric.MetricConstants.P75;
import static com.oceanbase.clogproxy.metric.MetricConstants.P95;
import static com.oceanbase.clogproxy.metric.MetricConstants.P99;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-26 Time: 13:42</p>
 *
 * @author jiyong.jy
 * <p>
 */
public class StreamMetric {
    private final Logger logger = LoggerFactory.getLogger(StreamMetric.class);

    public static StreamMetric instance() {
        return Singleton.instance;
    }

    static class Singleton {
        static StreamMetric instance = new StreamMetric();
    }

    private StreamMetric() {
        TaskExecutor.instance().background(() -> {
            routine();
            return null;
        });
    }

    private final Map<String, MetricRegistry> registrys = new ConcurrentHashMap<>();

    private boolean runFlag = true;

    public void stop() {
        runFlag = false;
    }

    public void routine() {
        while (runFlag) {
            Inc.sleep(Conf.METRIC_INTERVAL_S * 1000L);
            StringBuilder sb = new StringBuilder();

            Map<MetricName, Object> items = new HashMap<>();
            for (Map.Entry<String, MetricRegistry> entry : registrys.entrySet()) { // <cliendId, metrics>
                MetricRegistry registry = entry.getValue();

                items.clear();
                registry.getCounters().forEach((name, counter) -> {
                    long count = counter.getCount();
                    items.put(name, count);
                    counter.dec(count);
                });

                if (!items.isEmpty()) {
                    sb.delete(0, sb.length());
                    sb.append("[").append(entry.getKey()).append(']');
                    for (Map.Entry<MetricName, Object> item : items.entrySet()) {
                        sb.append('[').append(item.getKey()).append(':').append(item.getValue()).append(']');
                    }
                    logger.info(sb.toString());

                    Long monv = (Long) items.get(new MetricName(MetricConstants.METRIC_READ_TPS, Collections.emptyMap()));
                    if (monv != null) {
                        Monitor.instance().rtps(entry.getKey(), monv);
                    }
                    monv = (Long) items.get(new MetricName(MetricConstants.METRIC_READ_IOS, Collections.emptyMap()));
                    if (monv != null) {
                        Monitor.instance().rios(entry.getKey(), monv);
                    }
                    monv = (Long) items.get(new MetricName(MetricConstants.METRIC_WRITE_TPS, Collections.emptyMap()));
                    if (monv != null) {
                        Monitor.instance().wtps(entry.getKey(), monv);
                    }
                    monv = (Long) items.get(new MetricName(MetricConstants.METRIC_WRITE_IOS, Collections.emptyMap()));
                    if (monv != null) {
                        Monitor.instance().wios(entry.getKey(), monv);
                    }
                }

                items.clear();
                registry.getGauges().forEach((name, gauge) -> items.put(name, gauge.getValue()));
                if (!items.isEmpty()) {
                    sb.delete(0, sb.length());
                    sb.append("[").append(entry.getKey()).append(']');
                    for (Map.Entry<MetricName, Object> item : items.entrySet()) {
                        sb.append('[').append(item.getKey()).append(':').append(item.getValue()).append(']');
                    }
                    logger.info(sb.toString());

                    Long monv = (Long) items.get(new MetricName(MetricConstants.METRIC_DELAY, Collections.emptyMap()));
                    if (monv != null) {
                        Monitor.instance().delay(entry.getKey(), monv);
                    }
                }

                items.clear();
                registry.getTimers().forEach(((name, timer) -> items.putAll(getSamplingMetric(name, timer))));
                if (!items.isEmpty()) {
                    sb.delete(0, sb.length());
                    sb.append("[").append(entry.getKey()).append(']');
                    for (Map.Entry<MetricName, Object> sampling : items.entrySet()) {
                        sb.append('[').append(sampling.getKey()).append(':').append(sampling.getValue()).append(']');
                    }
                    logger.info(sb.toString());
                }
            }
        }
    }

    private Map<MetricName, Long> getSamplingMetric(MetricName metricName, Sampling sampling) {
        Map<MetricName, Long> metricNameValueMap = new HashMap<>();
        metricNameValueMap.put(new MetricName(metricName.getKey() + AVG, metricName.getTags()), TimeUnit.NANOSECONDS.toMillis(
            (long) sampling.getSnapshot().getMean()));
        metricNameValueMap.put(new MetricName(metricName.getKey() + P99, metricName.getTags()), TimeUnit.NANOSECONDS.toMillis(
            (long) sampling.getSnapshot().get99thPercentile()));
        metricNameValueMap.put(new MetricName(metricName.getKey() + P95, metricName.getTags()), TimeUnit.NANOSECONDS.toMillis(
            (long) sampling.getSnapshot().get95thPercentile()));
        metricNameValueMap.put(new MetricName(metricName.getKey() + P75, metricName.getTags()), TimeUnit.NANOSECONDS.toMillis(
            (long) sampling.getSnapshot().get75thPercentile()));
        metricNameValueMap.put(new MetricName(metricName.getKey() + MEDIAN, metricName.getTags()), TimeUnit.NANOSECONDS.toMillis(
            (long) sampling.getSnapshot().getMedian()));
        return metricNameValueMap;
    }

    private void counter(String name, String id, long number) {
        MetricRegistry registry = registrys.get(id);
        if (registry == null) {
            registry = new MetricRegistry();
            registrys.put(id, registry);
        }
        registry.counter(new MetricName(name, Collections.emptyMap())).inc(number);
    }

    public void rtps(String clientId, long number) {
        counter(MetricConstants.METRIC_READ_TPS, clientId, number);
    }

    public void rios(String clientId, long number) {
        counter(MetricConstants.METRIC_READ_IOS, clientId, number);
    }

    public void wtps(String clientId, long number) {
        counter(MetricConstants.METRIC_WRITE_TPS, clientId, number);
    }

    public void wios(String clientId, long number) {
        counter(MetricConstants.METRIC_WRITE_IOS, clientId, number);
    }

    public void addGauge(String name, String id, MetricSupplier<Gauge> gaugeMetricSupplier) {
        MetricRegistry registry = registrys.get(id);
        if (registry == null) {
            registry = new MetricRegistry();
            registrys.put(id, registry);
        }

        registry.gauge(new MetricName(name, Collections.emptyMap()), gaugeMetricSupplier);
    }

    public void remove(String clientId) {
        registrys.remove(clientId);
    }
}
