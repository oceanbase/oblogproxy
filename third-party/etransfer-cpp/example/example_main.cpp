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

#include <iostream>

#include "convert/convert_tool.h"
#include "convert/ddl_parser_context.h"
#include "convert/sql_builder_context.h"

int main() {
  std::string dest;
  std::string err_msg;
  std::string source = "create table t(a int, b int) BLOCK_SIZE = 16384";
  std::cout << "==========" << "etransfer default parse" << "==========" << std::endl;
  if (!etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg)) {
    std::cout << dest << std::endl;
  } else {
    std::cout << "parse error: " << err_msg << std::endl;
  }

  std::cout << "========" << "etransfer parse with context" << "========" << std::endl;
  auto parse_context = std::make_shared<etransfer::source::ParseContext>(source, "", false);
  auto builder_context = std::make_shared<etransfer::sink::BuildContext>();
  builder_context->use_escape_string = false;
  if (!etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg)) {
    std::cout << dest << std::endl;
  } else {
    std::cout << "parse error: " << err_msg << std::endl;
  }

  return 0;
}
