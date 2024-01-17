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

#pragma once
#include <string>
namespace etransfer {
namespace common {
class RawConstant {
 public:
  RawConstant(bool escaped, const std::string &origin_string,
              const std::string &value)
      : escaped(escaped), origin_string(origin_string), value(value) {}
  RawConstant(const RawConstant &raw_constant)
      : escaped(raw_constant.escaped),
        origin_string(raw_constant.origin_string),
        value(raw_constant.value) {}
  RawConstant() : escaped(false), origin_string(""), value("") {}
  RawConstant(const std::string &origin_string)
      : escaped(false), origin_string(origin_string), value(origin_string) {}

 public:
  bool escaped;
  std::string origin_string;
  std::string value;
};
};  // namespace common
};  // namespace etransfer