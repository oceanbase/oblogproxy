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

#include <time.h>
#include <unistd.h>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <map>
#include <functional>
#include "common/common.h"
#include "common/thread.h"
#include "common/timer.h"
#include "common/config.h"

namespace oceanbase {
namespace logproxy {

class Counter : public Thread {
  OMS_SINGLETON(Counter);
  OMS_AVOID_COPY(Counter);

public:
  void stop() override;

  void run() override;

  void register_gauge(const std::string& key, const std::function<int64_t()>& func);

  void count_read(int count = 1);

  void count_write(int count = 1);

  void count_read_io(int bytes);

  void count_write_io(int bytes);

  void count_xwrite_io(int bytes);

  // MUST BE as same order as _counts
  enum CountKey {
    READER_FETCH_US = 0,
    READER_OFFER_US = 1,
    SENDER_POLL_US = 2,
    SENDER_ENCODE_US = 3,
    SENDER_SEND_US = 4,
  };

  void count_key(CountKey key, uint64_t count);

  void mark_timestamp(int timestamp);

  void mark_checkpoint(uint64_t checkpoint);

private:
  void sleep();

private:
  struct CountItem {
    const char* name;
    std::atomic<uint64_t> count{0};

    CountItem(const char* n) : name(n)
    {}
  };

  Timer _timer;

  std::atomic<uint64_t> _read_count{0};
  std::atomic<uint64_t> _write_count{0};
  std::atomic<uint64_t> _read_io{0};
  std::atomic<uint64_t> _write_io{0};
  std::atomic<uint64_t> _xwrite_io{0};
  volatile int _timestamp = time(nullptr);
  volatile int _checkpoint = time(nullptr);

  CountItem _counts[5]{{"RFETCH"}, {"ROFFER"}, {"SPOLL"}, {"SENCODE"}, {"SSEND"}};

  std::map<std::string, std::function<int64_t()>> _gauges;

  std::mutex _sleep_cv_lk;
  std::condition_variable _sleep_cv;

  uint64_t _sleep_interval_s = Config::instance().counter_interval_s.val();
};

}  // namespace logproxy
}  // namespace oceanbase
