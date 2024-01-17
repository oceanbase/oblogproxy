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
namespace sink {
// BuildContext define some build related context params.

class BuildContext {
 public:
  BuildContext();
  void Reset();
  void SetErrMsg(const std::string& err_msg);

  void SetDbVersion(const uint32_t major, const uint32_t minor,
                    const uint32_t patch);

 public:
  std::string escape_char;  // mysql escape string
  bool use_schema_prefix;
  bool use_origin_identifier;
  bool use_escape_string;
  bool use_foreign_key_filter;
  uint32_t target_db_version;
  bool succeed;
  std::string err_msg;
};

};  // namespace sink
};  // namespace etransfer