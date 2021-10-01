package com.oceanbase.clogproxy.obaccess;

import com.oceanbase.clogproxy.util.Endpoint;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import org.apache.commons.lang3.builder.ReflectionToStringBuilder;
import org.apache.commons.lang3.builder.ToStringStyle;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-14
 * <p>
 * ClusterUrl json pattern:
 * <pre>
 * {
 *   "Code": 200,
 *   "Cost": 7,
 *   "Data": {
 *     "ObCluster": "tenant",
 *     "Type": "PRIMARY",
 *     "ObRegionId": 111111111,
 *     "ObClusterId": 111111111,
 *     "RsList": [
 *       {
 *         "sql_port": 2881,
 *         "address": "127.0.0.1:2882",
 *         "role": "LEADER"
 *       },
 *       {
 *         "sql_port": 2881,
 *         "address": "127.0.0.1:2882",
 *         "role": "FOLLOWER"
 *       },
 *       {
 *         "sql_port": 2881,
 *         "address": "127.0.0.1:2882",
 *         "role": "FOLLOWER"
 *       }
 *     ],
 *     "ReadonlyRsList": [],
 *     "ObRegion": "tenant",
 *     "timestamp": 1594748166586751
 *   },
 *   "Message": "successful",
 *   "Success": true,
 *   "Trace": "xxxxxxxxxxxxxxxxxxxxxx"
 * }
 * </pre>
 */
@JsonIgnoreProperties(ignoreUnknown = true)
public class ObRsClusterInfo {
    @JsonProperty("Code")
    private Integer code;

    @JsonProperty("Success")
    private Boolean success;

    @JsonProperty("Message")
    private String message;

    @JsonProperty("Data")
    private RsCluster cluster;

    @JsonIgnoreProperties(ignoreUnknown = true)
    public static class RsCluster {
        @JsonProperty("ObCluster")
        private String cluster;

        @JsonProperty("RsList")
        private List<RootServer> rootServers;

        @Override
        public String toString() {
            return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
        }

        public String getCluster() {
            return cluster;
        }

        public List<RootServer> getRootServers() {
            return rootServers;
        }
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    public static class RootServer {
        @JsonProperty("address")
        private String endpointStr;

        @JsonProperty("sql_port")
        private Integer port;

        private Endpoint endpoint;
        private AtomicBoolean converted = new AtomicBoolean(false);

        @Override
        public String toString() {
            return endpointStr;
        }

        public Endpoint getEndpoint() {
            if (converted.compareAndSet(false, true)) {
                endpoint = new Endpoint(endpointStr);
                endpoint.setPort(port);
            }
            return endpoint;
        }
    }

    @Override
    public String toString() {
        return ReflectionToStringBuilder.toString(this, ToStringStyle.SHORT_PREFIX_STYLE);
    }

    public Integer getCode() {
        return code;
    }

    public Boolean getSuccess() {
        return success;
    }

    public String getMessage() {
        return message;
    }

    public RsCluster getCluster() {
        return cluster;
    }
}
