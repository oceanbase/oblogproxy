package com.oceanbase.clogproxy.server;

import com.oceanbase.clogproxy.handler.LogProxyHandler;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-07 Time: 11:29</p>
 *
 * @author jiyong.jy
 */
public class LogProxyServer {
    private static Logger logger = LoggerFactory.getLogger(LogProxyServer.class);

    private static class Singleton {
        private static final LogProxyServer instance = new LogProxyServer();
    }

    public static LogProxyServer instance() {
        return LogProxyServer.Singleton.instance;
    }

    public void start() throws Exception {
        NioEventLoopGroup bossLoopGroup = new NioEventLoopGroup(Conf.NETTY_ACCEPT_THREADPOOL_SIZE);
        NioEventLoopGroup workLoopGroup = new NioEventLoopGroup(Conf.NETTY_WORKER_THREADPOOL_SIZE);

        try {
            ServerBootstrap serverBootstrap = new ServerBootstrap();
            serverBootstrap.group(bossLoopGroup, workLoopGroup)
                .channel(NioServerSocketChannel.class)
                .childHandler(new ChannelInitializer<SocketChannel>() {
                    @Override
                    protected void initChannel(SocketChannel ch) {
                        ch.pipeline().addLast(LogProxyHandler.newInstance());
                    }

                })
                .option(ChannelOption.SO_BACKLOG, 128)
                .childOption(ChannelOption.SO_KEEPALIVE, true);

            ChannelFuture channelFuture = serverBootstrap.bind(Conf.PROXY_SERVICE_PORT).sync();
            logger.info("#### LogProxy server start with port: {}", Conf.PROXY_SERVICE_PORT);
            channelFuture.channel().closeFuture().sync(); // blocking utill close
        } finally {
            bossLoopGroup.shutdownGracefully();
            workLoopGroup.shutdownGracefully();
            logger.info("#### LogProxy server end");
        }
    }
}
