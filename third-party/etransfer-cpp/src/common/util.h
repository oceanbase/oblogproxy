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
#include <algorithm>  // std::equal
#include <cctype>     // std::tolower
#include <locale>
#include <string>

#include "antlr4-runtime.h"
#include "common/raw_constant.h"

namespace etransfer {
using Strings = std::vector<std::string>;
namespace common {
class Util {
 public:
  static std::string RawConstantValue(const RawConstant& raw_constant);

  static std::string RawConstantValue(const RawConstant& raw_constant,
                                      bool use_orig_name);

  static std::string GetCtxString(antlr4::ParserRuleContext* ctx);

  static bool IsEscapeString(const std::string& name,
                             const std::string& escape_string);

  static std::string LowerCase(std::string data);

  static std::string UpperCase(std::string data);

  static bool CharEquals(char a, char b);

  static bool EqualsIgnoreCase(const std::string& a, const std::string& b);

  static std::string StringJoin(const Strings& strings,
                                const std::string& delim);

  static bool ContainsIgnoreCase(const std::string& str_haystack,
                                 const std::string& str_needle);
  static RawConstant GetValueString(antlr4::ParserRuleContext* parse_tree);

  static RawConstant ProcessEscapeString(const std::string& name,
                                         const std::string& escape_string);

  static bool StartsWithIgnoreCase(const std::string& a, const std::string& b);

  // trim from start (in place)
  static void Ltrim(std::string& s);

  // trim from end (in place)
  static void Rtrim(std::string& s);

  // trim from both ends (in place)
  static void Trim(std::string& s);

  static void ReplaceFirst(std::string& s, const std::string& to_replace,
                           const std::string& replace_with);

  static std::vector<std::string> Split(const std::string& s,
                                        const std::string& delimiter);

  static bool StartsWith(const std::string& name, const std::string& start);

  static bool EndsWith(const std::string& name, const std::string& end);
};
};  // namespace common

};  // namespace etransfer
