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
#include "obaccess/ob_mysql_packet.h"
#include "obaccess/ob_sha1.h"

namespace oceanbase {
namespace logproxy {

/**
 * 以代理的身份对客户端做鉴权
 * 拿到使用sha1加密后的password，根据鉴权信息，向指定OceanBase服务器发起鉴权
 * 鉴权协议参考
 * https://dev.mysql.com/doc/internals/en/secure-password-authentication.html#packet-Authentication::Native41
 * NOTE: 这里有个缺陷，不能对用户的IP地址做校验
 * NOTE: 使用明文保存sha1加密后的密码，仍然不安全
 */
class MysqlProtocol {
public:
  ~MysqlProtocol();

  void close();

  int login(const std::string& host, int port, const std::string& username, const std::string& passwd_sha1,
      const std::string& database = "");

  int query(const std::string& sql, MysqlResultSet& rs);

  /**
   * 设置接收网络消息包时，首次检测是否有消息到达的超时时间。单位毫秒
   */
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

  int calc_mysql_auth_info(const std::vector<char>& scramble_buffer, std::vector<char>& auth_info);

  int send_auth_info(const std::vector<char>& auth_info, uint8_t sequence);

  int is_mysql_response_ok();

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
