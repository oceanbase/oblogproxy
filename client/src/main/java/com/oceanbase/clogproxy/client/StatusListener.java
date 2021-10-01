package com.oceanbase.clogproxy.client;

import com.oceanbase.clogproxy.common.packet.protocol.LogProxyProto;

/**
 * @author Fankux(yuqi.fy)
 * @since 2021-01-11
 */
public interface StatusListener {
    void notify(LogProxyProto.RuntimeStatus status);
}
