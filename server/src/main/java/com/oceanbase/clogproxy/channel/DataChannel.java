package com.oceanbase.clogproxy.channel;

import com.oceanbase.clogproxy.packet.Packet;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 11:18</p>
 *
 * @author jiyong.jy
 */
public interface DataChannel {

    void start();

    void stop();

    void push(Packet packet);

}
