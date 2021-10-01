package com.oceanbase.clogproxy.obaccess;

import com.oceanbase.clogproxy.obaccess.handshake.OBMysqlAuth;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.common.util.CryptoUtil;
import com.oceanbase.clogproxy.util.Endpoint;
import com.oceanbase.clogproxy.common.util.Hex;
import com.oceanbase.clogproxy.util.HttpAccess;
import com.alipay.oceanbase.obproxy.config.ObGlobalConfig;
import com.alipay.oceanbase.obproxy.datasource.ObGroupDataSource;
import org.apache.commons.collections.CollectionUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-13
 */
public class OBAccess {
    private static final Logger logger = LoggerFactory.getLogger(OBAccess.class);

    private static final String SYS_USER = "oms@sys";
    private static final String SYS_PASS = "oms@sys";

    private static class Singleton {
        private static final OBAccess INSTANCE = new OBAccess();
    }

    public static OBAccess instance() {
        return Singleton.INSTANCE;
    }

    private OBAccess() {}

    public boolean init() {
        // decrypt OB sys password
        logger.info("start to decrypt OB sys user from: {}, {}", Conf.OB_SYS_USERNAME, Conf.OB_SYS_PASSWORD);
        Conf.OB_SYS_USERNAME = CryptoUtil.newEncryptor().decrypt(Hex.toBytes(Conf.OB_SYS_USERNAME));
        Conf.OB_SYS_PASSWORD = CryptoUtil.newEncryptor().decrypt(Hex.toBytes(Conf.OB_SYS_PASSWORD));
        if (Conf.OB_SYS_USERNAME.isEmpty() || Conf.OB_SYS_PASSWORD.isEmpty()) {
            logger.error("failed to decrypt OB sys user, exit !!!");
            return false;
        }
        ObGlobalConfig.getInstance().setCleanLogFileEnabled(true);
        ObGlobalConfig.getInstance().setUseTheSameThreadPool(true);
        return true;
    }

    ObGroupDataSource initConnection(String url, String cluster, String tenant, String username, String password) throws SQLException {
        ObGroupDataSource obDataSource = new ObGroupDataSource();
        obDataSource.setParamURL(url);
        obDataSource.setClustername(cluster);
        obDataSource.setTenantname(tenant);
        obDataSource.setUsername(username);
        obDataSource.setPassword(password);
        obDataSource.setDatabase("oceanbase");
        obDataSource.setGetConnectionTimeout(10000); // 10s
        obDataSource.init();
        return obDataSource;
    }

    /**
     * get tenant server list from oceanbase meta table, available in cloud env only
     */
    private List<Endpoint> fetchUserTenantServer(String clusterUrl, String cluster, String tenant) {
        List<Endpoint> endpoints = new ArrayList<>();
        clusterUrl += "&database=oceanbase";

        ObGroupDataSource ds = null;
        PreparedStatement stat = null;
        try {
            ds = initConnection(clusterUrl, cluster, "sys", Conf.OB_SYS_USERNAME, Conf.OB_SYS_PASSWORD);
            String sql = "SELECT /*+READ_CONSISTENCY(WEAK)*/ svr_ip,sql_port,table_id,role," +
                "part_num,replica_num,schema_version,spare1 " +
                "FROM oceanbase.__all_virtual_proxy_schema " +
                "WHERE tenant_name=? AND database_name='oceanbase' " +
                "AND table_name='__all_dummy' AND sql_port>0 ORDER BY role ASC";
            stat = ds.getConnection().prepareStatement(sql);
            stat.setString(1, tenant);
            ResultSet result = stat.executeQuery();

            while (result.next()) {
                Endpoint endpoint = new Endpoint(result.getString(1), result.getInt(2));
                endpoints.add(endpoint);
            }
            return endpoints;

        } catch (Exception e) {
            logger.info("failed to connect to OBServer: ", e);
            return endpoints;

        } finally {
            try {
                if (stat != null) {
                    stat.close();
                }
                if (ds != null) {
                    ds.close();
                }
            } catch (Exception e) {
                // do nothing
            }
        }
    }

    public boolean authUserTenants(ClientObConf conf) {
        if (!Conf.AUTH_ALLOW_SYS_USER && conf.getUsername().equals(Conf.OB_SYS_USERNAME)) {
            logger.error("OB SYS USER LEAK! client use sys username: {}", conf);
            return false;
        }

        if (conf.getUsername().equals(SYS_USER) && conf.getPassword().equals(SYS_PASS)) {
            logger.info("allow OMS SYS User");
            return true;
        }

        ObRsClusterInfo obRsClusterInfo = HttpAccess.get(conf.getClusterUrl(), ObRsClusterInfo.class);
        if (obRsClusterInfo == null || obRsClusterInfo.getCluster() == null ||
            CollectionUtils.isEmpty(obRsClusterInfo.getCluster().getRootServers())) {
            logger.error("failed to auth client, failed to fetch RootServer from clusterUrl: {}", conf);
            return false;
        }

        for (String tenant : conf.getTenants()) {
            if (!authOneTenant(obRsClusterInfo.getCluster(), conf, tenant)) {
                return false;
            }
            logger.info("succ to auth tenant: {} of {}", tenant, conf.getUsername());
        }
        return true;
    }

    public boolean authOneTenant(ObRsClusterInfo.RsCluster cluster, ClientObConf conf, String tenant) {
        if (Conf.AUTH_PASSWORD_HASH) {
            Endpoint tenantServer = null;
            if (Conf.AUTH_USE_RS) {
                tenantServer = cluster.getRootServers().get(0).getEndpoint();
            } else {
                List<Endpoint> tenantServers = fetchUserTenantServer(conf.getClusterUrl(), cluster.getCluster(), tenant);
                if (CollectionUtils.isEmpty(tenantServers)) {
                    logger.error("failed to auth client, failed to fetch tenant server: {}", conf);
                    return false;
                }
                tenantServer = tenantServers.get(0);
            }

            // * connect to cloud Tenant server: user@tenant
            String username = conf.getUsername() + "@" + tenant;
            byte[] password = Hex.toBytes(conf.getPassword());
            OBMysqlAuth auth = new OBMysqlAuth(tenantServer.getHost(), tenantServer.getPort(), username, password, "");
            if (!auth.auth()) {
                logger.error("failed to auth client: tenant: {}, username: {}, url: {}, tenant server: {}",
                    conf.getTenants(), username, conf.getClusterUrl(), tenantServer);
                return false;
            }
            return true;

        } else {
            ObGroupDataSource ds = null;
            try {
                String connectUrl = conf.getClusterUrl() + "&database=oceanbase";
                ds = initConnection(connectUrl, cluster.getCluster(), tenant, conf.getUsername(), conf.getPassword());
                if (ds == null) {
                    logger.error("failed to auth client, failed to connect to tenant server: {}", conf);
                    return false;
                }
                return true;

            } catch (Exception e) {
                logger.error("failed to auth client, failed to connect to tenant server: {}, ", conf, e);
                return false;
            } finally {
                if (ds != null) {
                    ds.close();
                }
            }
        }
    }
}
