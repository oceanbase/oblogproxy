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

#include "convert/convert_tool.h"

#include "object/object.h"
#include "sink/mysql_builder.h"
#include "source/obmysql_ddl_source.h"
namespace etransfer {
using namespace source;
using namespace object;
namespace tool {
int ConvertTool::Parse(const std::string& origin,
                       const std::string& default_schema,
                       bool is_case_sensitive, std::string& dest,
                       std::string& err_msg) {
  std::shared_ptr<sink::BuildContext> builder_context =
      std::make_shared<sink::BuildContext>();
  std::shared_ptr<source::ParseContext> parse_context =
      std::make_shared<ParseContext>(origin, default_schema, is_case_sensitive);
  return ParseWithContext(origin, parse_context, builder_context, dest,
                          err_msg);
}

int ConvertTool::ParseWithContext(const std::string& origin,
                                  std::shared_ptr<ParseContext> parse_context,
                                  std::shared_ptr<BuildContext> builder_context,
                                  std::string& dest, std::string& err_msg) {
  sink::MySqlBuilder builder;
  OBMySQLDDLSource object_source;
  ObjectPtr object = object_source.BuildObject(parse_context);
  if (object == nullptr) {
    std::cerr << "parse sql: " << origin << " error: " << parse_context->err_msg
              << std::endl;
    err_msg = parse_context->err_msg;
    return -1;
  }
  Strings converted_sql_list =
      builder.ApplySchemaObject(object, builder_context);
  if (converted_sql_list.size() == 1) {
    dest = converted_sql_list[0];
  } else {
    for (const auto& converted : converted_sql_list) {
      dest.append(converted).append("\n");
    }
  }
  if (dest.empty() && !builder_context->succeed) {
    std::cerr << "build sql: " << origin
              << " error: " << builder_context->err_msg << std::endl;
    err_msg = builder_context->err_msg;
    return -1;
  }
  return 0;
}

};  // namespace tool
};  // namespace etransfer