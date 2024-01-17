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

#include "object/create_table_object.h"

#include "object/table_check_constraint.h"
namespace etransfer {
namespace object {
CreateTableObject::CreateTableObject(
    const Catalog& catalog, const RawConstant& object_name,
    const std::string& raw_ddl, const std::shared_ptr<ObjectPtrs> extra_object,
    std::shared_ptr<CreateTableDDLTricks> create_table_ddl_tricks)
    : CreateTableObject(catalog, object_name, raw_ddl, extra_object,
                        create_table_ddl_tricks, nullptr) {}

CreateTableObject::CreateTableObject(
    const Catalog& catalog, const RawConstant& object_name,
    const std::string& raw_ddl, std::shared_ptr<ObjectPtrs> extra_object,
    std::shared_ptr<CreateTableDDLTricks> create_table_ddl_tricks,
    std::shared_ptr<std::vector<std::shared_ptr<Option>>> option_objects)
    : Object(catalog, object_name, raw_ddl, ObjectType::CREATE_TABLE_OBJECT),
      extra_object_(extra_object),
      create_table_ddl_tricks_(create_table_ddl_tricks),
      option_objects_(option_objects) {}
bool CreateTableObject::FilterInner(const ObjectFilter& object_filter) {
  std::shared_ptr<ObjectPtrs> filter_extra_object =
      std::make_shared<ObjectPtrs>();
  if (extra_object_ == nullptr) {
    return false;
  }
  for (auto extra : *extra_object_) {
    if (!extra->DoFilter(object_filter)) {
      filter_extra_object->push_back(extra);
    }
  }
  extra_object_ = filter_extra_object;
  return false;
}
}  // namespace object

}  // namespace etransfer