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
#include <vector>
#include <unordered_set>
#include "obaccess/oblog_config.h"

namespace oceanbase {
namespace logproxy {

class ObAccess {
public:
  struct ServerInfo {
    std::string host;
    int port;
  };

public:
  ObAccess() = default;
  ~ObAccess() = default;

  int init(const OblogConfig&);

  /**
   * 按照提供的ob server集群，校验客户端发来的用户名和密码，是否合法
   */
  int auth();

private:
  int auth_sys(const ServerInfo&);

  int auth_tenant(const ServerInfo&);

private:
  std::vector<ServerInfo> _servers;
  std::string _user;
  std::string _user_to_conn;
  std::string _password_sha1;
  std::string _sys_user;
  std::string _sys_password_sha1;

  // TODO... database table auth
  TenantDbTable _table_whites;
};

struct ObUsername {
  std::string cluster;
  std::string tenant;
  std::string username;

  explicit ObUsername(const std::string& full_name);

  std::string name_without_cluster(const std::string& in_tenant = "");
};

}  // namespace logproxy
}  // namespace oceanbase
