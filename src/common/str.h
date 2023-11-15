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
#include <algorithm>
#include <iterator>
#include <cstring>
namespace oceanbase {
namespace logproxy {
/**
 * copy splitted string to vec
 */
size_t split_all(const std::string& str, const std::string& seps, std::vector<std::string>& vec, bool first = false);

size_t split(const std::string& str, char sep, std::vector<std::string>& vec, bool first = false);

int index_of(const char* range, char c);

std::string pad_left(const std::string& value, int size, char padding = ' ');

std::string pad_left(const std::string* value = nullptr, int size = 0, char padding = ' ');

std::string pad_right(const std::string& value, int size, char padding = ' ');

std::string pad_right(const std::string* value = nullptr, int size = 0, char padding = ' ');

size_t split_by_str(std::string str, const std::string& sep, std::vector<std::string>& vec, bool first = false);

/*
 * @param str
 * @return null
 * @description trim from begin
 * @date 2022/9/19 15:36
 */
void ltrim(std::string& s);

/*
 * @param str
 * @return null
 * @description trim from end
 * @date 2022/9/19 15:36
 */
void rtrim(std::string& s);

/*
 * @param str
 * @return null
 * @description trim from both begin and end
 * @date 2022/9/19 15:36
 */
void trim(std::string& s);

}  // namespace logproxy
}  // namespace oceanbase

template <typename T>
bool any_string_equal(const T& str, const std::initializer_list<T>& strings)
{
  for (const auto& s : strings) {
    if (strcasecmp(s, str) == 0) {
      return true;
    }
  }
  return false;
}
template <typename T, typename... Args>
bool any_string_equal(const T& str, const std::initializer_list<T>& strings, Args... args)
{
  return any_string_equal(str, strings) || any_string_equal(str, args...);
}

// Checks whether one of the N strings is consistent with the specified string and returns true
#define ANY_STRING_EQUAL(str, ...) any_string_equal(str, {__VA_ARGS__})