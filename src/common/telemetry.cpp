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
#include "telemetry.h"
#include "json/json.h"
#include "uuid_global.h"
#include "config.h"
#include "log.h"

namespace oceanbase {
namespace logproxy {

std::string path = "/api/web/oceanbase/report";

bool Telemetry::report_telemetry_json(const std::string& host, const std::string& telemetry_json_data,
    int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec)
{
  if (!Config::instance().telemetry_enabled.val()) {
    OMS_DEBUG("Reporting telemetry..., but telemetry not enabled.");
    return false;
  }
  OMS_INFO("Reporting telemetry...");
  HttpPostClient http_client(host);

  http_client.set_connection_timeout(connection_timeout_sec, 0);
  http_client.set_read_timeout(read_timeout_sec, 0);
  http_client.set_write_timeout(write_timeout_sec, 0);

  std::string content_type = "application/json";
  std::string response_body;

  bool result = http_client.post(path, telemetry_json_data, response_body, content_type);
  if (result) {
    OMS_INFO("Report telemetry successful，data: {}", telemetry_json_data);
    return true;
  }
  OMS_INFO("Failed to report telemetry，response: {}", response_body);
  return false;
}

std::future<bool> Telemetry::report_telemetry_json_async(const std::string& host,
    const std::string& telemetry_json_data, int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec)
{
  return std::async(std::launch::async,
      [host, telemetry_json_data, connection_timeout_sec, read_timeout_sec, write_timeout_sec]() -> bool {
        return report_telemetry_json(
            host, telemetry_json_data, connection_timeout_sec, read_timeout_sec, write_timeout_sec);
      });
}

bool Telemetry::report_telemetry_data(
    const std::string& host, int count, int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec)
{
  if (!Config::instance().telemetry_enabled.val()) {
    OMS_DEBUG("Reporting telemetry..., but telemetry not enabled.");
    return false;
  }
  OMS_INFO("Reporting telemetry...");
  HttpPostClient http_client(host);
  http_client.set_connection_timeout(connection_timeout_sec, 0);
  http_client.set_read_timeout(read_timeout_sec, 0);
  http_client.set_write_timeout(write_timeout_sec, 0);

  Json::Value json_obj;
  json_obj["content"]["id"] = global_uuid;
  json_obj["content"]["version"] = __OMS_VERSION__;
  json_obj["content"]["mode"] = Config::instance().binlog_mode.val() ? "binlog" : "cdc";
  json_obj["content"]["projects"]["count"] = count;
  json_obj["time"] = getCurrentTimeISO8601();
  json_obj["component"] = "OBLogProxy";

  Json::StreamWriterBuilder builder;
  std::string json_data = Json::writeString(builder, json_obj);

  std::string content_type = "application/json";
  std::string response_body;

  bool result = http_client.post(path, json_data, response_body, content_type);
  if (result) {
    OMS_INFO("Report telemetry successful，data: {}", json_data);
    return true;
  }
  OMS_INFO("Failed to report telemetry，response: {}", response_body);
  return false;
}

std::future<bool> Telemetry::report_telemetry_data_async(
    const std::string& host, int count, int connection_timeout_sec, int read_timeout_sec, int write_timeout_sec)
{
  return std::async(
      std::launch::async, [host, count, connection_timeout_sec, read_timeout_sec, write_timeout_sec]() -> bool {
        return report_telemetry_data(host, count, connection_timeout_sec, read_timeout_sec, write_timeout_sec);
      });
}

std::string Telemetry::getCurrentTimeISO8601()
{
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_c = system_clock::to_time_t(now);
  auto now_tm = *std::localtime(&now_c);

  std::ostringstream oss;
  oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

}  // namespace logproxy
}  // namespace oceanbase