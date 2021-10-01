package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.obaccess.ObRsClusterInfo;
import com.oceanbase.clogproxy.util.HttpAccess;
import org.junit.Assert;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-14
 */
public class HttpAccessTest {
    private static final Logger logger = LoggerFactory.getLogger(HttpAccessTest.class);

    String url1 = "https://www.baidu.com";
    String url2 = "http://obconsole.test.alibaba-inc.com/ocp-api/services?Action=ObRootServiceInfo&User_ID=alibaba&UID=alibaba&ObRegion=obvip_cp_1";

    @Test
    public void testGet() {
        String payloadBody = HttpAccess.get(url1, String.class);
        logger.info("Http GET: {}, response body: {}", url1, payloadBody);

        payloadBody = HttpAccess.get(url2, String.class);
        logger.info("Http GET: {}, response body: {}", url2, payloadBody);

        ObRsClusterInfo obRsCluster = HttpAccess.get(url2, ObRsClusterInfo.class);
        logger.info("Http GET: {}, response body: {}", url2, obRsCluster);

        Assert.assertNotNull(obRsCluster);
        Assert.assertNotNull(obRsCluster.getCluster());
        for (ObRsClusterInfo.RootServer rs : obRsCluster.getCluster().getRootServers()) {
            logger.info("root server: {}", rs.getEndpoint());
        }
    }
}
