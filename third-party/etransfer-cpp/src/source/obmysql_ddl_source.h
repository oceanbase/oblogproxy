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
#include <antlr4-runtime.h>

#include "OBLexer.h"
#include "OBParser.h"
#include "source/ddl_source.h"
#include "source/error_listener.h"

namespace etransfer {
namespace source {
using namespace oceanbase;
// OBMySQLDDLSource parse OceanBase MySQL mode DDL statements
class OBMySQLDDLSource : public DDLSource {
 public:
  OBMySQLDDLSource() {}
  ObjectPtr RealBuildObject(std::shared_ptr<ParseContext> args);
  void ParseTree(antlr4::tree::ParseTree* parse_tree,
                 std::shared_ptr<antlr4::tree::ParseTreeListener> listener);

 private:
  void InitParser(const std::string& ddl);
  std::shared_ptr<antlr4::ANTLRInputStream> input;
  std::shared_ptr<oceanbase::OBLexer> lexer;
  std::shared_ptr<antlr4::CommonTokenStream> tokens;
  std::shared_ptr<oceanbase::OBParser> ob_parser;
  ErrorListener error_listener;
};
};  // namespace source
};  // namespace etransfer