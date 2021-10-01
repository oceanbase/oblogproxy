package com.oceanbase.clogproxy.metric;

import com.google.common.collect.Maps;
import com.oceanbase.clogproxy.common.util.NetworkUtil;
import com.oceanbase.clogproxy.common.util.TaskExecutor;
import com.oceanbase.clogproxy.stream.ClientId;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.util.Inc;
import io.dropwizard.metrics5.MetricRegistry;
import io.dropwizard.metrics5.jvm.GarbageCollectorMetricSet;
import io.netty.util.internal.PlatformDependent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Field;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Function;

import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.RuntimeStatus;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-12-08
 */
public class LogProxyMetric {
    private final Logger logger = LoggerFactory.getLogger(StreamMetric.class);

    public static LogProxyMetric instance() {
        return LogProxyMetric.Singleton.instance;
    }

    static class Singleton {
        static LogProxyMetric instance = new LogProxyMetric();
    }

    MetricRegistry gc = new MetricRegistry();

    private AtomicLong nettyMemoryCounter;

    private boolean runFlag = true;

    public void stop() {
        runFlag = false;
    }

    public LogProxyMetric() { }

    public boolean init() {
        try {
            Field field = PlatformDependent.class.getDeclaredField("DIRECT_MEMORY_COUNTER");
            field.setAccessible(true);
            nettyMemoryCounter = (AtomicLong) field.get(PlatformDependent.class);

        } catch (NoSuchFieldException | IllegalAccessException e) {
            logger.error("failed to fetch Netty PlatformDependent.DIRECT_MEMORY_COUNTER");
            return false;
        }

        gc.register("gc", new GarbageCollectorMetricSet());
        TaskExecutor.instance().background(() -> {
            routine();
            return null;
        });
        return true;
    }

    Map<ClientId, Function<RuntimeStatus, Integer>> cbs = Maps.newConcurrentMap();

    public synchronized void registerRuntimeStatusCallback(ClientId clientId, Function<RuntimeStatus, Integer> callback) {
        cbs.put(clientId, callback);
    }

    public synchronized void removeRuntimeStatusCallback(ClientId clientId) {
        cbs.remove(clientId);
    }

    public void routine() {
        while (runFlag) {
            Inc.sleep(Conf.METRIC_INTERVAL_S * 1000L);

            StringBuilder sb = new StringBuilder();
            sb.append(String.format("[netty.direct.memory:%dK][thd_count:%d]", nettyMemoryCounter.get() / 1024, Thread.activeCount()));
            sb.append(String.format("[task.async:%d][task.concurrent:%d][task.bg:%s]", TaskExecutor.instance().getAsyncTaskCount(),
                TaskExecutor.instance().getConcurrentTaskCount(), TaskExecutor.instance().getBgTaskCount()));
            gc.getGauges().forEach((name, gauge) -> sb.append('[').append(name).append(':').append(gauge.getValue()).append(']'));
            logger.info(sb.toString());

            RuntimeStatus status = RuntimeStatus.newBuilder().
                setIp(NetworkUtil.getLocalIp()).
                setPort(Conf.PROXY_SERVICE_PORT).
                setStreamCount(0).
                setWorkerCount(0).build();
            for (Map.Entry<ClientId, Function<RuntimeStatus, Integer>> cb : cbs.entrySet()) {
                cb.getValue().apply(status);
            }
        }
    }
}
