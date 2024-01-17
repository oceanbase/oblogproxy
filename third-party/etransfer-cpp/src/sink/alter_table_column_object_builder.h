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
#include "object/alter_table_column_object.h"
#include "sink/mysql_expr_builder.h"
#include "sink/object_builder.h"
namespace etransfer {
namespace sink {
class AlterTableColumnObjectBuilder : public ObjectBuilder {
 private:
  MySQLExprBuilder expr_builder_;

 public:
  Strings RealBuildSql(ObjectPtr db_object, ObjectPtr parent_object,
                       std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildColumnComment(
      std::shared_ptr<AlterTableColumnObject> alter_table_column_object);
};

}  // namespace sink

}  // namespace etransfer
