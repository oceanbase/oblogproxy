/**
 * Copyright (c) 2021 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#pragma once

#include <string>
#include <map>
#include <set>
#include "common/config_base.h"

namespace oceanbase {
namespace logproxy {

struct OblogConfig : protected ConfigBase {
public:
  // client defined params
  OMS_CONFIG_STR(id, "");
  OMS_CONFIG_STR_K(sys_user, "sys_user", "");
  OMS_CONFIG_STR_K(sys_password, "sys_password", "");

  // from here to beflow, params use to send to liboblog
  OMS_CONFIG_UINT64_K(start_timestamp, "first_start_timestamp", 0);

  OMS_CONFIG_STR_K(cluster_url, "cluster_url", "");
  // syntax: rs1:rpc_port1:sql_port1;rs2:rpc_port2:sql_port2
  OMS_CONFIG_STR_K(root_servers, "rootserver_list", "");
  OMS_CONFIG_STR_K(user, "cluster_user", "");
  OMS_CONFIG_STR_K(password, "cluster_password", "");
  OMS_CONFIG_STR_K(table_whites, "tb_white_list", "");

public:
  explicit OblogConfig(const std::string& str);

  void add(const std::string& key, const std::string& value);

  void set(const std::string& key, const std::string& value);

  void generate_configs(std::map<std::string, std::string>& configs) const;

  std::string generate_config_str() const;

  std::string debug_str(bool formatted = false) const;

private:
  std::map<std::string, std::string> _extras;
};

struct DbTable {
  bool all_database;
  std::map<std::string, std::set<std::string>> databases;
};

struct TenantDbTable {
  bool all_tenant;
  std::map<std::string, DbTable> tenants;

  int from(const std::string& table_whites);
};

}  // namespace logproxy
}  // namespace oceanbase
