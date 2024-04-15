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
class RenameTableColumnObject : public Object {
 private:
  RawConstant origin_column_name_;
  RawConstant current_column_name_;

 public:
  RenameTableColumnObject(const Catalog& catalog, const RawConstant& object_name,
                    const std::string& raw_ddl,
                    const RawConstant& origin_column_name,
                    const RawConstant& current_column_name)
      : Object(catalog, object_name, raw_ddl, ObjectType::RENAME_TABLE_COLUMN_OBJECT),
        origin_column_name_(origin_column_name),
        current_column_name_(current_column_name) {}

  std::string GetCurrentColumnName() {
    return Util::RawConstantValue(current_column_name_);
  }

  std::string GetOriginColumnName() {
    return Util::RawConstantValue(origin_column_name_);
  }
};

}
}