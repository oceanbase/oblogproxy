package com.oceanbase.clogproxy.server;

import com.oceanbase.clogproxy.handler.LogReaderHandler;
import com.oceanbase.clogproxy.util.Conf;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.util.ResourceLeakDetector;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 14:29</p>
 *
 * @author jiyong.jy
 */
public class LogReaderServer {
    private static Logger logger = LoggerFactory.getLogger(LogReaderServer.class);

    private static class Singleton {
        private static final LogReaderServer INSTANCE = new LogReaderServer();
    }

    public static LogReaderServer instance() {
        return Singleton.INSTANCE;
    }

    public void start() throws Exception {
        if (Conf.ADVANCED_LEAK_DETECT) {
            ResourceLeakDetector.setLevel(ResourceLeakDetector.Level.ADVANCED);
        }

        NioEventLoopGroup bossLoopGroup = new NioEventLoopGroup(Conf.NETTY_ACCEPT_THREADPOOL_SIZE);
        NioEventLoopGroup workLoopGroup = new NioEventLoopGroup(Conf.NETTY_WORKER_THREADPOOL_SIZE);

        try {
            ServerBootstrap serverBootstrap = new ServerBootstrap();
            serverBootstrap.group(bossLoopGroup, workLoopGroup)
                    .channel(NioServerSocketChannel.class)
                    .childHandler(new ChannelInitializer<SocketChannel>() {
                        @Override
                        protected void initChannel(SocketChannel ch) {
                            ch.pipeline().addLast(LogReaderHandler.newInstance());
                        }
                    })
                    .option(ChannelOption.SO_BACKLOG, 128)
                    .childOption(ChannelOption.SO_KEEPALIVE, true);

            logger.info("log capturer server start with port: {}", Conf.CAPTURE_SERVICE_PORT);
            ChannelFuture channelFuture = serverBootstrap.bind(Conf.CAPTURE_SERVICE_PORT).sync();
            channelFuture.channel().closeFuture().sync();
        } finally {
            bossLoopGroup.shutdownGracefully();
            workLoopGroup.shutdownGracefully();
            logger.info("log capturer server end");
        }
    }
}
