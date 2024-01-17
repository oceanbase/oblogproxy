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

#include "common/operator_type.h"
namespace etransfer {
namespace common {
const char* const OperatorTypeUtil::operator_type_names[] = {
    "OR",
    "||",
    "AND",
    "&&",
    "XOR",
    "NOT",
    "IS",
    "IS NOT",
    "IS NULL",
    "IS NOT NULL",
    "=",
    "<=>",
    ">=",
    ">",
    "<=",
    "<",
    "!=",
    "<>",
    "BETWEEN AND",
    "IN",
    "LIKE",
    "NOT BETWEEN AND",
    "NOT IN",
    "NOT LIKE",
    "^",
    "*",
    "/",
    "%",
    "DIV",
    "MOD",
    "+",
    "-",
    "+ INTERVAL",
    "- INTERVAL",
    "<<",
    ">>",
    "&",
    "|",
    "REGEXP",
    "NOT REGEXP",
    "~",
    "!~",
    "CASE WHEN",
    "->>",
    "->",
    ",",
    ".",
    "::",
    " ",
    "",  // UNKNOWN
};
static_assert(sizeof(OperatorTypeUtil::operator_type_names) / sizeof(char*) ==
                  static_cast<int>(OperatorType::UNKNOWN) + 1,
              "OperatorType sizes don't match");
const char* OperatorTypeUtil::GetTypeName(OperatorType type) {
  return operator_type_names[static_cast<int>(type)];
}
}  // namespace common

}  // namespace etransfer
