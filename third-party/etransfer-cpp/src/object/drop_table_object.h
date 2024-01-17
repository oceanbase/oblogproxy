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
class DropTableObject : public Object {
 public:
  DropTableObject(
      std::vector<std::pair<Catalog, common::RawConstant>> tables_to_drop,
      std::string raw_sql, bool is_tmp_table, bool drop_if_exist)
      : Object(tables_to_drop[0].first, tables_to_drop[0].second, raw_sql,
               ObjectType::DROP_TABLE_OBJECT),
        tables_to_drop_(tables_to_drop),
        is_tmp_table_(is_tmp_table),
        drop_if_exist_(drop_if_exist) {}

  std::vector<std::pair<Catalog, common::RawConstant>> GetTablesToDrop() {
    return tables_to_drop_;
  }

 private:
  std::vector<std::pair<Catalog, common::RawConstant>> tables_to_drop_;
  bool is_tmp_table_;
  bool drop_if_exist_;
};

};  // namespace object
};  // namespace etransfer