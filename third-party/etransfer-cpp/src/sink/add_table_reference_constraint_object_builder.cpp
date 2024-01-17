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

#include "sink/add_table_reference_constraint_object_builder.h"

#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings AddTableReferenceConstraintObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  Strings res;
  auto table_reference_constraint_object =
      std::dynamic_pointer_cast<TableReferenceConstraintObject>(db_object);
  std::string line("\t");
  if (parent_object->GetObjectType() == ObjectType::ALTER_TABLE_OBJECT) {
    line.append("ADD ");
  }
  if (table_reference_constraint_object->GetConstraintName() != "") {
    line.append("CONSTRAINT ")
        .append(SqlBuilderUtil::EscapeNormalObjectName(
            table_reference_constraint_object->GetConstraintName(),
            sql_builder_context))
        .append(" ");
  }
  line.append("FOREIGN KEY (")
      .append(SqlBuilderUtil::GetRealColumnNames(
          table_reference_constraint_object->GetCatalog(),
          table_reference_constraint_object->GetRawObjectName(),
          table_reference_constraint_object->GetRawChildTableColumns(),
          sql_builder_context))
      .append(") ")
      .append("REFERENCES ")
      .append(SqlBuilderUtil::GetRealTableName(
          table_reference_constraint_object->GetParentCatalog(),
          table_reference_constraint_object->GetRawParentTable(),
          sql_builder_context));

  if (table_reference_constraint_object->GetRawParentTableColumns() !=
      nullptr) {
    line.append("(")
        .append(SqlBuilderUtil::GetRealColumnNames(
            table_reference_constraint_object->GetParentCatalog(),
            table_reference_constraint_object->GetRawParentTable(),
            table_reference_constraint_object->GetRawParentTableColumns(),
            sql_builder_context))
        .append(") ");
  }
  line.append(" ");

  if (table_reference_constraint_object->GetOperationCascades() != nullptr) {
    line.append(BuildForeignKeyOptions(
        table_reference_constraint_object->GetOperationCascades()));
  }
  res.push_back(line);
  return res;
}
std::string AddTableReferenceConstraintObjectBuilder::BuildForeignKeyOptions(
    std::shared_ptr<std::vector<
        TableReferenceConstraintObject::FKReferenceOperationCascade>>
        options) {
  std::string line;
  for (const auto fk_reference_operation_cascade : *options) {
    switch (fk_reference_operation_cascade) {
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_CASCADE:
        line.append("ON DELETE CASCADE ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_UPDATE_CASCADE:
        line.append("ON UPDATE CASCADE ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_RESTRICT:
        line.append("ON DELETE RESTRICT ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_UPDATE_SET_NULL:
        line.append("ON UPDATE SET NULL ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_SET_NULL:
        line.append("ON DELETE SET NULL ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_NO_ACTION:
        line.append("ON DELETE NO ACTION ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_UPDATE_NO_ACTION:
        line.append("ON UPDATE NO ACTION ");
        break;
      case TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_UPDATE_RESTRICT:
        line.append("ON UPDATE RESTRICT ");
        break;
      default:
        break;
    }
  }
  return line;
}
}  // namespace sink

}  // namespace etransfer
