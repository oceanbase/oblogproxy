package com.oceanbase.clogproxy.client.connection;

import com.oceanbase.clogproxy.client.ErrorCode;
import com.oceanbase.clogproxy.client.LogProxyClientException;
import com.oceanbase.clogproxy.client.RecordListener;
import com.oceanbase.clogproxy.client.StatusListener;
import com.oceanbase.clogproxy.client.checkpoint.CheckpointExtractor;
import com.oceanbase.clogproxy.client.checkpoint.CheckpointExtractorFactory;
import com.oceanbase.clogproxy.client.config.ClientConf;
import com.oceanbase.oms.record.oms.OmsRecord;
import org.apache.commons.collections.CollectionUtils;
import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-06
 * <p>
 * TODO... listener exception
 */
public class ClientStream {
    private static final Logger logger = LoggerFactory.getLogger(ClientStream.class);

    // routine
    private final AtomicBoolean started = new AtomicBoolean(false);
    private Thread thread = null;

    // status
    private StreamContext context = null;
    private String checkpointString;
    private final CheckpointExtractor checkpointExtractor;

    // reconnection
    private int retryTimes = 0;
    private Connection connection = null;
    private final AtomicBoolean reconnecting = new AtomicBoolean(true);
    private final AtomicBoolean reconnect = new AtomicBoolean(true);

    // user callbacks
    private final List<RecordListener> listeners = new ArrayList<>();
    private final List<StatusListener> statusListeners = new ArrayList<>();

    private enum ReconnectState {
        /**
         * success
         */
        SUCCESS,
        /**
         * retry connect next round
         */
        RETRY,
        /**
         * failed, exit thread
         */
        EXIT;
    }

    public ClientStream(ConnectionParams connectionParams) {
        context = new StreamContext(this, connectionParams);
        checkpointExtractor = CheckpointExtractorFactory.getCheckpointExtractor(context.getParams().getLogType());
    }

    public void stop() {
        if (!started.compareAndSet(true, false)) {
//            synchronized (this) { // ensure start, stop status consistence
            logger.info("stoping LogProxy Client....");

            if (connection != null) {
                connection.close();
                connection = null;
            }

            join();
            thread = null;
//            }
        }
        logger.info("stoped LogProxy Client");
    }

    public void join() {
        if (thread != null) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                logger.warn("exception occured when join LogProxy exit: ", e);
            }
        }
    }

    public void triggerStop() {
        new Thread(this::stop).start();
    }

    public void triggerException(LogProxyClientException e) {
        // use thread make sure non blocking
        new Thread(() -> {
            for (RecordListener listener : listeners) {
                listener.onException(e);
            }
        }).start();
    }

    public void start() {
        // if status listener exist, enable monitor
        context.params.setEnableMonitor(CollectionUtils.isNotEmpty(statusListeners));

        if (started.compareAndSet(false, true)) {
//            synchronized (this) { // ensure start, stop status consistence
            thread = new Thread(() -> {
                while (isRunning()) {
                    ReconnectState state = reconnect();
                    if (state == ReconnectState.EXIT) {
                        logger.error("read thread to exit");
                        triggerException(new LogProxyClientException(ErrorCode.E_MAX_RECONNECT, "exceed max reconnect retry"));
                        break;
                    }
                    if (state == ReconnectState.RETRY) {
                        try {
                            TimeUnit.SECONDS.sleep(ClientConf.RETRY_INTERVAL_S);
                        } catch (InterruptedException e) {
                            // do nothing
                        }
                        continue;
                    }

                    StreamContext.TransferPacket packet;
                    while (true) {
                        try {
                            packet = context.recordQueue().poll(ClientConf.READ_WAIT_TIME_MS, TimeUnit.MILLISECONDS);
                            break;
                        } catch (InterruptedException e) {
                            // do nothing
                        }
                    }
                    if (packet == null) {
                        continue;
                    }
                    try {
                        switch (packet.getType()) {
                            case DATA_CLIENT:
                                for (RecordListener listener : listeners) {
                                    listener.notify(packet.getRecord());
                                    checkpointString = checkpointExtractor.extract(packet.getRecord());
                                }
                                break;
                            case STATUS:
                                for (StatusListener listener : statusListeners) {
                                    listener.notify(packet.getStatus());
                                }
                                break;
                            default:
                                throw new LogProxyClientException(ErrorCode.E_PROTOCOL, "Unsupported Packet Type: " + packet.getType());
                        }
                    } catch (LogProxyClientException e) {
                        triggerStop();
                        triggerException(e);
                        return;

                    } catch (Exception e) {
                        // if exception occured, we exit
                        triggerStop();
                        triggerException(new LogProxyClientException(ErrorCode.E_USER, e));
                        return;
                    }
                }

                started.set(false);
                if (connection != null) {
                    connection.close();
                }
                thread = null;

                // TODO... if exception occured, run handler callback

                logger.warn("!!! read thread exit !!!");
            });

            thread.setDaemon(false);
            thread.start();
        }
//        }
    }

    public boolean isRunning() {
        return started.get();
    }

    private ReconnectState reconnect() {
        if (reconnect.compareAndSet(true, false)) { // reconnect flag mark, tiny load for checking
            logger.warn("start to reconnect...");

            try {
                if (ClientConf.MAX_RECONNECT_TIMES != -1 && retryTimes >= ClientConf.MAX_RECONNECT_TIMES) {
                    logger.error("failed to reconnect, exceed max reconnect retry time: {}", ClientConf.MAX_RECONNECT_TIMES);
                    reconnect.set(true);
                    return ReconnectState.EXIT;
                }

                if (connection != null) {
                    connection.close();
                    connection = null;
                }
                // when stoped, context.recordQueue may not empty, just use checkpointString to do reconnection.
                if (StringUtils.isNotEmpty(checkpointString)) {
                    logger.warn("reconnect set checkpoint: {}", checkpointString);
                    context.getParams().updateCheckpoint(checkpointString);
                }
                connection = ConnectionFactory.instance().createConnection(context.getParams().getHost(),
                    context.getParams().getPort(), context);
                if (connection != null) {
                    logger.warn("reconnect SUCC");
                    retryTimes = 0;
                    reconnect.compareAndSet(true, false);
                    return ReconnectState.SUCCESS;
                }

                logger.error("failed to reconnect, retry count: {}, max: {}", ++retryTimes, ClientConf.MAX_RECONNECT_TIMES);
                reconnect.set(true); // not succ, retry next time
                return ReconnectState.RETRY;

            } catch (Exception e) {
                logger.error("failed to reconnect, retry count: {}, max: {}", ++retryTimes, ClientConf.MAX_RECONNECT_TIMES);
                reconnect.set(true); // not succ, retry next time
                return ReconnectState.RETRY;

            } finally {
                reconnecting.set(false);
            }
        }
        return ReconnectState.SUCCESS;
    }

    public void triggerReconnect() {
        // reconnection action guard, avoid concurrent or multiple invoke
        if (reconnecting.compareAndSet(false, true)) {
            reconnect.compareAndSet(false, true);
        }
    }

    public synchronized void addListener(RecordListener recordListener) {
        listeners.add(recordListener);
    }

    public synchronized void addStatusListener(StatusListener statusListener) {
        statusListeners.add(statusListener);
    }

}
