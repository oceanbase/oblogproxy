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

#include <algorithm>
#include "common/str.h"

namespace oceanbase {
namespace logproxy {

size_t split_all(const std::string& str, const std::string& seps, std::vector<std::string>& vec, bool first)
{
  bool sep_idx[256] = {false};
  for (char sep : seps) {
    sep_idx[(uint8_t)sep] = true;
  }

  vec.clear();
  if (str.empty()) {
    return 0;
  }
  bool flag = false;
  char* p_cur = const_cast<char*>(str.c_str());
  char* p_pre = const_cast<char*>(str.c_str());
  while (*p_cur != '\0') {
    if (sep_idx[(uint8_t)*p_cur] && (!first || !flag)) {
      vec.emplace_back(std::string(p_pre, p_cur - p_pre));
      p_pre = p_cur + 1;
      flag = true;
    }
    ++p_cur;
  }
  vec.emplace_back(p_pre);
  return vec.size();
}

size_t split(const std::string& str, char sep, std::vector<std::string>& vec, bool first)
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
size_t split_by_str(std::string str, const std::string& sep, std::vector<std::string>& vec, bool first)
{
  vec.clear();
  if (str.empty()) {
    return 0;
  }
  size_t pos = 0;
  std::string temp;
  while ((pos = str.find(sep)) != std::string::npos) {
    temp = str.substr(0, pos);
    vec.emplace_back(temp);
    str.erase(0, pos + sep.length());
  }
  vec.emplace_back(str);
  return vec.size();
}

void ltrim(std::string& s)
{
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

void rtrim(std::string& s)
{
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

void trim(std::string& s)
{
  ltrim(s);
  rtrim(s);
}

}  // namespace logproxy
}  // namespace oceanbase