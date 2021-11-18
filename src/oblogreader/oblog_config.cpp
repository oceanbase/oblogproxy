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

#include <vector>
#include "oblog_config.h"

namespace oceanbase {
namespace logproxy {

static size_t split(const std::string& str, char sep, std::vector<std::string>& vec, bool first)
{
  vec.clear();
  if (str.empty()) {
    return 0;
  }
  bool flag = false;
  char* p_cur = const_cast<char*>(str.c_str());
  char* p_pre = const_cast<char*>(str.c_str());
  while (*p_cur != '\0') {
    if (*p_cur == sep && (!first || !flag)) {
      vec.emplace_back(std::string(p_pre, p_cur - p_pre));
      p_pre = p_cur + 1;
      flag = true;
    }
    ++p_cur;
  }
  vec.emplace_back(p_pre);
  return vec.size();
}

/**
 * copy splitted string to vec
 */
size_t split(const std::string& str, char sep, std::vector<std::string>& vec)
{
  return split(str, sep, vec, false);
}

size_t split_first(const std::string& str, char sep, std::vector<std::string>& vec)
{
  return split(str, sep, vec, true);
}

OblogConfig::OblogConfig(const std::string& str)
{
  // syntax: "k1=v1 k2=v2 k3=v3"
  std::vector<std::string> kvs;
  split(str, ' ', kvs);
  for (std::string& kv : kvs) {
    std::vector<std::string> kv_split;
    int count = split_first(kv, '=', kv_split);
    if (count != 2) {
      continue;
    }
    _configs.emplace(kv_split[0], kv_split[1]);
  }
  const auto& entry = _configs.find("start_timestamp");
  if (entry != _configs.end()) {
    start_timestamp = strtoull(entry->second.c_str(), nullptr, 10);
  }
}

void OblogConfig::set_auth(const std::string& user, const std::string& password)
{
  _configs["cluster_user"] = user;
  _configs["cluster_password"] = password;
}

}  // namespace logproxy
}  // namespace oceanbase
