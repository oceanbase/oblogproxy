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

#include "convert/ddl_parser_context.h"
#include "object/object.h"
namespace etransfer {
using namespace object;
namespace source {
// DDLSource converts DDL statements into intermediate object.
class DDLSource {
 public:
  ObjectPtr BuildObject(std::shared_ptr<ParseContext> context) {
    return RealBuildObject(context);
  }

  virtual ObjectPtr RealBuildObject(std::shared_ptr<ParseContext> args) = 0;

  virtual void ParseTree(
      antlr4::tree::ParseTree* parse_tree,
      std::shared_ptr<antlr4::tree::ParseTreeListener> listener){};
};
};  // namespace source
};  // namespace etransfer