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

#pragma once

#include "sql_cmd.h"
#include "log.h"
#include "SQLParserResult.h"

#include <regex>
#include <string>

namespace oceanbase {
namespace binlog {
class ObSqlParser {
public:
  /*!
   * @brief Parse the passed sql statement and return the parsed sql set.
   * Returns OMS_OK if parsing is successful, otherwise returns OMS_FAILED
   * @param sql
   * @param result @link{hsql::SQLParserResult}
   * @return
   */
  static int parse(const std::string& sql, hsql::SQLParserResult& result);
};

}  // namespace binlog
}  // namespace oceanbase
