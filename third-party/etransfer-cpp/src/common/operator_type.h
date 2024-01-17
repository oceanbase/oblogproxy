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
#include <stdint.h>
namespace etransfer {
namespace common {
enum class OperatorType {
  // or , ||
  OR_SYMBOL,
  LOGICAL_OR_OPERATOR,

  // and , &&
  AND_SYMBOL,
  LOGICAL_AND_OPERATOR,

  // xor
  XOR_SYMBOL,
  // not
  NOT_SYMBOL,
  // is
  IS_SYMBOL,
  // is not
  IS_NOT_SYMBOL,
  // is null
  IS_NULL_SYMBOL,
  // is not null
  IS_NOT_NULL_SYMBOL,
  // =
  EQUAL_OPERATOR,
  // <=>
  NULL_SAFE_EQUAL_OPERATOR,
  // >=
  GREATER_OR_EQUAL_OPERATOR,
  // >
  GREATER_THAN_OPERATOR,
  // <=
  LESS_OR_EQUAL_OPERATOR,
  // <
  LESS_THAN_OPERATOR,
  // !=
  NOT_EQUAL_OPERATOR,
  // <>
  NOT_EQUAL_COMMON_OPERATOR,

  // between and
  BETWEEN_AND_SYMBOL,
  // in
  IN_SYMBOL,
  // like
  LIKE_SYMBOL,
  // between and
  NOT_BETWEEN_AND_SYMBOL,
  // in
  NOT_IN_SYMBOL,
  // like
  NOT_LIKE_SYMBOL,

  //^
  BITWISE_XOR_OPERATOR,
  //*
  MULT_OPERATOR,
  // /
  DIV_OPERATOR,
  // %
  MOD_OPERATOR,
  // DIV
  DIV_SYMBOL,
  // MOD
  MOD_SYMBOL,
  // +
  PLUS_OPERATOR,
  // -
  MINUS_OPERATOR,
  // + INTERVAL
  PLUS_INTERVAL_OPERATOR,
  // - INTERVAL
  MINUS_INTERVAL_OPERATOR,
  // <<
  SHIFT_LEFT_OPERATOR,
  // >>
  SHIFT_RIGHT_OPERATOR,
  // &
  BITWISE_AND_OPERATOR,
  // |
  BITWISE_OR_OPERATOR,
  // REGEXP
  REGEXP_SYMBOL,
  // NOT REGEXP
  NOT_REGEXP_SYMBOL,
  // ~
  TILDE_OPERATOR,
  NOT_TILDE_OPERATOR,
  CASE_WHEN,

  JSON_UNQUOTED_SEPARATOR_SYMBOL,
  JSON_SEPARATOR_SYMBOL,

  // use in  between and 、 in operator to cut params, for example :
  // simpleExpr(age,between and,simpleExpr(1,DOT_OPERATOR,10))
  DOT_OPERATOR,
  // use in column, such as databaseName.tableName -> new
  // simpleExpr(databaseName, PERIOD_OPERATOR, tableName)
  PERIOD_OPERATOR,
  TYPE_CAST,
  // use in expr [space] expr ,such as simpleExpr(literal(BINARY),
  // SPACE_OPERATOR, expr) ==>  BINARY expr
  SPACE_OPERATOR,
  UNKNOWN,
};
class OperatorTypeUtil {
 public:
  static const char* const operator_type_names[];
  static const char* GetTypeName(OperatorType type);
};
}  // namespace common

}  // namespace etransfer
