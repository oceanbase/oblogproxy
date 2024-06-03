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
#include <future>
#include "http_util.h"
namespace oceanbase {
namespace logproxy {
extern std::string path;
class Telemetry {
public:
  static bool report_telemetry_json(const std::string& host, const std::string& telemetry_json_data,
      int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec);

  static std::future<bool> report_telemetry_json_async(const std::string& host,
      const std::string& telemetry_json_data, int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec);

  static bool report_telemetry_data(const std::string& host, int count, int connection_timeout_sec,
      int read_timeout_sec, int write_timeout_sec);

  static std::future<bool> report_telemetry_data_async(const std::string& host, int count,
      int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec);

  static std::string getCurrentTimeISO8601();
};
}  // namespace logproxy
}  // namespace oceanbase
