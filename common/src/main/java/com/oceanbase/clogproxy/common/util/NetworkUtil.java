package com.oceanbase.clogproxy.common.util;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.Enumeration;

import io.netty.channel.Channel;
import org.apache.commons.lang3.StringUtils;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 11:33</p>
 *
 * @author jiyong.jy
 */
public class NetworkUtil {

    private static String IP;

    static {
        try {
            for (Enumeration<NetworkInterface> e = NetworkInterface.getNetworkInterfaces(); e.hasMoreElements(); ) {
                NetworkInterface item = e.nextElement();
                for (InterfaceAddress address : item.getInterfaceAddresses()) {
                    if (item.isLoopback() || !item.isUp()) {
                        continue;
                    }
                    if (address.getAddress() instanceof Inet4Address) {
                        Inet4Address inet4Address = (Inet4Address) address.getAddress();
                        IP = inet4Address.getHostAddress();
                        break;
                    }
                }
            }
            if (IP.isEmpty()) {
                IP = InetAddress.getLocalHost().getHostAddress();
            }
        } catch (SocketException | UnknownHostException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * get local ip
     *
     * @return local ip
     */
    public static String getLocalIp() {
        return IP;
    }

    /**
     * Parse the remote address of the channel.
     */
    public static String parseRemoteAddress(final Channel channel) {
        if (null == channel) {
            return StringUtils.EMPTY;
        }
        final SocketAddress remote = channel.remoteAddress();
        return doParse(remote != null ? remote.toString().trim() : StringUtils.EMPTY);
    }

    /**
     * <ol>
     * <li>if an address starts with a '/', skip it.
     * <li>if an address contains a '/', substring it.
     * </ol>
     */
    private static String doParse(String addr) {
        if (StringUtils.isBlank(addr)) {
            return StringUtils.EMPTY;
        }
        if (addr.charAt(0) == '/') {
            return addr.substring(1);
        } else {
            int len = addr.length();
            for (int i = 1; i < len; ++i) {
                if (addr.charAt(i) == '/') {
                    return addr.substring(i + 1);
                }
            }
            return addr;
        }
    }

}
