package com.oceanbase.clogproxy.metric;

import com.oceanbase.clogproxy.common.util.NetworkUtil;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.util.JsonHint;
import com.aliyuncs.DefaultAcsClient;
import com.aliyuncs.IAcsClient;
import com.aliyuncs.cms.model.v20190101.PutCustomMetricRequest;
import com.aliyuncs.cms.model.v20190101.PutCustomMetricResponse;
import com.aliyuncs.exceptions.ClientException;
import com.aliyuncs.profile.DefaultProfile;
import com.google.common.collect.Lists;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-21
 */
public class Monitor {
    private static final Logger logger = LoggerFactory.getLogger(Monitor.class);

    private static class Singleton {
        private static final Monitor INSTANCE = new Monitor();
    }

    public static Monitor instance() {
        return Monitor.Singleton.INSTANCE;
    }

    private Monitor() { }

    private IAcsClient client;

    public boolean init() {
        if (!Conf.MONINTOR) {
            return true;
        }

        DefaultProfile profile = DefaultProfile.getProfile(Conf.MONINTOR_REGION, Conf.MONINTOR_AK, Conf.MONINTOR_SK);
        client = new DefaultAcsClient(profile);

        return true;
    }

    public void delay(String clientId, long timestamp) {
        rawData("Delay", clientId, timestamp);
    }

    public void rtps(String clientId, long tps) {
        rawData("RTPS", clientId, tps);
    }

    public void wtps(String clientId, long tps) {
        rawData("WTPS", clientId, tps);
    }

    public void rios(String clientId, long tps) {
        rawData("RIOS", clientId, tps);
    }

    public void wios(String clientId, long tps) {
        rawData("WIOS", clientId, tps);
    }

    public void rawData(String name, String clientId, long value) {
        if (!Conf.MONINTOR) {
            return;
        }

        PutCustomMetricRequest.MetricList node = new PutCustomMetricRequest.MetricList();
        node.setGroupId(Conf.MONINTOR_GROUP_ID);
        node.setTime(String.valueOf(System.currentTimeMillis()));
        node.setType("0");
        node.setMetricName("LogProxy_" + name);
        node.setDimensions("{\"ClientId\":\"" + clientId + "\",\"IP\":\"" + NetworkUtil.getLocalIp() + "\"}");
        node.setValues(String.format("{\"value\":%d}", value));

        PutCustomMetricRequest request = new PutCustomMetricRequest();
        request.setSysRegionId(Conf.MONINTOR_REGION);
        request.setSysEndpoint(Conf.MONINTOR_ENDPOINT);
        request.setMetricLists(Lists.newArrayList(node));
        try {
            PutCustomMetricResponse response = client.getAcsResponse(request);
            if (Conf.VERBOSE && !"200".equals(response.getCode())) {
                logger.info("failed to mon {}: {}", name, JsonHint.obj2json(response));
            }
        } catch (ClientException e) {
            if (Conf.VERBOSE) {
                logger.error("failed to mon {}: ", name, e);
            }
        }
    }
}
