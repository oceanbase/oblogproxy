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
#include <set>

#include "object/object.h"
#include "object/option.h"
namespace etransfer {
namespace object {
using namespace common;
struct CreateTableDDLTricks {
  bool is_temporary_table;
  bool create_if_not_exists;
  bool is_create_like;
  ObjectPtr create_like_syntax;
};
class CreateTableObject : public Object {
 public:
  CreateTableObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl,
      const std::shared_ptr<ObjectPtrs> extra_object,
      std::shared_ptr<CreateTableDDLTricks> create_table_ddl_tricks);

  CreateTableObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, std::shared_ptr<ObjectPtrs> extra_object,
      std::shared_ptr<CreateTableDDLTricks> create_table_ddl_tricks,
      std::shared_ptr<std::vector<std::shared_ptr<Option>>> option_objects);

  ObjectPtrs GetCreateTableSubActions() { return sub_objects; }

  std::shared_ptr<ObjectPtrs> GetExtraObject() { return extra_object_; }

  std::shared_ptr<CreateTableDDLTricks> GetCreateTableDdlTricks() {
    return create_table_ddl_tricks_;
  }

  std::shared_ptr<std::vector<std::shared_ptr<Option>>> TableOptionDescribes() {
    return option_objects_;
  }

  bool FilterInner(const ObjectFilter& object_filter);

 private:
  // current only partition object defined
  std::shared_ptr<ObjectPtrs> extra_object_;

  std::shared_ptr<CreateTableDDLTricks> create_table_ddl_tricks_;

  std::shared_ptr<std::vector<std::shared_ptr<Option>>> option_objects_;
};
}  // namespace object

}  // namespace etransfer
