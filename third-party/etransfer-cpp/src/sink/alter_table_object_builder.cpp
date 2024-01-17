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

#include "sink/alter_table_object_builder.h"

#include "sink/object_builder_mapper.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings AlterTableObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  Strings result;
  std::string line;
  auto alter_table_object =
      std::dynamic_pointer_cast<AlterTableObject>(db_object);
  if (alter_table_object->GetSubObjects().empty()) {
    sql_builder_context->SetErrMsg("sub object is empty, skip sql building.");
    return result;
  }

  const auto& sub_objects = alter_table_object->GetSubObjects();

  Strings dependent_ddl =
      BuildIndependentAlterStatements(alter_table_object, sql_builder_context);
  if (!sql_builder_context->succeed) {
    std::cerr << "sub-object build failed" << std::endl;
    return result;
  }
  line.append("ALTER TABLE ")
      .append(SqlBuilderUtil::GetRealTableName(alter_table_object,
                                               sql_builder_context))
      .append("\n");
  // build body;
  bool append_valid = false;
  for (const auto& object : sub_objects) {
    if (object->GetObjectType() == ObjectType::COMMENT_OBJECT) {
      auto comment_object = std::dynamic_pointer_cast<CommentObject>(object);
      if (comment_object->IsTableComment())
        line.append(SqlBuilderUtil::BuildComment(comment_object))
            .append(", \n");
      append_valid = true;
    } else {
      auto builder =
          ObjectBuilderMapper::GetObjectBuilder(object->GetObjectType());
      if (builder == nullptr) {
        sql_builder_context->succeed = false;
        break;
      }
      auto strs = builder->BuildSql(object, db_object, sql_builder_context);
      if (!sql_builder_context->succeed) {
        return result;
      }
      for (auto str : strs) {
        line.append("\t").append(str).append(", \n");
      }
      append_valid = true;
    }
  }

  if (append_valid) {
    // remove last comma
    line = line.substr(0, line.length() - 3);
    line.append("\n");
    result.push_back(line);
  }

  result.insert(result.end(), dependent_ddl.begin(), dependent_ddl.end());
  return result;
}
Strings AlterTableObjectBuilder::BuildIndependentAlterStatements(
    std::shared_ptr<AlterTableObject> alter_table_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  Strings dependent_ddl;
  auto& sub_objects = alter_table_object->GetSubObjects();
  for (auto iterator = sub_objects.begin(); iterator != sub_objects.end();) {
    auto object = *iterator;
    std::string dependent_action;
    switch (object->GetObjectType()) {
      case ObjectType::DROP_TABLE_PARTITION_OBJECT:
      case ObjectType::TRUNCATE_TABLE_PARTITION_OBJECT:
        dependent_action = BuildIndependentStatement(alter_table_object, object,
                                                     sql_builder_context);
        if (!dependent_action.empty()) {
          dependent_ddl.push_back(dependent_action);
        }
        iterator = sub_objects.erase(iterator);
        break;
      default:
        iterator++;
        break;
    }
  }

  return dependent_ddl;
}

std::string AlterTableObjectBuilder::BuildIndependentStatement(
    std::shared_ptr<AlterTableObject> alter_table_object,
    ObjectPtr schema_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  line.append("ALTER TABLE ")
      .append(SqlBuilderUtil::GetRealTableName(alter_table_object,
                                               sql_builder_context))
      .append("\n");
  auto builder =
      ObjectBuilderMapper::GetObjectBuilder(schema_object->GetObjectType());
  if (builder == nullptr) {
    return "";
  }

  Strings sub_object =
      builder->BuildSql(schema_object, alter_table_object, sql_builder_context);
  if (!sql_builder_context->succeed) {
    return "";
  }
  // build body;
  auto sub_object_str = Util::StringJoin(sub_object, " \n ");
  if (!sub_object_str.empty()) line.append("\t").append(sub_object_str);

  return line;
}
}  // namespace sink

}  // namespace etransfer