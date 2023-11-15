/**
 * Copyright (c) 2023 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "sql_parser.h"
#include "SQLParserResult.h"
#include "SQLParser.h"
#include "util/sqlhelper.h"

namespace oceanbase {
namespace binlog {
int ObSqlParser::parse(const std::string& sql, hsql::SQLParserResult& result)

{
  hsql::SQLParser::parse(sql, &result);

  if (result.isValid()) {
    return OMS_OK;
  } else {
    return OMS_FAILED;
  }
}
}  // namespace binlog
}  // namespace oceanbase