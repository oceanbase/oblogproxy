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

#include <map>
#include "str_array.h"
#include "meta_info.h"
#include "rotating_file_with_compress_sink.hpp"
#include "trace_log.h"

namespace oceanbase {
namespace logproxy {

static const std::string TRACE_LOG_BASE_NAME = "trace_record_read.log";
static std::map<int, std::string> LOGGABLE_RECORD_TYPES = {
    {EINSERT, "insert"}, {EUPDATE, "update"}, {EDELETE, "delete"}, {EREPLACE, "replace"}, {EDDL, "ddl"}};
static std::shared_ptr<spdlog::logger> _trace_logger;

bool TraceLog::init(uint16_t log_max_file_size_mb, uint16_t log_retention_h)
{
  std::string log_basename("./log/");
  log_basename.append(TRACE_LOG_BASE_NAME);
  _trace_logger = rotating_with_compress_logger_mt(
      "trace_record_read", log_basename, log_max_file_size_mb * 1024 * 1024, log_retention_h);
  if (nullptr != _trace_logger) {
    _trace_logger->set_level(spdlog::level::info);
    _trace_logger->set_pattern("[%Y-%m-%d %H:%M:%S] %v");
    return true;
  }

  return false;
}

void TraceLog::info(ILogRecord* record)
{
  if (LOGGABLE_RECORD_TYPES.find(record->recordType()) != LOGGABLE_RECORD_TYPES.end()) {
    _trace_logger->info("{}", record_digest(record));
  }
}

std::string TraceLog::record_digest(ILogRecord* record)
{
  std::string digest;
  // mete info
  if (nullptr != record->dbname()) {
    digest.append(record->dbname());
  }
  if (nullptr != record->tbname()) {
    digest.append(".").append(record->tbname());
  }
  digest.append("|").append(to_string(record->getTimestamp()));
  digest.append("|").append(record->getCheckpoint());
  int record_type = record->recordType();
  digest.append("|").append(LOGGABLE_RECORD_TYPES[record_type]);

  // detail info
  unsigned int count;
  if (EDDL == record_type) {
    BinLogBuf* new_buf = record->newCols(count);
    digest.append("|").append(new_buf[0].buf, new_buf[0].buf_used_size);
    return digest;
  }

  ITableMeta* tableMeta = record->getTableMeta();
  std::vector<std::string>& pkOrUkColNames = tableMeta->getPKColNames();
  if (pkOrUkColNames.empty()) {
    pkOrUkColNames = tableMeta->getUKColNames();
  }

  if (EDELETE == record_type || EUPDATE == record_type) {
    BinLogBuf* old_buf = record->oldCols(count);
    parse_cols(old_buf, tableMeta, pkOrUkColNames, digest);
  }
  if (EINSERT == record_type || EUPDATE == record_type || EREPLACE == record_type) {
    BinLogBuf* new_buf = record->newCols(count);
    parse_cols(new_buf, tableMeta, pkOrUkColNames, digest);
  }

  return digest;
}

void TraceLog::parse_cols(
    BinLogBuf* binlogBuf, ITableMeta* tableMeta, std::vector<std::string>& pkOrUkColNames, string& digest)
{
  digest.append("|");
  if (pkOrUkColNames.empty()) {
    digest.append("no-pk/uk, please config enable_output_hidden_primary_key=1 to log value of the hidden primary "
                  "key(__pk_increment)");
    return;
  }

  for (size_t i = 0; i < pkOrUkColNames.size(); ++i) {
    const char* col_name = pkOrUkColNames[i].c_str();
    int col_index = tableMeta->getColIndex(col_name);
    char* col_value = binlogBuf[col_index].buf;
    size_t size = binlogBuf[col_index].buf_used_size;

    digest.append(0 == i ? "" : ",");
    digest.append(col_name).append("=");
    if (nullptr != col_value) {
      digest.append(col_value, size);
    } else {
      digest.append("NULL");
    }
  }
}

}  // namespace logproxy
}  // namespace oceanbase
