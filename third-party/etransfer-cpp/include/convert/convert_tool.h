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
#include <memory>
#include <string>

#include "ddl_parser_context.h"
#include "sql_builder_context.h"
namespace etransfer {
namespace tool {
using namespace source;
using namespace sink;
// It is currently used to convert DDL statements
// from OceanBase MySQL mode to MySQL fully compatible DDL statements.
class ConvertTool {
 public:
  /*
  Convert DDL statements from OceanBase MySQL mode
  to MySQL fully compatible DDL statements,
  return 0 if success.

  @param origin               OceanBase MySQL mode DDL statement
  @param default_schema       default schema name
  @param is_case_sensitive    whther or not case sensitive
  @param dest                 MySQL fully compatible DDL statement
  @param err_msg              error message if parse failed
  */
  static int Parse(const std::string& origin, const std::string& default_schema,
                   bool is_case_sensitive, std::string& dest,
                   std::string& err_msg);

  /*
  Similar to the ConvertTool::Parse, but allows
  for customization of context parameters.

  @param origin               OceanBase MySQL mode DDL statement
  @param parse_context        parse context
  @param build_context        build context
  @param dest                 MySQL fully compatible DDL statement
  @param err_msg              error message if parse failed
  */
  static int ParseWithContext(const std::string& origin,
                              std::shared_ptr<ParseContext> parse_context,
                              std::shared_ptr<BuildContext> build_context,
                              std::string& dest, std::string& err_msg);
};
};  // namespace tool
};  // namespace etransfer