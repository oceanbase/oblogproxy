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
#include "common.h"
#include "thread.h"
#include "timer.h"
#include "config.h"

namespace oceanbase {
namespace logproxy {
class Counter : public Thread {
  OMS_SINGLETON(Counter);
  OMS_AVOID_COPY(Counter);

public:
  void stop() override;

  void run() override;

  void register_gauge(const std::string& key, const std::function<int64_t()>& func);

  void count_read(uint64_t count = 1);

  void count_write(uint64_t count = 1);

  void count_read_io(uint64_t bytes);

  void count_write_io(uint64_t bytes);

  void count_xwrite_io(uint64_t bytes);

  void count_xwrite_io(int bytes);

  // MUST BE as same order as _counts
  enum CountKey {
    READER_FETCH_US = 0,
    READER_OFFER_US = 1,
    SENDER_POLL_US = 2,
    SENDER_ENCODE_US = 3,
    SENDER_SEND_US = 4,
    BINLOG_DELAY_US = 5,
  };

  void count_key(CountKey key, uint64_t count);

  void mark_timestamp(uint64_t timestamp_us);

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
  volatile uint64_t _timestamp_us = Timer::now();
  volatile uint64_t _checkpoint_us = _timestamp_us;
  volatile uint64_t _count_timestamp_us = _timestamp_us;

  CountItem _counts[6]{{"RFETCH"}, {"ROFFER"}, {"SPOLL"}, {"SENCODE"}, {"SSEND"}, {"BINLOG_DELAY_US"}};

  std::map<std::string, std::function<int64_t()>> _gauges;

  std::mutex _sleep_cv_lk;
  std::condition_variable _sleep_cv;

  uint64_t _sleep_interval_s = Config::instance().counter_interval_s.val();
};

/*!
 * @brief Statistics, mainly used to register statistical information, and regularly output to the log
 */
class CounterStatistics : public Thread {
public:
  void stop() override;

  void run() override;

  /*!
   * @brief
   * @param key Statistically Unique Identifier
   * @param func statistical function
   */
  void register_gauge(const std::string& key, const std::function<uint64_t()>& func);

  /*!
   * @brief
   * @param key Statistically Unique Identifier
   */
  void unregister_gauge(const std::string& key);

private:
  void sleep();

private:
  Timer _timer;

  /*!
   * @brief Triple, metric unique identifier - metric name array - data corresponding to the metric name
   */
  std::map<std::string, std::function<uint64_t()>> _gauges;

  std::mutex _sleep_cv_lk;
  std::condition_variable _sleep_cv;

  uint64_t _sleep_interval_s = Config::instance().counter_interval_s.val();
};

}  // namespace logproxy
}  // namespace oceanbase
