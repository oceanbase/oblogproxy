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

#include "convert/sql_builder_context.h"

#include "common/define.h"
namespace etransfer {
namespace sink {
BuildContext::BuildContext()
    : escape_char("`"),
      use_schema_prefix(false),
      use_origin_identifier(false),
      use_escape_string(true),
      use_foreign_key_filter(true),
      succeed(true),
      err_msg("") {
  SetDbVersion(5, 7, 22);
}
void BuildContext::Reset() {
  succeed = true;
  err_msg.clear();
}
void BuildContext::SetErrMsg(const std::string& err_msg) {
  succeed = false;
  this->err_msg = err_msg;
}

void BuildContext::SetDbVersion(const uint32_t major, const uint32_t minor,
                                const uint32_t patch) {
  target_db_version = common::CalVersion(major, minor, patch);
}

}  // namespace sink

}  // namespace etransfer
