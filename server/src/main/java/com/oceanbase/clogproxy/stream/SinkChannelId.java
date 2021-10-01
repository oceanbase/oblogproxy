package com.oceanbase.clogproxy.stream;

import io.netty.channel.Channel;
import io.netty.channel.ChannelHandlerContext;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-01
 */
public class SinkChannelId {
    public static String of(Channel channel) {
        return channel.id().asShortText();
    }

    public static String of(ChannelHandlerContext ctx) {
        return of(ctx.channel());
    }
}
