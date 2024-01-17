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
#include "object/table_column_object.h"
#include "sink/mysql_expr_builder.h"
#include "sink/object_builder.h"
namespace etransfer {
namespace sink {
using namespace object;
class AddTableColumnObjectBuilder : public ObjectBuilder {
 private:
  MySQLExprBuilder expr_builder_;

 public:
  Strings RealBuildSql(ObjectPtr db_object, ObjectPtr parent_object,
                       std::shared_ptr<BuildContext> sql_builder_context);
  std::string BuildSpatialRefId(
      std::shared_ptr<TableColumnObject> table_column_object,
      std::shared_ptr<BuildContext> sql_builder_context);
  std::string BuildOnUpdate(
      std::shared_ptr<TableColumnObject> table_column_object,
      RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  bool CheckVersionForOnUpdate(std::shared_ptr<BuildContext> context,
                               RealDataType data_type);

  std::string BuildCheckConstraint(
      std::shared_ptr<TableColumnObject> table_column_object,
      std::shared_ptr<BuildContext> sql_builder_context);

 private:
  MySQLExprBuilder default_value_expr_builder_;

  std::string BuildPosition(
      std::shared_ptr<TableColumnObject> table_column_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildGenExpr(
      std::shared_ptr<TableColumnObject> table_column_object,
      RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  bool CheckVersionForGenExpr(
      std::shared_ptr<BuildContext> sql_builder_context);
};
}  // namespace sink

}  // namespace etransfer