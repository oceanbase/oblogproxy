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

#include "obaccess/ob_access.h"
#include "obaccess/ob_mysql_auth.h"
#include "common/common.h"
#include "common/log.h"

namespace oceanbase {
namespace logproxy {

int ObAccess::init(const ServerInfo* servers, int num)
{
  if (nullptr == servers || num <= 0) {
    OMS_ERROR << "Failed to init ObAccess. invalid argument. servers=" << (intptr_t)servers << ", num=" << num;
    return OMS_FAILED;
  }

  for (int i = 0; i < num; i++) {
    const ServerInfo& server = servers[i];
    if (server.host.empty()) {
      OMS_ERROR << "Got server with empty host. server index=" << i;
      return OMS_FAILED;
    }
    if (server.port < 0 || server.port >= 65536) {
      OMS_ERROR << "Got server with invalid port. port=" << server.port << ", server index=" << i;
      return OMS_FAILED;
    }
  }

  _servers.assign(servers, servers + num);
  return OMS_OK;
}

int ObAccess::auth(const char* user, const char* passwd_sha1, int passwd_sha1_size, const char* db) const
{
  if (nullptr == user || 0 == user[0] || nullptr == passwd_sha1 || 0 == passwd_sha1[0] || passwd_sha1_size <= 0) {
    OMS_ERROR << "Cannot auth because of invalid argument.";
    return OMS_FAILED;
  }

  if (nullptr == db) {
    db = "";
  }

  std::vector<char> passwd_sha1_vec(passwd_sha1, passwd_sha1 + passwd_sha1_size);
  for (const auto& server : _servers) {
    ObMysqlAuth auther(server.host, server.port, user, passwd_sha1_vec, db);
    int result = auther.auth();
    if (result != OMS_CONNECT_FAILED) {
      return result;
    }
  }

  return OMS_FAILED;
}

}  // namespace logproxy
}  // namespace oceanbase
