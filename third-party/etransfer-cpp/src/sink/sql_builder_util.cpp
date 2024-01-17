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

#include "sink/sql_builder_util.h"

#include "common/util.h"
#include "object/alter_table_column_object.h"
#include "object/table_column_object.h"
namespace etransfer {
using namespace common;
namespace sink {
std::string SqlBuilderUtil::BuildRealTableName(
    const Catalog& catalog, const RawConstant& object_name,
    std::shared_ptr<BuildContext> sql_builder_context, bool with_schema) {
  std::string res;
  bool use_origin = sql_builder_context->use_origin_identifier;
  if (sql_builder_context->use_schema_prefix && with_schema) {
    std::string catalog_name = catalog.GetCatalogName(use_origin);
    if (!catalog_name.empty()) {
      res.append(EscapeNormalObjectName(catalog_name, sql_builder_context))
          .append(".");
    }
  }
  res.append(EscapeNormalObjectName(
      Util::RawConstantValue(object_name, use_origin), sql_builder_context));
  return res;
}
std::string SqlBuilderUtil::EscapeNormalObjectName(
    const std::string& object_name,
    std::shared_ptr<BuildContext> sql_builder_context) {
  if (object_name.empty()) {
    return object_name;
  }
  if (sql_builder_context->use_escape_string &&
      !Util::StartsWith(object_name, sql_builder_context->escape_char)) {
    return AddEscapeString(object_name, sql_builder_context->escape_char);
  }
  return object_name;
}
std::string SqlBuilderUtil::AddEscapeString(const std::string& object_name,
                                            const std::string& escape_char) {
  return escape_char + object_name + escape_char;
}

std::string SqlBuilderUtil::GetRealTableName(
    ObjectPtr object, std::shared_ptr<BuildContext> sql_builder_context) {
  return BuildRealTableName(object->GetCatalog().GetRawCatalogName(),
                            object->GetRawObjectName(), sql_builder_context,
                            true);
}
std::string SqlBuilderUtil::GetRealTableName(
    const Catalog& catalog, const RawConstant& object_name,
    std::shared_ptr<BuildContext> sql_builder_context) {
  return BuildRealTableName(catalog, object_name, sql_builder_context, true);
}

std::string SqlBuilderUtil::GetRealColumnName(
    ObjectPtr object, std::shared_ptr<BuildContext> sql_builder_context) {
  RawConstant column_name;
  switch (object->GetObjectType()) {
    case TABLE_COLUMN_OBJECT:
      column_name = std::dynamic_pointer_cast<object::TableColumnObject>(object)
                        ->GetRawColumnNameToAdd();
      break;

    case ALTER_TABLE_COLUMN_OBJECT:
      column_name =
          std::dynamic_pointer_cast<object::AlterTableColumnObject>(object)
              ->GetRawColumnNameToAlter();
      break;
    default:
      break;
  }

  return GetRealColumnName(object->GetCatalog(), object->GetRawObjectName(),
                           column_name, sql_builder_context);
}

std::string SqlBuilderUtil::GetRealColumnNames(
    const Catalog& catalog, const RawConstant& object_name,
    std::shared_ptr<std::vector<RawConstant>> names,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  if (names == nullptr) {
    return "";
  }
  for (int i = 0; i < names->size(); i++) {
    if (i > 0) {
      line.append(",");
    }
    line.append(GetRealColumnName(catalog, object_name, names->at(i),
                                  sql_builder_context));
  }
  return line;
}

std::string SqlBuilderUtil::GetRealColumnName(
    const Catalog& catalog, const RawConstant& object_name,
    const RawConstant& name,
    std::shared_ptr<BuildContext> sql_builder_context) {
  bool use_orig_name = sql_builder_context->use_origin_identifier;
  return EscapeNormalObjectName(Util::RawConstantValue(name, use_orig_name),
                                sql_builder_context);
}

std::string SqlBuilderUtil::BuildComment(
    std::shared_ptr<CommentObject> comment_object) {
  std::string builder(" COMMENT ");
  builder.append(comment_object->GetCommentPair().comment);
  return builder;
}

std::string SqlBuilderUtil::BuildPosition(
    std::shared_ptr<TableColumnObject> table_column_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  auto col_position_identifier =
      table_column_object->GetColPositionIdentifier();
  if (col_position_identifier != nullptr) {
    switch (col_position_identifier->GetColPositionLocator()) {
      case ColumnLocationIdentifier::ColPositionLocator::HEAD:
        line.append(" FIRST");
        break;

      case ColumnLocationIdentifier::ColPositionLocator::BEFORE:
        line.append(" BEFORE ")
            .append(SqlBuilderUtil::GetRealColumnName(
                table_column_object->GetCatalog(),
                table_column_object->GetRawObjectName(),
                col_position_identifier->GetAdjacentColumnBefore(),
                sql_builder_context));
        break;

      case ColumnLocationIdentifier::ColPositionLocator::AFTER:
        line.append(" AFTER ").append(SqlBuilderUtil::GetRealColumnName(
            table_column_object->GetCatalog(),
            table_column_object->GetRawObjectName(),
            col_position_identifier->GetAdjacentColumnAfter(),
            sql_builder_context));
        break;

      case ColumnLocationIdentifier::ColPositionLocator::TAIL:
        break;

      default:
        sql_builder_context->SetErrMsg("column position not recognized");
        break;
    }
  }
  return line;
}

}  // namespace sink

}  // namespace etransfer
