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
class DropTableConstraintObject : public Object {
 public:
  DropTableConstraintObject(const Catalog& catalog,
                            const RawConstant& object_name,
                            const std::string& raw_ddl, IndexType index_type,
                            const RawConstant& index_name)
      : Object(catalog, object_name, raw_ddl,
               ObjectType::DROP_TABLE_CONSTRAINT_OBJECT),
        index_name_(index_name),
        index_type_(index_type) {}

  std::string GetIndexName() { return Util::RawConstantValue(index_name_); }

  RawConstant GetRawIndexName() { return index_name_; }

  IndexType GetIndexType() { return index_type_; }

 private:
  RawConstant index_name_;
  IndexType index_type_;
};
}  // namespace object

}  // namespace etransfer
