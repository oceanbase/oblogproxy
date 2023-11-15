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

#include <unistd.h>
#include "common/log.h"
#include "common/common.h"
#include "common/config.h"
#include "common/counter.h"
#include "oblogreader/oblogreader.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

ReaderRoutine::ReaderRoutine(ObLogReader& reader, OblogAccess& oblog, BlockingQueue<ILogRecord*>& q)
    : Thread("ReaderRoutine"), _reader(reader), _oblog(oblog), _queue(q)
{}

int ReaderRoutine::init(const OblogConfig& config)
{
  std::map<std::string, std::string> configs;
  config.generate_configs(configs);

  int ret = _clog_meta.init(config);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to init clog check ,ret:" << ret;
    return ret;
  }

  uint64_t clog_min_timestamp_us = 0;
  ret = _clog_meta.fetch_once(clog_min_timestamp_us);

  if (config.start_timestamp_us.val() != 0) {
    return _oblog.init_with_us(configs, config.start_timestamp_us.val());
  } else {
    return _oblog.init(configs, config.start_timestamp.val());
  }
}

void ReaderRoutine::stop()
{
  if (is_run()) {
    Thread::stop();
    _oblog.stop();
    _clog_meta.stop();
    _queue.clear([this](ILogRecord* record) { _oblog.release(record); });
  }
}

void ReaderRoutine::run()
{
  _clog_meta.start();

  if (_oblog.start() != OMS_OK) {
    OMS_ERROR << "Failed to start ReaderRoutine";
    return;
  }

  Counter& counter = Counter::instance();
  Timer stage_tm;

  uint64_t record_us = 0;

  while (is_run()) {
    if (record_us != 0 && !_clog_meta.check(record_us)) {
      OMS_ERROR << "Failed to check clog available of last record with timestamp is us:" << record_us
                << ", exit oblogreader";
      break;
    }

    stage_tm.reset();
    ILogRecord* record = nullptr;
    int ret = _oblog.fetch(record, _s_config.read_timeout_us.val());
    int64_t fetch_us = stage_tm.elapsed();

    if (ret == OB_TIMEOUT && record == nullptr) {
      OMS_INFO << "fetch liboblog timeout, nothing incoming...";
      continue;
    }
    if (ret != OB_SUCCESS || record == nullptr) {
      OMS_WARN << "fetch liboblog " << (ret == OB_SUCCESS ? "nullptr" : "failed") << ", ignore...";
      ::usleep(_s_config.read_fail_interval_us.val());
      continue;
    }
    record_us = record->getTimestamp() * 1000000 + record->getRecordUsec();
    int record_size = record->getRealSize();
    // once put record to queue, never access it
    stage_tm.reset();
    while (!_queue.offer(record, _s_config.read_timeout_us.val())) {
      OMS_WARN << "reader transfer queue full(" << _queue.size(false) << "), retry...";
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
