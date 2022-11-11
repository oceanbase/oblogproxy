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

namespace oceanbase {
namespace logproxy {

/**
 * copy splitted string to vec
 */
size_t split_all(const std::string& str, const std::string& seps, std::vector<std::string>& vec, bool first = false);

size_t split(const std::string& str, char sep, std::vector<std::string>& vec, bool first = false);

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