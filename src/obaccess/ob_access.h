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

namespace oceanbase {
namespace logproxy {

/**
 * 按照提供的ob server集群，校验客户端发来的用户名和密码，是否合法
 */
class ObAccess {
public:
  struct ServerInfo {
    std::string host;
    int port;

    ServerInfo(const char* host_, int port_) : host(host_), port(port_)
    {}
  };

public:
  ObAccess() = default;
  ~ObAccess() = default;

  /**
   * @param servers 服务器地址列表，格式 ip:port,ip1:port1
   */
  int init(const ServerInfo* servers, int num);
  int auth(const char* user, const char* passwd_sha1, int passwd_sha1_size, const char* db) const;

private:
  std::vector<ServerInfo> _servers;
};

}  // namespace logproxy
}  // namespace oceanbase
