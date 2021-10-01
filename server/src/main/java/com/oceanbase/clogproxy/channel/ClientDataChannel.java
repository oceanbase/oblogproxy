package com.oceanbase.clogproxy.channel;

import com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto;
import com.oceanbase.clogproxy.common.util.TaskExecutor;
import com.oceanbase.clogproxy.metric.MetricConstants;
import com.oceanbase.clogproxy.metric.StreamMetric;
import com.oceanbase.clogproxy.packet.ClientMeta;
import com.oceanbase.clogproxy.packet.Packet;
import com.oceanbase.clogproxy.packet.PacketsEncoder;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.util.Inc;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelFutureListener;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 15:46</p>
 *
 * @author jiyong.jy
 */
public class ClientDataChannel implements DataChannel {

    private static final Logger logger = LoggerFactory.getLogger(ClientDataChannel.class);

    private AtomicBoolean started = new AtomicBoolean(false);
    private AtomicBoolean failed = new AtomicBoolean(false);

    private PacketsEncoder packetsEncoder = new PacketsEncoder();

    private ClientMeta meta;
    private Channel channel;
    private BlockingQueue<Packet> encodeQueue = new LinkedBlockingQueue<>(Conf.ENCODE_QUEUE_SIZE);
    private BlockingQueue<Future<List<PacketsEncoder.Result>>> sendQueue = new LinkedBlockingQueue<>(Conf.SEND_QUEUE_SIZE);

    private TaskExecutor.BackgroundTask encodeTask = null;
    private TaskExecutor.BackgroundTask sendTask = null;

    private TaskExecutor.ConcurrentTask encodePool = TaskExecutor.instance().refConcurrent("encodePool", Conf.ENCODE_THREADPOOL_SIZE);

    public ClientDataChannel(ClientMeta meta, Channel channel, boolean run) {
        this.meta = meta;
        this.channel = channel;
        if (run) {
            start();
        }
    }

    public ClientDataChannel(ClientMeta meta, Channel channel) {
        this(meta, channel, true);
    }

    @Override
    public void start() {
        if (started.compareAndSet(false, true)) {
            StreamMetric.instance().addGauge(MetricConstants.METRIC_ENCODE_QUEUE_SIZE, meta.getClientId().get(), () -> () -> encodeQueue.size());
            StreamMetric.instance().addGauge(MetricConstants.METRIC_WRITE_QUEUE_SIZE, meta.getClientId().get(), () -> () -> sendQueue.size());

            sendTask = TaskExecutor.instance().background(() -> {
                sendRoutine();
                return null;
            }, (e) -> {
                logger.error("#### SendTask: {} unrecovered exception occured, disconnect and stop!!!", meta.getClientId());
                triggerStop();
            });

            encodeTask = TaskExecutor.instance().background(() -> {
                encodeRoutine();
                return null;
            }, (e) -> {
                logger.error("#### EncodeTask: {} unrecovered exception occured, disconnect and stop!!!", meta.getClientId());
                triggerStop();
            });
        }
    }

    @Override
    public void stop() {
        logger.info("#### Closing clientChannel of Client: {}", meta.getClientId());
        if (started.compareAndSet(true, false)) {
            if (channel != null) {
                channel.close();
            }

            if (encodeTask != null) {
                encodeTask.join();
            }
            if (sendTask != null) {
                sendTask.join();
            }

            cleanMetric();

            // clean pending queue
            logger.warn("#### {} packets in EncodeQueue to clean", encodeQueue.size());
            List<Packet> encodes = new ArrayList<>(encodeQueue.size());
            encodeQueue.drainTo(encodes);
            for (Packet packet : encodes) {
                packet.release();
            }

            logger.warn("#### {} packets in SendQueue to clean", sendQueue.size());
            List<Future<List<PacketsEncoder.Result>>> byteBufs = new ArrayList<>(sendQueue.size());
            sendQueue.drainTo(byteBufs);
            for (Future<List<PacketsEncoder.Result>> byteBuf : byteBufs) {
                try {
                    List<PacketsEncoder.Result> results = byteBuf.get();
                    for (PacketsEncoder.Result result : results) {
                        result.getBuf().release();
                    }
                } catch (InterruptedException | ExecutionException e) {
                    // do nothing
                }
            }
        }
        logger.info("#### Closed clientChannel of Client: {}", meta.getClientId());
    }

    /**
     * used for stop thread itself.
     */
    private void triggerStop() {
        TaskExecutor.instance().async((Callable<Void>) () -> {
            stop();
            return null;
        });
    }

    @Override
    public void push(Packet packet) {
        while (started.get()) {
            try {
                encodeQueue.put(packet);
            } catch (InterruptedException e) {
                logger.error("failed to push Packet to channel, interrupted, retry...");
                continue;
            }
            break;
        }
    }

    public void pushRuntimeStatus(LogProxyProto.RuntimeStatus status) {
        Future<List<PacketsEncoder.Result>> result = encodePool.concurrent(() -> packetsEncoder.encodeRuntimeStatusV1(status));

        while (started.get()) {
            try {
                sendQueue.put(result);
                break;
            } catch (InterruptedException e) {
                logger.error("failed to push Runtime Status Packet to channel, interrupted, retry...");
            }
        }
    }

    /**
     * if any error occure in this thread, we retry infinite
     */
    public void encodeRoutine() {
        logger.info("#### Start EncodeRoutine: {}", meta.getClientId());

        while (started.get()) {
            List<Packet> packets = new ArrayList<>(Conf.WAIT_NUM + 1);
            long start = System.currentTimeMillis();

            try {
                do { // used as timed wait
                    Packet packet = encodeQueue.poll(Conf.WAIT_TIME_MS, TimeUnit.MILLISECONDS);
                    if (packet == null) { // timeout
                        break;
                    }
                    packets.add(packet);
                    encodeQueue.drainTo(packets, Conf.WAIT_NUM);
                } while (packets.size() < Conf.WAIT_NUM && System.currentTimeMillis() - start < Conf.WAIT_TIME_MS);
                if (packets.isEmpty()) {
                    continue;
                }
            } catch (InterruptedException e) {
                logger.error("failed to encode due to failed to poll from encodeQueue: ", e);
                Inc.sleep(Conf.WAIT_TIME_MS);
                continue;
            }

            Future<List<PacketsEncoder.Result>> byteBufFutures;
            do {
                try {
                    byteBufFutures = encodePool.concurrent(() -> packetsEncoder.encodeV0(packets));
                    break;
                } catch (RejectedExecutionException e) {
                    logger.error("failed to encode due to failed to submit encode task: ", e);
                    Inc.sleep(Conf.WAIT_TIME_MS);
                }
            } while (true);

            do {
                try {
                    sendQueue.put(byteBufFutures);
                    break;
                } catch (InterruptedException e) {
                    logger.error("failed to encode due to failed to add to sendQueue: ", e);
                    Inc.sleep(Conf.WAIT_TIME_MS);
                }
            } while (true);

            if (Conf.VERBOSE) {
                logger.info("{} packets inqueue to send, SendQueue now size: {}", packets.size(), sendQueue.size());
            }
        }
        logger.info("#### Exit EncodeRoutine: {}", meta.getClientId());
    }

    class CompleteListener implements ChannelFutureListener {
        private int count;
        private int byteSize;
        private long encodeCost;
        private long startTime;

        CompleteListener(int count, int byteSize, long encodeCost, long startTime) {
            this.count = count;
            this.byteSize = byteSize;
            this.encodeCost = encodeCost;
            this.startTime = startTime;
        }

        @Override
        public void operationComplete(ChannelFuture future) {
            if (future.isSuccess()) {
                metric(count, byteSize);
                if (Conf.VERBOSE) {
                    logger.info("[channelId:{}][encodeCost:{}ms][sendCost:{}ms][count:{}][size:{}KB] send packet SUCC",
                        meta.getId(), encodeCost, System.currentTimeMillis() - startTime, count, byteSize / 1024);
                }
            } else {
                if (failed.compareAndSet(false, true)) {
                    logger.error("failed to send to client: {}", meta.getClientId().toString(), future.cause());
                }
            }
        }
    }

    public void sendRoutine() throws Exception {
        logger.info("#### Start SendRoutine: {}", meta.getClientId());

        List<Future<List<PacketsEncoder.Result>>> byteBufFutures = new ArrayList<>(Conf.SEND_QUEUE_SIZE);
        while (started.get()) {
            try {
                byteBufFutures.clear();
                long start = System.currentTimeMillis();
                do { // used as timed wait
                    Future<List<PacketsEncoder.Result>> byteBuf = sendQueue.poll(Conf.WAIT_TIME_MS, TimeUnit.MILLISECONDS);
                    if (byteBuf == null) { // timeout
                        break;
                    }
                    byteBufFutures.add(byteBuf);
                    sendQueue.drainTo(byteBufFutures, Conf.WAIT_NUM);
                } while (byteBufFutures.size() < Conf.WAIT_NUM && System.currentTimeMillis() - start < Conf.WAIT_TIME_MS);

            } catch (InterruptedException e) {
                // do nothing, continue
            }

            for (Future<List<PacketsEncoder.Result>> byteBufFuture : byteBufFutures) {
                List<PacketsEncoder.Result> results = null; // Blocking, Future of encode Pool
                while (true) {
                    try {
                        results = byteBufFuture.get();
                        break;
                    } catch (InterruptedException e) {
                        logger.error("failed to send packet due to failed to poll from sendQueue: ", e);
                        Inc.sleep(Conf.WAIT_TIME_MS);

                    } catch (ExecutionException e) {
                        logger.error("failed to send packet due to failed wait encode execution, EXIT THREAD: ", e);
                        throw e; // Exit thread directly
                    }
                }

                for (PacketsEncoder.Result result : results) {
                    long now = System.currentTimeMillis();

                    if (Conf.DEBUG && Conf.READONLY) {
                        metric(result.getCount(), result.getByteSize());
                        if (Conf.VERBOSE) {
                            logger.info("[channelId:READONLY-CHANNEL][encodeCost:{}ms][sendCost:0ms][count:{}]" +
                                    "[size:{}KB] send packet SUCC",
                                result.getEncodeCost(), result.getCount(), result.getByteSize() / 1024);
                        }
                        result.getBuf().release();

                    } else {
//                                byte[] bytes = new byte[19];
//                                result.getBuf().readBytes(bytes);
//                                logger.info("packet MD5: ({}){}, hexdump: ({})\n{}", bytes.length,
//                                        DigestUtils.md5Hex(bytes), bytes.length, HexDump.format(bytes));
//                                result.getBuf().readerIndex(0);

                        channel.writeAndFlush(result.getBuf()).addListener(new CompleteListener(result.getCount(),
                            result.getByteSize(), result.getEncodeCost(), now));
                    }
                }
            }
        }
        logger.info("#### Exit SendRoutine: {}", meta.getClientId());
    }

    private void metric(int size, int bytesSize) {
        StreamMetric.instance().wtps(meta.getClientId().get(), size);
        StreamMetric.instance().wios(meta.getClientId().get(), bytesSize);
    }

    private void cleanMetric() {
        if (Conf.READONLY) {
            StreamMetric.instance().remove(MetricConstants.METRIC_READONLY_CHANNEL_ID);
        }
        if (meta != null) {
            StreamMetric.instance().remove(meta.getClientId().get());
        }
    }

    public ClientMeta getMeta() {
        return meta;
    }
}
