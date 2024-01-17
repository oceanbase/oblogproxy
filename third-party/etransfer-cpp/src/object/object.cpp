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

#include "object/object.h"

namespace etransfer {
namespace object {
common::ObjectType Object::GetObjectType() const { return object_type; }

// catalog of object
common::Catalog Object::GetCatalog() const { return catalog; }

// object name
common::RawConstant Object::GetRawObjectName() const { return object_name; }

bool Object::DoFilter(const ObjectFilter& object_filter) {
  ObjectPtrs filtered;
  for (auto sub_object : sub_objects) {
    if (!sub_object->DoFilter(object_filter)) {
      filtered.push_back(sub_object);
    }
  }
  sub_objects = filtered;
  return FilterInner(object_filter);
}

bool Object::FilterInner(const ObjectFilter& object_filter) { return false; }

ObjectPtrs Object::ResortSubObjects(const ObjectPtrs& sub_actions) {
  if (sub_actions.empty() ||
      !(object_type == ObjectType::CREATE_TABLE_OBJECT)) {
    return sub_actions;
  }
  ObjectPtrs first_part_objects;
  ObjectPtrs other_objects;
  for (const auto& action : sub_actions) {
    if (action->GetObjectType() == ObjectType::TABLE_COLUMN_OBJECT) {
      first_part_objects.push_back(action);
    } else {
      other_objects.push_back(action);
    }
  }
  first_part_objects.insert(first_part_objects.end(), other_objects.begin(),
                            other_objects.end());
  return first_part_objects;
}

void Object::SetSubObjects(const ObjectPtrs& sub_actions) {
  auto sorted_actions = ResortSubObjects(sub_actions);
  sub_objects = sorted_actions;
}

};  // namespace object
};  // namespace etransfer