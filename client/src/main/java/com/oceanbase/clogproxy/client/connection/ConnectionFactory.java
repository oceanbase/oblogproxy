package com.oceanbase.clogproxy.client.connection;

import java.net.InetSocketAddress;

import com.oceanbase.clogproxy.client.ErrorCode;
import com.oceanbase.clogproxy.client.LogProxyClientException;
import com.oceanbase.clogproxy.client.config.ClientConf;
import io.netty.bootstrap.Bootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.handler.timeout.IdleStateHandler;
import io.netty.util.AttributeKey;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 10:07</p>
 *
 * @author jiyong.jy
 */
public class ConnectionFactory {

    private static class Singleton {
        private static final ConnectionFactory INSTANCE = new ConnectionFactory();
    }

    public static ConnectionFactory instance() {
        return Singleton.INSTANCE;
    }

    private ConnectionFactory() {
        this.bootstrap = initBootstrap();
    }

    public static final AttributeKey<StreamContext> CONTEXT_KEY = AttributeKey.valueOf("context");

    private static final EventLoopGroup WORKER_GROUP = NettyEventLoopUtil.newEventLoopGroup(1,
            new NamedThreadFactory("log-proxy-client-worker", true));

    private Bootstrap bootstrap;

    private Bootstrap initBootstrap() {
        Bootstrap bootstrap = new Bootstrap();
        bootstrap.group(WORKER_GROUP).channel(NettyEventLoopUtil.getClientSocketChannelClass())
                .option(ChannelOption.TCP_NODELAY, true)
                .option(ChannelOption.SO_KEEPALIVE, true);

        bootstrap.handler(new ChannelInitializer<SocketChannel>() {
            @Override
            protected void initChannel(SocketChannel ch) {
                ch.pipeline().addLast(new IdleStateHandler(ClientConf.IDLE_TIMEOUT_S, 0, 0));
                ch.pipeline().addLast(new ClientHandler());
            }
        });
        return bootstrap;
    }

    public Connection createConnection(String host, int port, StreamContext context) throws LogProxyClientException {
        this.bootstrap = initBootstrap(); // TODO... Is there connection leak possible?
        bootstrap.option(ChannelOption.CONNECT_TIMEOUT_MILLIS, ClientConf.CONNECT_TIMEOUT_MS);
        ChannelFuture channelFuture = bootstrap.connect(new InetSocketAddress(host, port));
        channelFuture.channel().attr(CONTEXT_KEY).set(context);
        channelFuture.awaitUninterruptibly();
        if (!channelFuture.isDone()) {
            throw new LogProxyClientException(ErrorCode.E_CONNECT, "timeout of create connection!");
        }
        if (channelFuture.isCancelled()) {
            throw new LogProxyClientException(ErrorCode.E_CONNECT, "cancelled by user of create connection!");
        }
        if (!channelFuture.isSuccess()) {
            throw new LogProxyClientException(ErrorCode.E_CONNECT, "failed to create connection!", channelFuture.cause());
        }
        return new Connection(channelFuture.channel());
    }
}
