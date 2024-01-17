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
namespace source {
// ParseContext define some parse related context params.
class ParseContext {
 public:
  ParseContext(const std::string& raw_ddl, const std::string& default_schema,
               bool is_case_sensitive)
      : raw_ddl(raw_ddl),
        default_schema(default_schema),
        is_case_sensitive(is_case_sensitive),
        default_charset(""),
        default_collection(""),
        ignore_not_supported(true),
        escape_char("`"),
        succeed(true),
        err_msg("") {}

  enum CharLengthSemantics { CHAR, TWO_BYTE_CHAR, BYTE };
  static int GetCharLengthSemanticsUnit(CharLengthSemantics c) {
    switch (c) {
      case CHAR:
        return 4;
      case TWO_BYTE_CHAR:
        return 2;
      case BYTE:
        return 1;
      default:
        return -1;
    }
  }

  void SetErrMsg(const std::string& err_msg) {
    succeed = false;
    this->err_msg = err_msg;
  }
  std::string raw_ddl;
  std::string default_schema;
  bool is_case_sensitive;
  std::string default_charset;
  std::string default_collection;

  bool ignore_not_supported;
  std::string escape_char;
  bool succeed;
  std::string err_msg;
};

};  // namespace source
};  // namespace etransfer