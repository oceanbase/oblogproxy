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

#include "log.h"
#include "common.h"
#include "config.h"
#include "counter.h"
#include "trace_log.h"
#include "oblogreader/oblogreader.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

ReaderRoutine::ReaderRoutine(ObLogReader& reader, BlockingQueue<ILogRecord*>& q)
    : Thread("ReaderRoutine"), _reader(reader), _obcdc(nullptr), _queue(q)
{}

int ReaderRoutine::init(const OblogConfig& config, IObCdcAccess* obcdc)
{
  _obcdc = obcdc;
  std::map<std::string, std::string> configs;
  config.generate_configs(configs);

  int ret = _clog_meta.init(config);
  if (ret != OMS_OK) {
    OMS_ERROR("Failed to init clog check ,ret: {}", ret);
    return ret;
  }

  uint64_t clog_min_timestamp_us = 0;
  ret = _clog_meta.fetch_once(clog_min_timestamp_us);
  if (config.start_timestamp_us.val() != 0) {
    return _obcdc->init_with_us(configs, config.start_timestamp_us.val());
  } else {
    return _obcdc->init(configs, config.start_timestamp.val());
  }
}

void ReaderRoutine::stop()
{
  if (is_run()) {
    Thread::stop();
    _queue.clear([this](ILogRecord* record) { _obcdc->release(record); });
    _obcdc->stop();
    _clog_meta.stop();
  }
}

void ReaderRoutine::run()
{
  _clog_meta.start();

  if (_obcdc->start() != OMS_OK) {
    OMS_ERROR("Failed to start ReaderRoutine");
    return;
  }

  Counter& counter = Counter::instance();
  Timer stage_tm;

  uint64_t record_us = 0;
  while (is_run()) {
    if (record_us != 0 && !_clog_meta.check(record_us)) {
      OMS_ERROR("Failed to check clog available of last record with timestamp is us: {}, exit oblogreader", record_us);
      break;
    }

    stage_tm.reset();
    ILogRecord* record = nullptr;
    int ret = _obcdc->fetch(record, _s_config.read_timeout_us.val());
    int64_t fetch_us = stage_tm.elapsed();

    if (ret == OB_TIMEOUT && record == nullptr) {
      OMS_INFO("Fetch liboblog timeout, nothing incoming...");
      continue;
    }

    if (ret != OB_SUCCESS) {
      OMS_ERROR("Failed to get data from liboblog, the error code is: {}", ret);
      break;
    }
    record_us = record->getTimestamp() * 1000000 + record->getRecordUsec();
    int record_size = record->getRealSize();
    // once put record to queue, never access it
    stage_tm.reset();
    if (_s_config.verbose_record_read.val()) {
      TraceLog::info(record);
    }

    while (!_queue.offer(record, _s_config.read_timeout_us.val())) {
      OMS_WARN("reader transfer queue full({}), retry...", _queue.size(false));
    }
    int64_t offer_us = stage_tm.elapsed();

    counter.count_key(Counter::READER_FETCH_US, fetch_us);
    counter.count_key(Counter::READER_OFFER_US, offer_us);
    counter.count_read_io(record_size);
    counter.count_read(1);
  }

  _reader.stop();
}

}  // namespace logproxy
}  // namespace oceanbase
