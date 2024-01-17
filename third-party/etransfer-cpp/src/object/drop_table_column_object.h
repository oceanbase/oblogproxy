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
#include "object/object.h"
namespace etransfer {
namespace object {
class DropTableColumnObject : public Object {
 public:
  DropTableColumnObject(const Catalog& catalog, const RawConstant& object_name,
                        const std::string& raw_ddl,
                        const RawConstant& column_name_to_drop,
                        const Strings& drop_table_option)
      : Object(catalog, object_name, raw_ddl,
               ObjectType::DROP_TABLE_COLUMN_OBJECT),
        column_name_to_drop(column_name_to_drop),
        drop_table_option(drop_table_option) {}

  RawConstant GetRawColumnNameToDrop() { return column_name_to_drop; }

 private:
  RawConstant column_name_to_drop;
  Strings drop_table_option;
};

}  // namespace object

}  // namespace etransfer
