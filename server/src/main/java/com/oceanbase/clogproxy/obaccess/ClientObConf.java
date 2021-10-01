package com.oceanbase.clogproxy.obaccess;

import com.oceanbase.clogproxy.common.util.Password;
import com.oceanbase.clogproxy.util.Conf;
import com.google.common.base.Splitter;
import com.google.common.collect.Sets;
import org.apache.commons.collections.CollectionUtils;
import org.apache.commons.lang3.StringUtils;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

import java.util.List;
import java.util.Set;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-14
 */
public class ClientObConf {
    private static Splitter splitter = Splitter.on(' ').omitEmptyStrings().trimResults();
    private static Splitter splitterKV = Splitter.on('=').omitEmptyStrings().trimResults().limit(2);
    private static Splitter splitterTenant = Splitter.on('.').omitEmptyStrings().trimResults();
    private static Splitter splitterMulti = Splitter.on('|').omitEmptyStrings().trimResults();

    private String username;
    private Password password = new Password();
    private String clusterUrl;
    private Set<String> tenants = Sets.newHashSet();

    enum Keys {
        /**
         * connect username
         */
        CLUSTER_USER("cluster_user"),
        CLUSTER_PASSWORD("cluster_password"),
        CLUSTER_URL("cluster_url"),
        TB_WHITE_LIST("tb_white_list");

        Keys(String key) {
            this.key = key;
        }

        public String key;
    }

    /**
     * cluster_url=http://xxxxx/aa/bb/cc cluster_user=username cluster_password=password tb_white_list=tenant.db.table otherK=otherV
     */
    public static String fromConfiguration(String configuration, ClientObConf conf) {
        List<String> sections = splitter.splitToList(configuration);
        if (sections.isEmpty()) {
            return "";
        }
        StringBuilder sb = new StringBuilder();
        for (String section : sections) {
            List<String> kv = splitterKV.splitToList(section);
            if (kv.size() != 2) {
                continue;
            }
            String k = kv.get(0);
            sb.append(k).append("=");

            String v = kv.get(1);
            if (Keys.CLUSTER_USER.key.equals(k)) {
                sb.append(Conf.OB_SYS_USERNAME);
                conf.username = v;
            } else if (Keys.CLUSTER_PASSWORD.key.equals(k)) {
                sb.append(Conf.OB_SYS_PASSWORD);
                conf.password.set(v);
            } else if (Keys.CLUSTER_URL.key.equals(k)) {
                sb.append(v);
                conf.clusterUrl = v;
            } else if (Keys.TB_WHITE_LIST.key.equals(k)) { // multiple tenant
                sb.append(v);

                List<String> multis = splitterMulti.splitToList(v);
                for (String multi : multis) {
                    List<String> tenants = splitterTenant.splitToList(multi);
                    if (tenants.size() > 1 && !tenants.get(0).equals("*")) {
                        conf.tenants.add(tenants.get(0));
                    }
                }
            } else {
                sb.append(v);
            }
            sb.append(' ');
        }
        if (StringUtils.isEmpty(conf.username) || StringUtils.isEmpty(conf.password.get()) ||
                StringUtils.isEmpty(conf.clusterUrl)) {
            return "";
        }
        if (!Conf.ALLOW_ALL_TENANT && CollectionUtils.isEmpty(conf.tenants)) {
            return "";
        }
        return sb.toString();
    }

    public ClientObConf() { }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
    }

    public String getUsername() {
        return username;
    }

    public String getPassword() {
        return password.get();
    }

    public String getClusterUrl() {
        return clusterUrl;
    }

    public Set<String> getTenants() {
        return tenants;
    }
}
