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

#include "object/expr_token.h"
#include "object/object.h"
namespace etransfer {
namespace object {

class TableCheckConstraint : public Object {
 public:
  std::string GetConstraintName() {
    return Util::RawConstantValue(constraint_name_);
  }

  std::string GetConstraintName(bool origin) {
    return Util::RawConstantValue(constraint_name_, origin);
  }

  TableCheckConstraint(
      const Catalog& catalog, const RawConstant& object_name,
      const RawConstant& constraint_name, const std::string& expression,
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> inner_tokens)
      : Object(catalog, object_name, ObjectType::TABLE_CHECK_CONSTRAINT_OBJECT),
        constraint_name_(constraint_name),
        expression_(expression),
        inner_tokens_(inner_tokens) {}

  std::string GetExpression() const { return expression_; }

  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> GetInnerTokens()
      const {
    return inner_tokens_;
  }

 private:
  RawConstant constraint_name_;
  std::string expression_;
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> inner_tokens_;
};
}  // namespace object

}  // namespace etransfer
