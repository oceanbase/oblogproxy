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
class RenameIndexObject : public Object {
 private:
  RawConstant origin_index_name_;
  RawConstant current_index_name_;

 public:
  RenameIndexObject(const Catalog& catalog, const RawConstant& object_name,
                    const std::string& raw_ddl,
                    const RawConstant& origin_index_name,
                    const RawConstant& current_index_name)
      : Object(catalog, object_name, raw_ddl, ObjectType::RENAME_INDEX_OBJECT),
        origin_index_name_(origin_index_name),
        current_index_name_(current_index_name) {}

  std::string GetCurrentIndexName() {
    return Util::RawConstantValue(current_index_name_);
  }

  std::string GetOriginIndexName() {
    return Util::RawConstantValue(origin_index_name_);
  }
};

}  // namespace object

}  // namespace etransfer
