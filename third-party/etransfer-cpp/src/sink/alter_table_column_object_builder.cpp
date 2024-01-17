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

#include "sink/alter_table_column_object_builder.h"

#include "sink/mysql_datatype_converter.h"
#include "sink/object_builder_mapper.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings AlterTableColumnObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<AlterTableColumnObject> alter_table_column_object =
      std::dynamic_pointer_cast<AlterTableColumnObject>(db_object);
  Strings res;
  std::string line;
  bool action_appended = false;
  std::string mapped_column_name = SqlBuilderUtil::GetRealColumnName(
      alter_table_column_object, sql_builder_context);
  bool append_comment_and_position = true;

  // there is only one change default, this is a set default.
  if (alter_table_column_object->GetAlterColumnType()->size() == 1 &&
      alter_table_column_object->GetAlterColumnType()->at(0) ==
          AlterTableColumnObject::AlterColumnType::CHANGE_DEFAULT) {
    line.append("ALTER COLUMN ")
        .append(mapped_column_name)
        .append(" SET DEFAULT ");
    std::string default_value =
        expr_builder_.Build(alter_table_column_object->GetDefaultValue(),
                            RealDataType::UNSUPPORTED, sql_builder_context);
    line.append(default_value);
  } else if (alter_table_column_object->GetAlterColumnType()->size() >= 1 &&
             alter_table_column_object->GetAlterColumnType()->at(0) ==
                 AlterTableColumnObject::AlterColumnType::REDEFINE_COLUMN) {
    auto new_col_def = alter_table_column_object->GetModifiedColumnDefinition();
    auto builder =
        ObjectBuilderMapper::GetObjectBuilder(new_col_def->GetObjectType());
    if (builder == nullptr) {
      sql_builder_context->SetErrMsg("unsupported objectType");
      return res;
    }
    line.append("CHANGE COLUMN ").append(mapped_column_name);
    line.append(Util::StringJoin(
        builder->BuildSql(new_col_def, db_object, sql_builder_context), ""));
    // change column sql already append column comment and position
    append_comment_and_position = false;
  } else {
    for (AlterTableColumnObject::AlterColumnType alter_column_type :
         *alter_table_column_object->GetAlterColumnType()) {
      if (alter_column_type ==
          AlterTableColumnObject::AlterColumnType::DROP_DEFAULT) {
        line.append("ALTER COLUMN ")
            .append(mapped_column_name)
            .append(" DROP DEFAULT ");
      } else {
        if (!action_appended) {
          line.append("MODIFY ").append(mapped_column_name).append(" ");
          action_appended = true;
        }
        switch (alter_column_type) {
          case AlterTableColumnObject::AlterColumnType::CHANGE_DEFAULT:
            line.append(" ").append(" DEFAULT ");
            line.append(expr_builder_.Build(
                alter_table_column_object->GetDefaultValue(),
                RealDataType::UNSUPPORTED, sql_builder_context));
            break;
          case AlterTableColumnObject::AlterColumnType::CHANGE_NULLABLE:
            if (alter_table_column_object->GetNullable()) {
              line.append(" NULL ");
            } else {
              line.append(" NOT NULL ");
            }
            break;
          case AlterTableColumnObject::AlterColumnType::CHANGE_DATA_TYPE: {
            auto data_type_integer_integer_triple =
                alter_table_column_object->GetDataType();
            line.append(MySQLDataTypeConverterMapper::GetConverter(
                            DataTypeUtil::GetGenericDataType(
                                data_type_integer_integer_triple.first))(
                            data_type_integer_integer_triple.first,
                            sql_builder_context,
                            data_type_integer_integer_triple.second)
                            .second);
            break;
          }
          default:
            break;
        }
      }
    }
  }

  if (append_comment_and_position) {
    // build column comment before position
    line.append(BuildColumnComment(alter_table_column_object));
    line.append(SqlBuilderUtil::BuildPosition(
        alter_table_column_object->GetModifiedColumnDefinition(),
        sql_builder_context));
  }
  res.push_back(line);
  return res;
}

std::string AlterTableColumnObjectBuilder::BuildColumnComment(
    std::shared_ptr<AlterTableColumnObject> alter_table_column_object) {
  std::string extra_info;
  auto sub_objects = alter_table_column_object->GetSubObjects();
  for (auto object : sub_objects) {
    if (std::shared_ptr<CommentObject> comment_object =
            std::dynamic_pointer_cast<CommentObject>(object)) {
      if (comment_object->IsColumnComment() &&
          comment_object->IsCommentForObject(
              alter_table_column_object->GetColumnNameToAlter())) {
        extra_info = SqlBuilderUtil::BuildComment(comment_object);
        break;
      }
    }
  }

  return extra_info;
}
}  // namespace sink

}  // namespace etransfer
