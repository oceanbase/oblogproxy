package com.oceanbase.clogproxy.client;

import com.oceanbase.clogproxy.client.config.AbstractConnectionConfig;
import com.oceanbase.clogproxy.client.config.ClientConf;
import com.oceanbase.clogproxy.client.connection.ClientStream;
import com.oceanbase.clogproxy.client.connection.ConnectionParams;
import com.oceanbase.clogproxy.client.util.ClientIdGenerator;
import com.oceanbase.clogproxy.client.util.Validator;
import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 13:53</p>
 *
 * @author jiyong.jy
 */
public class LogProxyClient {

    private final ClientStream stream;

    /**
     * @param host   server hostname name or ip
     * @param port   server port
     * @param config real config object accroding to what-you-expected
     */
    public LogProxyClient(String host, int port, AbstractConnectionConfig config) {
        LogType logType = Validator.notNull(config.getLogType(), "log type cannot be null");
        host = Validator.notNull(host, "server cannot be null");
        port = Validator.validatePort(port, "port is not valid");
        String clientId = ClientConf.USER_DEFINED_CLIENTID.isEmpty() ? ClientIdGenerator.generate() : ClientConf.USER_DEFINED_CLIENTID;
        ConnectionParams connectionParams = new ConnectionParams(logType, clientId, host, port, config);
        connectionParams.setProtocolVersion(ProtocolVersion.V1);
        this.stream = new ClientStream(connectionParams);
    }

    public void start() {
        stream.start();
    }

    public void stop() {
        stream.stop();
    }

    public void join() {
        stream.join();
    }

    public synchronized void addListener(RecordListener recordListener) {
        stream.addListener(recordListener);
    }

    public synchronized void addStatusListener(StatusListener statusListener) {
        stream.addStatusListener(statusListener);
    }
}
