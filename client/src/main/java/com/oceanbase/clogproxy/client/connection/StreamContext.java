package com.oceanbase.clogproxy.client.connection;

import com.oceanbase.clogproxy.client.config.ClientConf;
import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.oms.record.oms.OmsRecord;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import static com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto.RuntimeStatus;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-06
 * <p>
 * connection parameters, connection ,transit data which communicate with Netty
 */
public class StreamContext {
    public static class TransferPacket {
        private HeaderType type;
        private OmsRecord record;
        private RuntimeStatus status;

        public TransferPacket(OmsRecord record) {
            this.type = HeaderType.DATA_CLIENT;
            this.record = record;
        }

        public TransferPacket(RuntimeStatus status) {
            this.type = HeaderType.STATUS;
            this.status = status;
        }

        public HeaderType getType() {
            return type;
        }

        public OmsRecord getRecord() {
            return record;
        }

        public RuntimeStatus getStatus() {
            return status;
        }
    }

    private final BlockingQueue<TransferPacket> recordQueue = new LinkedBlockingQueue<>(ClientConf.TRANSFER_QUEUE_SIZE);

    private ClientStream stream;
    ConnectionParams params;

    public StreamContext(ClientStream stream, ConnectionParams params) {
        this.stream = stream;
        this.params = params;
    }

    public ConnectionParams getParams() {
        return params;
    }

    public ClientStream stream() {
        return stream;
    }

    public BlockingQueue<TransferPacket> recordQueue() {
        return recordQueue;
    }
}
