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

#include "object/table_check_constraint.h"
#include "sink/check_constraint_object_builder.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings CheckConstraintObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<TableCheckConstraint> check_constraint =
      std::dynamic_pointer_cast<TableCheckConstraint>(db_object);
  Strings res;
  std::string line("\t");
  if (parent_object->GetObjectType() == ObjectType::ALTER_TABLE_OBJECT) {
    line.append("ADD ");
  }

  if (check_constraint->GetConstraintName() != "") {
    line.append("CONSTRAINT ")
        .append(SqlBuilderUtil::EscapeNormalObjectName(
            check_constraint->GetConstraintName(
                sql_builder_context->use_origin_identifier),
            sql_builder_context))
        .append(" ");
  }

  line.append("CHECK (")
      .append(GetAssemblyTokenString(check_constraint, sql_builder_context))
      .append(")");

  res.push_back(line);
  return res;
}

std::string CheckConstraintObjectBuilder::GetAssemblyTokenString(
    std::shared_ptr<TableCheckConstraint> check_constraint,
    std::shared_ptr<BuildContext> sql_builder_context) {
  const auto tokens = check_constraint->GetInnerTokens();
  if (tokens->empty()) {
    return check_constraint->GetExpression();
  }
  std::string line;
  bool contains_identifier_token = false;

  for (auto t : *tokens) {
    switch (t->GetTokenType()) {
      case ExprTokenType::IDENTIFIER_NAME:
        line.append(SqlBuilderUtil::GetRealColumnName(
            check_constraint->GetCatalog(),
            check_constraint->GetRawObjectName(), t->token,
            sql_builder_context));
        contains_identifier_token = true;
        break;
      default:
        line.append(Util::RawConstantValue(
            t->token, sql_builder_context->use_origin_identifier));
        break;
    }
    line.append(" ");
  }
  if (!contains_identifier_token) {
    return check_constraint->GetExpression();
  }
  return line;
}

}  // namespace sink

}  // namespace etransfer
