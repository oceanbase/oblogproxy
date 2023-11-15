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

#include "counter.h"
#include "trace_log.h"
#include "binlog_converter/binlog_converter.h"
#include "clog_reader_routine.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

ClogReaderRoutine::ClogReaderRoutine(BinlogConverter& converter, BlockingQueue<ILogRecord*>& queue)
    : Thread("Clog Reader Routine"), _converter(converter), _oblog(nullptr), _queue(queue)
{}

int ClogReaderRoutine::init(const OblogConfig& config, IObCdcAccess* oblog)
{
  _oblog = oblog;
  std::map<std::string, std::string> configs;
  config.generate_configs(configs);
  if (config.start_timestamp_us.val() != 0) {
    return _oblog->init_with_us(configs, config.start_timestamp_us.val());
  } else {
    return _oblog->init(configs, config.start_timestamp.val());
  }
}

void ClogReaderRoutine::stop()
{
  if (is_run()) {
    Thread::stop();
    _oblog->stop();
    _queue.clear([this](ILogRecord* record) { _oblog->release(record); });
  }
}

void ClogReaderRoutine::run()
{
  if (_oblog->start() != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to start ReaderRoutine";
    return;
  }

  Counter& counter = Counter::instance();
  Timer stage_tm;

  while (is_run()) {
    stage_tm.reset();
    ILogRecord* record = nullptr;
    int ret = _oblog->fetch(record, _s_config.read_timeout_us.val());
    int64_t fetch_us = stage_tm.elapsed();

    if (ret == OB_TIMEOUT && record == nullptr) {
      OMS_STREAM_INFO << "fetch liboblog timeout, nothing incoming...";
      continue;
    }

    if (ret != OB_SUCCESS) {
      OMS_STREAM_ERROR << "Failed to get data from liboblog, the error code is: " << ret;
      break;
    }

    if (_s_config.verbose_record_read.val()) {
      TraceLog::info(record);
    }

    stage_tm.reset();
    counter.count_read_io(record->getRealSize());
    counter.count_read(1);
    while (!_queue.offer(record, _s_config.read_timeout_us.val())) {
      OMS_STREAM_WARN << "reader transfer queue full(" << _queue.size(false) << "), retry...";
    }
    int64_t offer_us = stage_tm.elapsed();

    counter.count_key(Counter::READER_FETCH_US, fetch_us);
    counter.count_key(Counter::READER_OFFER_US, offer_us);
  }
  _converter.stop();
}

}  // namespace logproxy
}  // namespace oceanbase