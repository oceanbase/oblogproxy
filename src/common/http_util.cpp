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

#include "http_util.h"

namespace oceanbase {
namespace logproxy {

HttpPostClient::HttpPostClient(const std::string& host) : client_(host)
{}

void HttpPostClient::set_connection_timeout(int seconds, int microseconds)
{
  client_.set_connection_timeout(seconds, microseconds);
}

void HttpPostClient::set_read_timeout(int seconds, int microseconds)
{
  client_.set_read_timeout(seconds, microseconds);
}

void HttpPostClient::set_write_timeout(int seconds, int microseconds)
{
  client_.set_write_timeout(seconds, microseconds);
}

bool HttpPostClient::post(
    const std::string& path, const std::string& body, std::string& response_body, const std::string& content_type)
{
  auto res = client_.Post(path, body, content_type);
  if (res) {
    if (res->status == 200) {
      response_body = res->body;
      return true;
    } else {
      response_body = "Http post failed with status code: " + std::to_string(res->status);
    }
  } else {
    response_body = "Failed to post request.";
  }
  return false;
}

}  // namespace logproxy
}  // namespace oceanbase