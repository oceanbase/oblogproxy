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
#include "oblogreader/reader_routine.h"

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
  return _oblog.init(configs, config.start_timestamp.val());
}

void ReaderRoutine::stop()
{
  if (is_run()) {
    Thread::stop();
    _oblog.stop();
    _queue.clear([this](ILogRecord* record) { _oblog.release(record); });
  }
}

void ReaderRoutine::run()
{
  if (_oblog.start() != OMS_OK) {
    return;
  }

  Counter& counter = Counter::instance();
  Timer stage_tm;

  while (is_run()) {
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

    stage_tm.reset();
    while (!_queue.offer(record, _s_config.read_timeout_us.val())) {
      OMS_WARN << "reader transfer queue full(" << _queue.size(false) << "), retry...";
    }
    int64_t offer_us = stage_tm.elapsed();

    counter.count_key(Counter::READER_FETCH_US, fetch_us);
    counter.count_key(Counter::READER_OFFER_US, offer_us);
    counter.count_read_io(record->getRealSize());
    counter.count_read(1);
  }

  _reader.stop();
}

}  // namespace logproxy
}  // namespace oceanbase
