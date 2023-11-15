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

#include "common.h"
#include "log.h"

namespace oceanbase {
namespace logproxy {

void init_log(const char* log_basename)
{
  std::string bin_name = log_basename;
  auto pos = bin_name.find_last_of('/');
  if (pos != std::string::npos) {
    bin_name = bin_name.substr(pos + 1);
  }

  Config& s_config = Config::instance();
  std::string log_path("./log/");
  log_path.append(bin_name).append(".log");
  if (!Logger::instance().init(
          bin_name, log_path, s_config.log_max_file_size_mb.val(), s_config.log_retention_h.val())) {
    OMS_ERROR("Failed to init spdlog logger: {}", log_path);
  }
  spdlog::set_level(spdlog::level::level_enum(s_config.log_level.val()));
  if (s_config.log_flush_strategy.val()) {
    spdlog::flush_on(spdlog::level::level_enum(s_config.log_flush_level.val()));
  } else {
    spdlog::flush_every(std::chrono::seconds(s_config.log_flush_period_s.val()));
  }
}

void replace_spdlog_default_logger()
{
  auto logger = spdlog::basic_logger_mt("init", "./log/init.log");
  if (nullptr != logger) {
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(logger);
  }
}

}  // namespace logproxy
}  // namespace oceanbase