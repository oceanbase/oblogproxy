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
#include "object/alter_table_object.h"
#include "sink/object_builder.h"
namespace etransfer {
namespace sink {
class AlterTableObjectBuilder : public ObjectBuilder {
 public:
  Strings RealBuildSql(ObjectPtr db_object, ObjectPtr parent_object,
                       std::shared_ptr<BuildContext> sql_builder_context);

  // build dependent ddl, It should be noted that whenever an sub object is
  // build, it needs to be removed from subObjects
  Strings BuildIndependentAlterStatements(
      std::shared_ptr<AlterTableObject> alter_table_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildIndependentStatement(
      std::shared_ptr<AlterTableObject> alter_table_object,
      ObjectPtr schema_object,
      std::shared_ptr<BuildContext> sql_builder_context);
};

}  // namespace sink

}  // namespace etransfer