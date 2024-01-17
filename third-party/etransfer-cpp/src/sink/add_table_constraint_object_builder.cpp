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

#include "sink/add_table_constraint_object_builder.h"

#include "object/comment_object.h"
#include "object/table_constraint_object.h"
#include "sink/sql_builder_util.h"

namespace etransfer {
namespace sink {
Strings AddTableConstraintObjectSqlBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<TableConstraintObject> table_constraint_object =
      std::dynamic_pointer_cast<TableConstraintObject>(db_object);
  Strings res;
  std::string line("\t");
  if (parent_object == nullptr) {
    line.append("CREATE ");
  } else if (parent_object->GetObjectType() == ObjectType::ALTER_TABLE_OBJECT) {
    line.append("ADD ");
  }

  if (parent_object != nullptr &&
      parent_object->GetObjectType() == ObjectType::CREATE_TABLE_OBJECT &&
      table_constraint_object->GetIndexType() != IndexType::NORMAL) {
    if (table_constraint_object->GetIndexType() == IndexType::PRIMARY) {
      line.append("PRIMARY KEY ");
    } else if (table_constraint_object->GetIndexType() == IndexType::UNIQUE) {
      line.append("UNIQUE KEY ");
      if (table_constraint_object->GetConstraintName().origin_string != "") {
        line.append(SqlBuilderUtil::EscapeNormalObjectName(
                        table_constraint_object->GetConstraintName(),
                        sql_builder_context))
            .append(" ");
      }
    } else {
      sql_builder_context->SetErrMsg("unknown table constraints");
      return res;
    }
    line.append("(");
    const auto& affect_columns =
        table_constraint_object->GetRawAffectedColumns();
    const auto& pre_lengths = table_constraint_object->GetLengthOfPreIndex();

    for (int i = 0; i < affect_columns->size(); i++) {
      if (i > 0) {
        line.append(",");
      }
      std::string column_name = SqlBuilderUtil::GetRealColumnName(
          table_constraint_object->GetCatalog(),
          table_constraint_object->GetRawObjectName(),
          (*affect_columns)[i]->token, sql_builder_context);
      if (pre_lengths != nullptr && i < pre_lengths->size())
        column_name += (*pre_lengths)[i].origin_string;
      line.append(column_name);
    }
    line.append(")");
  } else {
    line.append(BuildIndexType(table_constraint_object->GetIndexType(),
                               sql_builder_context));
    line.append("INDEX ");
    if (table_constraint_object->GetConstraintName().origin_string != "") {
      line.append(SqlBuilderUtil::EscapeNormalObjectName(
          table_constraint_object->GetConstraintName(), sql_builder_context));
    }
    if (parent_object == nullptr) {
      line.append(" ON ").append(SqlBuilderUtil::GetRealTableName(
          table_constraint_object, sql_builder_context));
    }
    line.append("(");
    const auto& constraint_sort_order_map =
        table_constraint_object->GetConstraintSortOrder();
    const auto& affect_columns =
        table_constraint_object->GetRawAffectedColumns();
    const auto& pre_lengths = table_constraint_object->GetLengthOfPreIndex();
    for (int i = 0; i < affect_columns->size(); i++) {
      if (i > 0) {
        line.append(",");
      }
      std::string column_name = SqlBuilderUtil::GetRealColumnName(
          table_constraint_object->GetCatalog(),
          table_constraint_object->GetRawObjectName(),
          (*affect_columns)[i]->token, sql_builder_context);
      if (pre_lengths != nullptr && i < pre_lengths->size())
        column_name += (*pre_lengths)[i].origin_string;
      std::string origin_name = (*affect_columns)[i]->token.origin_string;
      if (nullptr != constraint_sort_order_map &&
          constraint_sort_order_map->count(origin_name) != 0) {
        column_name += BuildColumnSortKeyWord(
            constraint_sort_order_map->at(origin_name).constraint_sort_order);
      }
      line.append(column_name);
    }

    line.append(") ");

    // this is CREATE INDEX statement
    if (parent_object == nullptr) {
      for (const auto& object : table_constraint_object->GetSubObjects()) {
        if (object->GetObjectType() == ObjectType::COMMENT_OBJECT) {
          std::shared_ptr<CommentObject> comment_object =
              std::dynamic_pointer_cast<CommentObject>(object);
          line.append(SqlBuilderUtil::BuildComment(comment_object));
        }
      }
    } else {
      // this is ALTER TABLE ADD INDEX statement
      for (const auto& object : table_constraint_object->GetSubObjects()) {
        if (object->GetObjectType() == ObjectType::COMMENT_OBJECT) {
          std::shared_ptr<CommentObject> comment_object =
              std::dynamic_pointer_cast<CommentObject>(object);
          if (comment_object->GetCommentTargetType() ==
              CommentObject::CommentTargetType::CONSTRAINT) {
            if (comment_object->GetCommentPair().comment_target.value ==
                table_constraint_object->GetConstraintName().value) {
              line.append(SqlBuilderUtil::BuildComment(comment_object));
            }
          }
        }
      }
    }
  }

  res.push_back(line);
  return res;
}

std::string AddTableConstraintObjectSqlBuilder::BuildIndexType(
    IndexType index_type, std::shared_ptr<BuildContext> sql_builder_context) {
  switch (index_type) {
    case UNIQUE:
      return "UNIQUE ";

    case FULLTEXT:
      return "FULLTEXT ";

    case SPATIAL:
      if (MYSQL_57_VERSION > sql_builder_context->target_db_version) {
        sql_builder_context->SetErrMsg("mysql doesn't support spatial index");
        return "";
      }
      return "SPATIAL ";
    case NORMAL:
      break;
    default:
      sql_builder_context->SetErrMsg("mysql doesn't support index type");
      return "";
  }
  return "";
}

std::string AddTableConstraintObjectSqlBuilder::BuildColumnSortKeyWord(
    TableConstraintObject::ConstraintSortOrder sort_order) {
  switch (sort_order) {
    case TableConstraintObject::ConstraintSortOrder::ASC:
      return " ASC";
    case TableConstraintObject::ConstraintSortOrder::DESC:
      return " DESC";
    case TableConstraintObject::ConstraintSortOrder::RANDOM:
      return " RANDOM";
    default:
      return "";
  }
}
}  // namespace sink

}  // namespace etransfer
