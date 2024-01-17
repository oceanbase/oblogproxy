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

#include "source/obmysql_ddl_source.h"

#include "source/obmysql_object_parser.h"
namespace etransfer {
namespace source {
ObjectPtr OBMySQLDDLSource::RealBuildObject(
    std::shared_ptr<ParseContext> context) {
  auto object_parser = std::make_shared<OBMySQLObjectParser>(context);
  InitParser(context->raw_ddl);
  ParseTree(ob_parser->sql_stmt(), object_parser);
  if (!error_listener.message.empty()) {
    context->SetErrMsg(error_listener.message);
  }
  if (!context->succeed) {
    return nullptr;
  }
  return object_parser->parent_object;
}

void OBMySQLDDLSource::ParseTree(
    antlr4::tree::ParseTree* parse_tree,
    std::shared_ptr<antlr4::tree::ParseTreeListener> listener) {
  antlr4::tree::ParseTreeWalker::DEFAULT.walk(listener.get(), parse_tree);
}

void OBMySQLDDLSource::InitParser(const std::string& ddl) {
  input = std::make_shared<antlr4::ANTLRInputStream>(ddl);
  lexer = std::make_shared<oceanbase::OBLexer>(input.get());
  tokens = std::make_shared<antlr4::CommonTokenStream>(lexer.get());
  ob_parser = std::make_shared<oceanbase::OBParser>(tokens.get());
  ob_parser->removeErrorListeners();
  ob_parser->addErrorListener(&error_listener);
  ob_parser->setTrace(false);
}

};  // namespace source
};  // namespace etransfer