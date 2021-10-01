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

#include <sstream>
#include <fstream>
#include "log.h"
#include "config.h"
#include "jsonutil.hpp"

namespace oceanbase {
namespace logproxy {

int Config::load(const std::string& file)
{
  std::ifstream ifs(file, std::ios::in);
  if (!ifs.good()) {
    OMS_FATAL << "failed to open config file: " << file;
    return OMS_FAILED;
  }

  std::string errmsg;
  Json::Value json;
  if (!str2json(ifs, json, &errmsg)) {
    OMS_FATAL << "failed to parse config file: " << file << ", errmsg: " << errmsg;
    return OMS_FAILED;
  }

  for (const std::string& k : json.getMemberNames()) {
    const auto& centry = configs_.find(k);
    if (configs_.count(k) != 0) {
      centry->second->from_str(json[k].asString());
    }
  }

  OMS_INFO << "success to load config: \n" << debug_str(true);
  return OMS_OK;
}

void Config::add(const std::string& key, const std::string& value)
{
  auto entry = configs_.find(key);
  if (entry == configs_.end()) {
    extras_.emplace(key, value);
  } else {
    entry->second->from_str(value);
  }
}

std::string Config::debug_str(bool formatted) const
{
  std::stringstream ss;
  for (auto& entry : configs_) {
    ss << entry.first << ":" << entry.second->debug_str() << ",";
    if (formatted) {
      ss << '\n';
    }
  }
  for (auto& entry : extras_) {
    ss << entry.first << ":" << entry.second << "," << std::endl;
    if (formatted) {
      ss << '\n';
    }
  }
  return ss.str();
}

}  // namespace logproxy
}  // namespace oceanbase
