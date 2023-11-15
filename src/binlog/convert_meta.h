/**
 * Copyright (c) 2023 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OMS_LOGPROXY_CONVERT_META_H
#define OMS_LOGPROXY_CONVERT_META_H

#include "model.h"
#include "codec/message.h"

namespace oceanbase {
namespace logproxy {

class ConvertMeta : public Model {
  OMS_MF_ENABLE_COPY(ConvertMeta);

  OMS_MF(std::string, log_bin_prefix);
  OMS_MF(std::string, server_uuid);
  OMS_MF_DFT(int, _fd, -1);
  OMS_MF_DFT(int64_t, send_binlog_interval_ms, 100);
  OMS_MF_DFT(uint64_t, first_start_timestamp, 0);
  OMS_MF_DFT(std::string, white_list, "");
  OMS_MF_DFT(std::string, cluster_url, "");
  OMS_MF_DFT(std::string, sys_user, "");
  OMS_MF_DFT(std::string, sys_password, "");
  OMS_MF_DFT(std::string, cluster, "");
  OMS_MF_DFT(std::string, tenant, "");

  OMS_MF_DFT(uint32_t, max_binlog_size_bytes, 1024 * 1024 * 500);  // 500MB

public:
  ConvertMeta() = default;

  int init_from_json(const rapidjson::Value& json)
  {
    log_bin_prefix = json["log_bin_prefix"].GetString();
    server_uuid = json["server_uuid"].GetString();
    _fd = json["_fd"].GetInt();
    send_binlog_interval_ms = json["send_binlog_interval_ms"].GetInt64();
    first_start_timestamp = json["first_start_timestamp"].GetUint64();
    white_list = json["white_list"].GetString();
    cluster_url = json["cluster_url"].GetString();
    sys_user = json["sys_user"].GetString();
    sys_password = json["sys_password"].GetString();
    cluster = json["cluster"].GetString();
    tenant = json["tenant"].GetString();
    max_binlog_size_bytes = json["max_binlog_size_bytes"].GetUint();
    return OMS_OK;
  }

  void to_json(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const
  {
    writer.Key(CONVERT_META);
    writer.StartObject();
    writer.Key("log_bin_prefix");writer.String(log_bin_prefix.c_str());
    writer.Key("server_uuid");writer.String(server_uuid.c_str());
    writer.Key("_fd");writer.Int(_fd);
    writer.Key("send_binlog_interval_ms");writer.Int64(send_binlog_interval_ms);
    writer.Key("first_start_timestamp");writer.Uint64(first_start_timestamp);
    writer.Key("white_list");writer.String(white_list.c_str());
    writer.Key("cluster_url");writer.String(cluster_url.c_str());
    writer.Key("sys_user");writer.String(sys_user.c_str());
    writer.Key("sys_password");writer.String(sys_password.c_str());
    writer.Key("cluster");writer.String(cluster.c_str());
    writer.Key("tenant");writer.String(tenant.c_str());
    writer.Key("max_binlog_size_bytes");writer.Uint(max_binlog_size_bytes);
    writer.EndObject();
  }

  const string& to_string() const override
  {
    return Model::to_string();
  }
};

}  // namespace logproxy
}  // namespace oceanbase

#endif  // OMS_LOGPROXY_CONVERT_META_H
