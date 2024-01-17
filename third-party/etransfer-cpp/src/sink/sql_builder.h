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
#include "convert/sql_builder_context.h"
#include "object/object.h"
#include "object/object_filter.h"
namespace etransfer {
namespace sink {
using namespace object;
// SqlBuilder convert intermediate object to DDL statement.
class SqlBuilder {
 public:
  Strings ApplySchemaObject(ObjectPtr db_object,
                            std::shared_ptr<BuildContext> context);

  virtual Strings RealApplySchemaObject(
      ObjectPtr db_object, std::shared_ptr<BuildContext> context) = 0;

 protected:
  ObjectFilter object_filter;
};
}  // namespace sink
}  // namespace etransfer