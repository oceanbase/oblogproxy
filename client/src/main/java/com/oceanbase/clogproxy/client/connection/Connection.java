package com.oceanbase.clogproxy.client.connection;

import java.util.concurrent.atomic.AtomicBoolean;

import com.oceanbase.clogproxy.common.util.NetworkUtil;
import io.netty.channel.Channel;
import io.netty.util.concurrent.Future;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 14:20</p>
 *
 * @author jiyong.jy
 */
public class Connection {

    private static final Logger logger = LoggerFactory.getLogger(Connection.class);

    private Channel channel;

    private final AtomicBoolean closed = new AtomicBoolean(false);

    public Connection(Channel channel) {
        this.channel = channel;
    }

    public void close() {
        if (!closed.compareAndSet(false, true)) {
            logger.warn("connection already closed");
        }
        if (channel != null) {
            if (channel.isActive()) {
                try {
                    channel.close().addListener(this::logCloseResult).syncUninterruptibly();
                } catch (Exception e) {
                    logger.warn("close connection to remote address {} exception",
                            NetworkUtil.parseRemoteAddress(channel), e);
                }
            }
            channel = null;
        }
    }

    private void logCloseResult(Future future) {
        if (future.isSuccess()) {
            if (logger.isInfoEnabled()) {
                logger.info("close connection to remote address {} success", NetworkUtil.parseRemoteAddress(channel));
            }
        } else {
            logger.warn("close connection to remote address {} fail", NetworkUtil.parseRemoteAddress(channel), future.cause());
        }
    }
}
