/**
 * Copyright (c) 2024 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include <string>
#include <httplib.h>

namespace oceanbase {
namespace logproxy {

class HttpPostClient {
public:
  explicit HttpPostClient(const std::string& host);
  void set_connection_timeout(int seconds, int microseconds = 0);
  void set_read_timeout(int seconds, int microseconds = 0);
  void set_write_timeout(int seconds, int microseconds = 0);
  bool post(const std::string& path, const std::string& body, std::string& response_body,
      const std::string& content_type = "text/plain");

private:
  httplib::Client client_;
};

}  // namespace logproxy
}  // namespace oceanbase
