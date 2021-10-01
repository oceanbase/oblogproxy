package com.oceanbase.clogproxy.stream;

import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Function;

import com.oceanbase.clogproxy.capture.SourcePath;
import com.oceanbase.clogproxy.capture.SourceProcess;
import com.oceanbase.clogproxy.capture.SourceInvokeManager;
import com.oceanbase.clogproxy.capture.SourceInvoke;
import com.oceanbase.clogproxy.channel.ClientDataChannel;
import com.oceanbase.clogproxy.channel.DataChannel;
import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto;
import com.oceanbase.clogproxy.metric.LogProxyMetric;
import com.oceanbase.clogproxy.metric.MetricConstants;
import com.oceanbase.clogproxy.packet.ClientMeta;
import com.oceanbase.clogproxy.packet.Packet;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.util.Inc;
import com.oceanbase.clogproxy.common.util.TaskExecutor;
import io.netty.channel.Channel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-27
 * <p>
 * We call the dataflow 'LogReader -> LogProxy -> Client' a Stream
 * LogReader/StoreClient is SOURCE of LogProxy, Client is SINK of LogProxy
 * <p>
 * There are 4 different IDs coordinate each other:
 * [PID](LogReader) -> [SOURCE_channelID -> SINK_channelID](LogProxy) -> [ClientId](Client)
 * Notice that a LogReader must be bound with a ClientId
 * <p>
 */
public class StreamManager {
    private static final Logger logger = LoggerFactory.getLogger(StreamManager.class);

    AtomicBoolean detectRunFlag = new AtomicBoolean(true);

    private static class Singleton {
        private static final StreamManager INSTANCE = new StreamManager();
    }

    public static StreamManager instance() {
        return Singleton.INSTANCE;
    }

    private StreamManager() {
        ClientMeta dummy = new ClientMeta();
        dummy.setClientId(MetricConstants.METRIC_READONLY_CHANNEL_ID);
        readonlyChannel = new ClientDataChannel(dummy, null, false);
    }

    /**
     * <SourceChannelId, SourceMeta>
     * manipulated when Netty event trigger
     */
    private Map<String, SourceMeta> sources = new ConcurrentHashMap<>();

    /**
     * <ClientId, SourceMeta>
     * source entries by ClientId
     */
    private Map<ClientId, SourceMeta> clientIdSources = new ConcurrentHashMap<>();

    /**
     * <SinkChannelId, ClientMeta>
     * manipulated when Netty event trigger
     */
    private Map<String, ClientMeta> clients = new ConcurrentHashMap<>();

    /**
     * <ClientId, SourceMeta>
     */
    private Map<ClientId, ClientDataChannel> clientChannels = new ConcurrentHashMap<>();

    /**
     * dummpy channel only use for readonly mode
     */
    private DataChannel readonlyChannel;

    public boolean init() {
        TaskExecutor.instance().background(() -> {
            detectLogReaderRoutine();
            return null;
        });
        return true;
    }

    /**
     * stage 1
     * when client handshake income, we stored partial META info(client, SINK_channelID),
     * but the real datastream dosen't established yet
     */
    private void prepareSink(String sinkChannelId, ClientMeta client) {
        ClientMeta meta = clients.get(sinkChannelId);
        if (meta != null) {
            logger.warn("Client proposal already exist: {}", meta);
            return;
        }
        clients.put(sinkChannelId, client);
    }

    private void closeClient(String sinkChannelId, ClientId clientId) {
        // `clients` and `clientChannels` could be inconsistence, so seperated channelId and clientId needed
        ClientMeta meta = clients.get(sinkChannelId);
        if (meta != null) {
            LogProxyMetric.instance().removeRuntimeStatusCallback(meta.getClientId());
            clients.remove(sinkChannelId);
        }

        ClientDataChannel channel = clientChannels.get(clientId);
        if (channel != null) {
            channel.stop();
            clientChannels.remove(clientId);
        }
    }

    /**
     * stage 2
     * try to invoke LogReader(Process) or StoreClient(TODO).
     */
    private void startSourceProcess(LogType logCaptureType, ClientId clientId, String configuration) {
        SourceInvokeManager.getInvoke(logCaptureType).start(clientId.get(), configuration);
    }

    public synchronized boolean createStream(ClientMeta client, Channel channel) {
        String sinkChannelId = SinkChannelId.of(channel);
        try {
            logger.info("Client connecting: {}", client.toString());

            prepareSink(sinkChannelId, client);

            if (clientChannels.containsKey(client.getClientId())) {
                // TODO... response client duplication clientId exist
            }
            final ClientDataChannel clientDataChannel = new ClientDataChannel(client, channel);
            // if here exception, `clients` and `clientChannels` could get inconsistence
            clientChannels.put(client.getClientId(), clientDataChannel);

            startSourceProcess(client.getType(), client.getClientId(), client.getConfiguration());

            if (client.getEnableMonitor()) {
                LogProxyMetric.instance().registerRuntimeStatusCallback(client.getClientId(), status -> {
                    status = status.toBuilder().setStreamCount(clients.size()).setWorkerCount(sources.size()).build();
                    clientDataChannel.pushRuntimeStatus(status);
                    return 0;
                });
                logger.info("Client registered Monitor: {}", client.getClientId());
            }

            logger.info("Client connected: {}", client.getClientId());
            return true;
        } catch (Exception e) {
            closeClient(sinkChannelId, client.getClientId());
            return false;
        }
    }

    /**
     * stage 3
     * logreader connected, and connection established success which means stage 1 and 2 above succeed.
     * Notice that this method work as a async "callback" trigger by another network connection,
     * and it's possible any failure occured whitout trigger this callback,
     * so we need a detect thread to do some GC to cleanup failure(timeout) client metas.
     * see {@code registerLogReader}
     */
    public synchronized boolean registerLogReader(SourceMeta logreader) {
        logger.info("LogReader incoming: {}", logreader.toString());

        String sourceChannelId = logreader.getId();
        // check if there exist ClientChannel with ClientId
        DataChannel channel = Conf.READONLY ? readonlyChannel : clientChannels.get(logreader.getClientId());
        if (channel == null) {
            logger.error("failed to register LogReader, no Client Channel with ClientId: {}", logreader.getClientId());
            return false;
        }

        SourceMeta meta = sources.get(sourceChannelId);
        if (meta != null) {
            logger.error("failed to register LogReader, Source already exist: {}", meta.toString());
            return false;
        }

        sources.put(sourceChannelId, logreader);
        clientIdSources.put(logreader.getClientId(), logreader);
        logger.info("LogReader registered: {}", logreader.toString());
        return true;
    }

    public boolean push(ClientId clientid, Packet packet) {
        try {
            if (Conf.READONLY) {
                readonlyChannel.start();
                readonlyChannel.push(packet);
                return true;
            }
            ClientDataChannel channel = clientChannels.get(clientid);
            if (channel == null) {
                logger.error("Not Exist dataChannel of ClientId: {}", clientid.toString());
                packet.release(); // if exception occured here, packet not release.
                return false;
            }
            channel.push(packet);
            return true;
        } catch (Exception e) {
            packet.release(); // if exception occured here, packet not release.
            return false;
        }
    }

    /**
     * mark LogReader alive time
     */
    public void check(Channel channel) {
        SourceMeta source = sources.get(SourceChannelId.of(channel));
        if (source != null) {
            // TODO... maybe atomic CAS needed in case of Source is stoped and released at this moment?
            source.setLastTime(new Date());
        }
    }

    public synchronized void stopStreamByClient(Channel channel) {
        stopStreamByClient(SinkChannelId.of(channel));
    }

    public synchronized void stopStreamByClient(String sinkChannelId) {
        ClientMeta clientMeta = clients.get(sinkChannelId);
        if (clientMeta == null) {
            logger.warn("not exist client meta of sinkChannelId: {}", sinkChannelId);
            closeClient(sinkChannelId, ClientId.EMPTY);
            return;
        }

        SourceMeta sourceMeta = clientIdSources.get(clientMeta.getClientId());
        if (sourceMeta == null) {
            logger.warn("not exist source meta of ClientMeta: {}", clientMeta.toString());
            closeClient(sinkChannelId, clientMeta.getClientId());
            return;
        }

        // stop source first
        SourceInvokeManager.getInvoke(sourceMeta.getType()).stop(SourceProcess.of(sourceMeta));

        sources.remove(sourceMeta.getId());
        clientIdSources.remove(sourceMeta.getClientId());

        closeClient(sinkChannelId, sourceMeta.getClientId());
    }

    public synchronized void stopStreamByLogReader(Channel channel) {
        stopStreamByLogReader(SourceChannelId.of(channel));
    }

    public synchronized void stopStreamByLogReader(String sourceChannelId) {
        SourceMeta sourceMeta = sources.get(sourceChannelId);
        if (sourceMeta == null) {
            logger.warn("not exist source meta of sourceChannelId: {}", sourceChannelId);
            return;
        }

        SourceInvokeManager.getInvoke(sourceMeta.getType()).stop(SourceProcess.of(sourceMeta));

        ClientId clientId = sourceMeta.getClientId();
        LogProxyMetric.instance().removeRuntimeStatusCallback(clientId);
        sources.remove(sourceMeta.getId());
        clientIdSources.remove(clientId);

        if (Conf.READONLY) {
            readonlyChannel.stop();
            return;
        }

        ClientDataChannel clientDataChannel = clientChannels.get(clientId);
        if (clientDataChannel == null) {
            logger.warn("Not Exist client channel of SourceMeta: {}", sourceMeta);
            return;
        }

        clientDataChannel.stop();
        clients.remove(clientDataChannel.getMeta().getId());
        clientChannels.remove(clientId);
    }

    private void detectLogReaderRoutine() {
        logger.info("###### start LOGREADER_DETECT_ROUTINE");

        SourceInvoke invoke = SourceInvokeManager.getInvoke(LogType.OCEANBASE);
        while (detectRunFlag.get()) {
            Inc.sleep(Conf.DETECT_INTERVAL_S * 1000L);

            // 1. Detect dead LogReader
            for (Map.Entry<String, SourceMeta> source : sources.entrySet()) {
                long delta = (System.currentTimeMillis() - source.getValue().getLastTime().getTime()) / 1000;
                if (delta >= Conf.OB_LOGREADER_LEASE_S) {
                    logger.warn("###### LogReader lease timeout, KILL, timeout: {}, {}", delta, source);
                    stopStreamByLogReader(source.getKey());
                }
            }

            // 2. Detect clients exit but without LogReader, we treat this as initial failure
            for (Map.Entry<String, ClientMeta> client : clients.entrySet()) {
                long delta = (System.currentTimeMillis() - client.getValue().getRegisterTime().getTime()) / 1000;
                ClientId clientId = client.getValue().getClientId();
                if (delta >= Conf.CLIENT_INIT_TIMEOUT_S && !clientIdSources.containsKey(clientId)) {
                    logger.warn("###### LogReader init timeout, stop client, timeout: {}, {}", delta, client);

                    ClientDataChannel channel = clientChannels.get(clientId);
                    if (channel != null) {
                        // TODO... write stop packet to client
                    }
                    stopStreamByClient(client.getKey());
                }
            }

            // 3. Check wild-LogReader and kill them all
            List<SourceProcess> logreaderProcesses = invoke.listProcesses();
            for (SourceProcess process : logreaderProcesses) {
                boolean registered = false;
                for (SourceMeta source : sources.values()) {
                    if (source.getPid().equals(process.getPid())) {
                        registered = true;
                        break;
                    }
                }
                if (registered) {
                    continue;
                }
                long delta = (System.currentTimeMillis() / 1000) - process.getStartTime();
                if (delta < Conf.CLIENT_INIT_TIMEOUT_S) {
                    continue;
                }
                logger.warn("###### Wild LogReader process detected, stop: {}", process);
                invoke.stop(process);
            }

            // 4. Check empty-LogReader path and do clean
            List<SourcePath> logreaderPaths = invoke.listPaths();
            for (SourcePath path : logreaderPaths) {
                if (clientIdSources.containsKey(ClientId.of(path.getClientId()))) {
                    // ignore running logreader
                    continue;
                }
                if ((System.currentTimeMillis() / 1000) - path.getLastTime() >= (Conf.OB_LOGREADER_PATH_RETAIN_HOUR * 3600L)) {
                    logger.warn("###### Stale LogReader path detected, delete: {}", path);
                    invoke.clean(path);
                }
            }
        }

        logger.info("###### end LOGREADER_DETECT_ROUTINE");
    }
}
