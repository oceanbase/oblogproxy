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
#include "ob_mysql_packet.h"
#include "ob_sha1.h"

namespace oceanbase {
namespace logproxy {
/**
 * see: https://dev.mysql.com/doc/internals/en/secure-password-authentication.html#packet-Authentication::Native41
 */
class MysqlProtocol {
public:
  ~MysqlProtocol();

  void close();

  int login(const std::string& host, int port, const std::string& username, const std::string& passwd_sha1,
      const std::string& database = "");

  int query(const std::string& sql, MySQLResultSet& rs);

  inline void set_detect_timeout(int t)
  {
    _detect_timeout = t;
  }

  template <typename T = std::string>
  static int do_sha_password(const std::string& pswd, T& sha_password)
  {
    SHA1 sha1;
    sha1.input((const unsigned char*)pswd.c_str(), pswd.size());
    sha_password.resize(SHA1::SHA1_HASH_SIZE);
    return sha1.get_result((unsigned char*)sha_password.data());
  }

private:
  int connect_to_server();

  int calc_mysql_auth_info(const std::vector<char>& scramble, std::vector<char>& auth);

  int send_auth(const std::vector<char>& auth_info, const std::string& database);

private:
  std::string _username;
  std::string _passwd_sha1;
  std::string _hostname;
  int _port = -1;
  int _sockfd = -1;

  /// @see set_detect_timeout
  int _detect_timeout = 10000;  // in millisecond
};

}  // namespace logproxy
}  // namespace oceanbase
