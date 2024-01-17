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

#include "sink/create_table_object_builder.h"

#include "object/create_table_object.h"
#include "sink/object_builder_mapper.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
using namespace object;
Strings CreateTableObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<object::CreateTableObject> create_table_object =
      std::dynamic_pointer_cast<object::CreateTableObject>(db_object);
  Strings res;
  std::string line;
  line.append(
      BuildCreateTableHeadPart(create_table_object, sql_builder_context));
  if (create_table_object->GetCreateTableDdlTricks()->is_create_like) {
    line.append(BuildCreateLikeBody(create_table_object, sql_builder_context));
    res.push_back(line);
  } else {
    if (BuildCreateNormalTableBody(create_table_object,
                                   create_table_object->GetSubObjects(),
                                   sql_builder_context, line)) {
      res.push_back(line);
    } else {
      res.push_back("");
    }
  }
  return res;
}
bool CreateTableObjectBuilder::BuildCreateNormalTableBody(
    std::shared_ptr<CreateTableObject> table_object,
    const ObjectPtrs& create_table_sub_actions,
    std::shared_ptr<BuildContext> sql_builder_context, std::string& line) {
  line.append("(\n");
  // build body;
  if (create_table_sub_actions.empty()) {
    return false;
  }

  Strings body;
  for (const auto& object : create_table_sub_actions) {
    auto builder =
        ObjectBuilderMapper::GetObjectBuilder(object->GetObjectType());
    if (builder == nullptr) {
      return false;
    }
    auto object_strs =
        builder->BuildSql(object, table_object, sql_builder_context);
    if (!sql_builder_context->succeed) {
      return false;
    }
    auto object_str = Util::StringJoin(object_strs, ",");
    if (!object_str.empty()) body.push_back(object_str);
  }
  line.append(Util::StringJoin(body, ",\n"));

  if (line.substr(line.length() - 2, 2) == ",\n") {
    line = line.substr(0, line.length() - 2);
  }
  line.append("\n)")
      .append(BuildTableAttributes(table_object, sql_builder_context))
      .append(BuildExtraInfo(table_object, sql_builder_context));
  return true;
}
std::string CreateTableObjectBuilder::BuildCreateTableHeadPart(
    std::shared_ptr<object::CreateTableObject> table_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  line.append("CREATE");
  if (table_object->GetCreateTableDdlTricks()->is_temporary_table) {
    line.append(" TEMPORARY ");
  }
  line.append(" TABLE ");
  if (table_object->GetCreateTableDdlTricks()->create_if_not_exists) {
    line.append(" IF NOT EXISTS ");
  }
  line.append(SqlBuilderUtil::BuildRealTableName(
      table_object->GetCatalog().GetRawCatalogName(),
      table_object->GetRawObjectName(), sql_builder_context, true));
  return line;
}

std::string CreateTableObjectBuilder::BuildCreateLikeBody(
    std::shared_ptr<object::CreateTableObject> table_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  line.append(" LIKE ");
  auto like_object =
      table_object->GetCreateTableDdlTricks()->create_like_syntax;
  line.append(SqlBuilderUtil::BuildRealTableName(
      like_object->GetCatalog().GetRawCatalogName(),
      like_object->GetRawObjectName(), sql_builder_context, true));
  return line;
}

std::string CreateTableObjectBuilder::BuildTableAttributes(
    std::shared_ptr<CreateTableObject> db_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  return BuildOptions(db_object->TableOptionDescribes(), ", ");
}

std::string CreateTableObjectBuilder::BuildExtraInfo(
    std::shared_ptr<CreateTableObject> db_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  for (const auto& sub_object : db_object->GetCreateTableSubActions()) {
    if (sub_object->GetObjectType() == ObjectType::COMMENT_OBJECT) {
      std::shared_ptr<CommentObject> comment_object =
          std::dynamic_pointer_cast<CommentObject>(sub_object);
      if (comment_object != nullptr && comment_object->IsTableComment()) {
        line.append(SqlBuilderUtil::BuildComment(comment_object));
        break;
      }
    }
  }
  auto extra_objects = db_object->GetExtraObject();
  Strings body;
  for (const auto& object : *extra_objects) {
    auto object_strs = ObjectBuilderMapper::SqlBuilderMapper()
                           .at(object->GetObjectType())
                           ->BuildSql(object, db_object, sql_builder_context);
    if (!sql_builder_context->succeed) {
      if (sql_builder_context->err_msg == "unsupported partition combination") {
        return "";
      }
      std::cerr << "build create table extra object failed: "
                << sql_builder_context->err_msg << std::endl;
      sql_builder_context->Reset();
      continue;
    }
    body.push_back(Util::StringJoin(object_strs, " "));
  }
  line.append(Util::StringJoin(body, " "));

  return line;
}

std::string CreateTableObjectBuilder::BuildOptions(
    std::shared_ptr<std::vector<std::shared_ptr<Option>>> options,
    const std::string& delim) {
  if (options == nullptr || options->size() == 0) {
    return "";
  }
  std::string ret(" ");
  bool append_delimiter = false;
  for (const auto option : *options) {
    std::string casted;
    switch (option->GetOptionType()) {
      case OptionType::TABLE_CHARSET:
        casted = BuildTableCharset(option);
        break;
      case OptionType::TABLE_COLLATION:
        casted = BuildTableCollation(option);
        break;
      default:
        break;
    }
    if (!casted.empty()) {
      // before first option don't append delimiter
      if (append_delimiter) {
        ret.append(delim);
      }
      ret.append(casted);
      append_delimiter = true;
    }
  }
  return ret;
}

std::string CreateTableObjectBuilder::BuildTableCharset(
    std::shared_ptr<Option> option_object) {
  std::shared_ptr<TableCharsetOption> charset_option =
      std::dynamic_pointer_cast<TableCharsetOption>(option_object);
  return "CHARACTER SET = " + charset_option->charset;
}

std::string CreateTableObjectBuilder::BuildTableCollation(
    std::shared_ptr<Option> option_object) {
  std::shared_ptr<TableCollationOption> collation_option =
      std::dynamic_pointer_cast<TableCollationOption>(option_object);
  return "COLLATE = " + collation_option->collation;
}

}  // namespace sink

}  // namespace etransfer
