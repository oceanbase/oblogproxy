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
#include <utility>

#include "object/object.h"
#include "object/option.h"
namespace etransfer {
namespace object {
using namespace common;
class TruncateTableObject : public Object {
 public:
  TruncateTableObject(
      const std::pair<Catalog, common::RawConstant>& table_to_truncate,
      const std::string& raw_sql)
      : Object(table_to_truncate.first, table_to_truncate.second, raw_sql,
               ObjectType::TRUNCATE_TABLE_OBJECT),
        table_to_truncate_(table_to_truncate) {}

  std::pair<Catalog, common::RawConstant> GetTableToTruncate() {
    return table_to_truncate_;
  }

 private:
  std::pair<Catalog, common::RawConstant> table_to_truncate_;
};

};  // namespace object
};  // namespace etransfer