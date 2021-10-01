package com.oceanbase.clogproxy.handler;

import com.oceanbase.clogproxy.common.packet.HeaderType;
import com.oceanbase.clogproxy.common.packet.ProtocolVersion;
import io.netty.channel.ChannelHandlerContext;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-17
 */
public class HandlerDispatcher {

    public static HandlerDispatcher newInstance() {
        return new HandlerDispatcher();
    }

    interface CheckedFunction<T, R> {
        R apply(T t) throws Exception;
    }

    public static <T, R> Function<T, R> wrap(CheckedFunction<T, R> checkedFunction) {
        return t -> {
            try {
                return checkedFunction.apply(t);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        };
    }

    private Map</*Version-HeaderType*/String, Function<ChannelHandlerContext, Boolean>> handlers = new HashMap<>();
    private Map</*Version-HeaderType*/String, Function<ChannelHandlerContext, Boolean>> resets = new HashMap<>();

    public void register(ProtocolVersion version, HeaderType type, Function<ChannelHandlerContext, Boolean> handler,
                         Function<ChannelHandlerContext, Boolean> reset) {
        String key = version.name() + "-" + type.name();
        handlers.put(key, handler);
        resets.put(key, reset);
    }

    /**
     * return ture indicate API parse finished or failed
     *
     * @param type API header type
     * @param ctx  netty context
     */
    public boolean dispatch(ProtocolVersion version, HeaderType type, ChannelHandlerContext ctx) throws Exception {
        String key = version.name() + "-" + type.name();
        if (!handlers.containsKey(key) || !resets.containsKey(key)) {
            // !!! we checked type before, if run here, severe BUG occured
            throw new RuntimeException("!!! disaster inner status occured when dispatcher inner packet");
        }
        try {
            return handlers.get(key).apply(ctx);
        } catch (Exception e) {
            // FIXME... may be exception
            resets.get(key).apply(ctx);
            throw e;
        }
    }
}
