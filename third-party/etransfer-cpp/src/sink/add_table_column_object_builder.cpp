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

#include "sink/add_table_column_object_builder.h"

#include "object/comment_object.h"
#include "sink/mysql_datatype_converter.h"
#include "sink/object_builder_mapper.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings AddTableColumnObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  Strings res;
  std::string line("\t");
  std::shared_ptr<object::TableColumnObject> table_column_object =
      std::dynamic_pointer_cast<object::TableColumnObject>(db_object);
  if (parent_object->GetObjectType() == ObjectType::ALTER_TABLE_OBJECT) {
    line.append("ADD COLUMN ");
  }
  line.append(SqlBuilderUtil::GetRealColumnName(table_column_object,
                                                sql_builder_context))
      .append(" ");
  auto converter = MySQLDataTypeConverterMapper::GetConverter(
      DataTypeUtil::GetGenericDataType(table_column_object->GetDataType()));
  if (!converter) {
    return res;
  }
  auto mapped_data_type_and_str =
      converter(table_column_object->GetDataType(), sql_builder_context,
                table_column_object->GetTypeInfo());
  line.append(mapped_data_type_and_str.second);

  if (nullptr != table_column_object->GetGenExpr()) {
    line.append(BuildGenExpr(table_column_object,
                             mapped_data_type_and_str.first,
                             sql_builder_context));
    if (!sql_builder_context->succeed) {
      return res;
    }
  }

  if (nullptr != table_column_object->GetDefaultValue()) {
    std::string default_value = default_value_expr_builder_.Build(
        table_column_object->GetDefaultValue(), mapped_data_type_and_str.first,
        sql_builder_context);
    if (!default_value.empty()) {
      line.append(" DEFAULT ").append(default_value);
    }
  }

  // NOT NULL should be after DEFAULT VALUE
  if (!table_column_object->IsNullable()) {
    line.append(" NOT NULL ");
  } else {
    if (mapped_data_type_and_str.first == RealDataType::TIMESTAMP) {
      line.append(" NULL ");
    }
  }

  if (table_column_object->GetColumnAttributes()->IsSpatialRefIdSet()) {
    line.append(BuildSpatialRefId(table_column_object, sql_builder_context));
  }

  line.append(BuildCheckConstraint(table_column_object, sql_builder_context));

  if (table_column_object->IsAutoInc()) {
    line.append(" AUTO_INCREMENT ");
  }

  if (table_column_object->HasOnUpdate()) {
    line.append(BuildOnUpdate(table_column_object,
                              mapped_data_type_and_str.first,
                              sql_builder_context));
  }
  const auto& sub_objects = parent_object->GetSubObjects();
  for (const auto& sub_object : sub_objects) {
    if (sub_object->GetObjectType() == ObjectType::COMMENT_OBJECT) {
      std::shared_ptr<CommentObject> comment_object =
          std::dynamic_pointer_cast<CommentObject>(sub_object);
      if (comment_object->GetCommentTargetType() ==
          CommentObject::CommentTargetType::COLUMN) {
        CommentObject::CommentPair comment_pair =
            comment_object->GetCommentPair();
        if (comment_pair.comment_target.value ==
            table_column_object->GetColumnNameToAdd()) {
          line.append(" COMMENT ").append(comment_pair.comment);
        }
      }
    }
  }

  line.append(BuildPosition(table_column_object, sql_builder_context));
  res.push_back(line);
  return res;
}

std::string AddTableColumnObjectBuilder::BuildCheckConstraint(
    std::shared_ptr<TableColumnObject> table_column_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;

  for (const auto& object : table_column_object->GetSubObjects()) {
    if (object->GetObjectType() == ObjectType::TABLE_CHECK_CONSTRAINT_OBJECT) {
      auto converter =
          ObjectBuilderMapper::GetObjectBuilder(object->GetObjectType());
      if (converter == nullptr) {
        continue;
      }
      Strings constaints =
          converter->BuildSql(object, table_column_object, sql_builder_context);
      for (const auto& constraint : constaints) {
        line.append(constraint).append(" ");
      }
    }
  }
  return line;
}

std::string AddTableColumnObjectBuilder::BuildSpatialRefId(
    std::shared_ptr<TableColumnObject> table_column_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  if (MYSQL_80_VERSION > sql_builder_context->target_db_version) {
    sql_builder_context->SetErrMsg(
        "mysql doesn't support column SRID attribute before 8.0");
    return "";
  } else if (table_column_object->GetColumnAttributes()->GetSpatialRefId() ==
             "") {
    sql_builder_context->SetErrMsg("column attributes spatialRefId is empty");
    return "";
  }
  line.append(" SRID ")
      .append(table_column_object->GetColumnAttributes()->GetSpatialRefId())
      .append(" ");
  return line;
}
std::string AddTableColumnObjectBuilder::BuildOnUpdate(
    std::shared_ptr<TableColumnObject> table_column_object,
    RealDataType data_type, std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  if (CheckVersionForOnUpdate(sql_builder_context, data_type)) {
    line.append(" ON UPDATE ").append(table_column_object->GetOnUpdate());
  }
  return line;
}

bool AddTableColumnObjectBuilder::CheckVersionForOnUpdate(
    std::shared_ptr<BuildContext> context, RealDataType data_type) {
  // if version is before 5.6.0 , datetime type don't allow ON UPDATE clause,
  // timestamp type allow
  if (context->target_db_version < MYSQL_56_VERSION &&
      data_type == RealDataType::DATETIME) {
    return false;
  }
  return true;
}

std::string AddTableColumnObjectBuilder::BuildPosition(
    std::shared_ptr<TableColumnObject> table_column_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  // mysql does not support the BEFORE keyword
  if (nullptr != table_column_object->GetColPositionIdentifier() &&
      ColumnLocationIdentifier::ColPositionLocator::BEFORE !=
          table_column_object->GetColPositionIdentifier()
              ->GetColPositionLocator()) {
    line.append(SqlBuilderUtil::BuildPosition(table_column_object,
                                              sql_builder_context));
  }
  return line;
}

bool AddTableColumnObjectBuilder::CheckVersionForGenExpr(
    std::shared_ptr<BuildContext> sql_builder_context) {
  if (sql_builder_context->target_db_version < MYSQL_57_VERSION) {
    std::cerr << "unsupported generated column" << std::endl;
    return false;
  } else {
    return true;
  }
}
std::string AddTableColumnObjectBuilder::BuildGenExpr(
    std::shared_ptr<TableColumnObject> table_column_object,
    RealDataType data_type, std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  if (CheckVersionForGenExpr(sql_builder_context)) {
    line.append(" GENERATED ALWAYS AS ");
    bool is_wrapped =
        table_column_object->GetGenExpr()->GetIsWrappedByBrackets();
    line.append(is_wrapped ? "" : "(")
        .append(expr_builder_.Build(table_column_object->GetGenExpr(),
                                    data_type, sql_builder_context))
        .append(is_wrapped ? "" : ") ");
    if (GenStoreType::INVALID != table_column_object->GetGenStoreType()) {
      line.append(ColumnAttributes::GetGenStoreName(
                      table_column_object->GetGenStoreType()))
          .append(" ");
    }
  }
  return line;
}
}  // namespace sink

}  // namespace etransfer