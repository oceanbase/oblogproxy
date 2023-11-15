/**
 * Copyright (c) 2021 OceanBase
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

#include <string>
#include "log_record.h"
#include "spdlog/spdlog.h"

namespace oceanbase {
namespace logproxy {

class TraceLog {
public:
  static bool init(uint16_t log_max_file_size_mb, uint16_t log_retention_h);

  static void info(ILogRecord* record);

private:
  static std::string record_digest(ILogRecord* record);

 static void parse_cols(BinLogBuf* binlogBuf, ITableMeta* tableMeta,std::vector<std::string>& pkOrUkColNames, string& digest);
};

}  // namespace logproxy
}  // namespace oceanbase
