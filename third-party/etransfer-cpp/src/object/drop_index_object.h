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
class DropIndexObject : public Object {
 private:
  RawConstant index_name_;

 public:
  DropIndexObject(const Catalog& catalog, const RawConstant& object_name,
                  const RawConstant& index_name, const std::string& raw_sql)
      : Object(catalog, object_name, raw_sql, ObjectType::DROP_INDEX_OBJECT),
        index_name_(index_name) {}

  RawConstant GetRawIndexName() { return index_name_; }
};
}  // namespace object

}  // namespace etransfer
