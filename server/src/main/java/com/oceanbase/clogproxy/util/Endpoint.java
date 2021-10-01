package com.oceanbase.clogproxy.util;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-15
 */
public class Endpoint {
    private String host;
    private int port;

    public Endpoint(String host, int port) {
        this.host = host;
        this.port = port;
    }

    public Endpoint(String endpointStr) {
        String[] sections = endpointStr.split(":");
        if (sections.length > 0) {
            host = sections[0];
        }
        if (sections.length > 1) {
            try {
                port = Integer.parseInt(sections[1]);
            } catch (NumberFormatException e) {
                port = 0;
            }
        }
    }

    @Override
    public String toString() {
        return host + ':' + port;
    }

    public String getHost() {
        return host;
    }

    public void setHost(String host) {
        this.host = host;
    }

    public int getPort() {
        return port;
    }

    public void setPort(int port) {
        this.port = port;
    }

}
