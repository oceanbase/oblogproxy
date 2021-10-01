package com.oceanbase.clogproxy.client.connection;

import java.util.concurrent.ThreadFactory;

import io.netty.channel.EventLoopGroup;
import io.netty.channel.epoll.Epoll;
import io.netty.channel.epoll.EpollEventLoopGroup;
import io.netty.channel.epoll.EpollSocketChannel;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioSocketChannel;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 22:12</p>
 *
 * @author jiyong.jy
 */
public class NettyEventLoopUtil {

    /** check whether epoll enabled, and it would not be changed during runtime. */
    private static boolean epollEnabled = Epoll.isAvailable();

    /**
     * Create the right event loop according to current platform and system property, fallback to NIO when epoll not enabled.
     *
     * @param nThreads
     * @param threadFactory
     * @return an EventLoopGroup suitable for the current platform
     */
    public static EventLoopGroup newEventLoopGroup(int nThreads, ThreadFactory threadFactory) {
        return epollEnabled ? new EpollEventLoopGroup(nThreads, threadFactory)
            : new NioEventLoopGroup(nThreads, threadFactory);
    }

    /**
     * @return a SocketChannel class suitable for the given EventLoopGroup implementation
     */
    public static Class<? extends SocketChannel> getClientSocketChannelClass() {
        return epollEnabled ? EpollSocketChannel.class : NioSocketChannel.class;
    }
}
