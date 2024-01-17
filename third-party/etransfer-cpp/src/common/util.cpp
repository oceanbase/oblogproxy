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

#include "common/util.h"

namespace etransfer {
namespace common {
std::string Util::RawConstantValue(const RawConstant& raw_constant) {
  return raw_constant.value;
}
std::string Util::RawConstantValue(const RawConstant& raw_constant,
                                   bool use_orig_name) {
  return use_orig_name ? raw_constant.origin_string : raw_constant.value;
}
std::string Util::GetCtxString(antlr4::ParserRuleContext* parser_rule_context) {
  auto a = parser_rule_context->start->getStartIndex();
  auto b = parser_rule_context->stop->getStopIndex();
  antlr4::misc::Interval interval(a, b);
  if (b >= a) {
    return parser_rule_context->start->getInputStream()->getText(interval);
  }
  return std::string("");
}
bool Util::IsEscapeString(const std::string& name,
                          const std::string& escape_string) {
  return StartsWith(name, escape_string) && EndsWith(name, escape_string);
}
bool Util::StartsWith(const std::string& name, const std::string& start) {
  return name.rfind(start, 0) == 0;
}

bool Util::EndsWith(const std::string& name, const std::string& end) {
  if (end.size() > name.size()) return false;
  return std::equal(end.rbegin(), end.rend(), name.rbegin());
}
std::string Util::LowerCase(std::string data) {
  std::transform(data.begin(), data.end(), data.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return data;
}

std::string Util::UpperCase(std::string data) {
  std::transform(data.begin(), data.end(), data.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return data;
}

bool Util::CharEquals(char a, char b) {
  return std::tolower(static_cast<unsigned char>(a)) ==
         std::tolower(static_cast<unsigned char>(b));
}

bool Util::EqualsIgnoreCase(const std::string& a, const std::string& b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), CharEquals);
}
std::string Util::StringJoin(const Strings& strings, const std::string& delim) {
  std::ostringstream out;
  if (!strings.empty()) {
    std::copy(strings.begin(), strings.end() - 1,
              std::ostream_iterator<std::string>(out, delim.c_str()));
    out << strings.back();
  }
  return out.str();
}

bool Util::ContainsIgnoreCase(const std::string& str_haystack,
                              const std::string& str_needle) {
  auto it =
      std::search(str_haystack.begin(), str_haystack.end(), str_needle.begin(),
                  str_needle.end(), [](unsigned char ch1, unsigned char ch2) {
                    return std::toupper(ch1) == std::toupper(ch2);
                  });
  return (it != str_haystack.end());
}
RawConstant Util::GetValueString(antlr4::ParserRuleContext* parse_tree) {
  std::string origin_value = GetCtxString(parse_tree);
  if (IsEscapeString(origin_value, "'")) {
    return RawConstant(true, origin_value,
                       origin_value.substr(1, origin_value.length() - 2));
  } else {
    return RawConstant(false, origin_value, origin_value);
  }
}

RawConstant Util::ProcessEscapeString(const std::string& name,
                                      const std::string& escape_string) {
  if (IsEscapeString(name, escape_string)) {
    return RawConstant(true, name.substr(1, name.length() - 2),
                       name.substr(1, name.length() - 2));
  } else {
    return RawConstant(false, name, name);
  }
}

bool Util::StartsWithIgnoreCase(const std::string& a, const std::string& b) {
  if (a.size() < b.size()) {
    return false;
  }
  return EqualsIgnoreCase(a.substr(0, b.size()), b);
}

void Util::ReplaceFirst(std::string& s, const std::string& to_replace,
                        const std::string& replace_with) {
  std::size_t pos = s.find(to_replace);
  if (pos == std::string::npos) return;
  s.replace(pos, to_replace.length(), replace_with);
}

void Util::Ltrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

void Util::Rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

void Util::Trim(std::string& s) {
  Rtrim(s);
  Ltrim(s);
}

std::vector<std::string> Util::Split(const std::string& s,
                                     const std::string& delimiter) {
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  std::string token;
  std::vector<std::string> res;

  while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
  }

  res.push_back(s.substr(pos_start));
  return res;
}

}  // namespace common

}  // namespace etransfer
