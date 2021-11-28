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

#include <sstream>
#include "log.h"
#include "counter.h"

namespace oceanbase {
namespace logproxy {

void Counter::stop()
{
  Thread::stop();
  _sleep_cv.notify_all();
  join();
}

void Counter::run()
{
  OMS_INFO << "#### Counter thread running, tid: " << tid();

  std::stringstream ss;
  while (is_run()) {
    _timer.reset();
    this->sleep();
    int64_t interval_ms = _timer.elapsed() / 1000;
    int64_t interval_s = interval_ms == 0 ? 0 : (interval_ms / 1000);

    uint64_t rcount = _read_count.load();
    uint64_t wcount = _write_count.load();
    uint64_t rio = _read_io.load();
    uint64_t wio = _write_io.load();
    uint64_t rtps = interval_s == 0 ? rcount : (rcount / interval_s);
    uint64_t wtps = interval_s == 0 ? wcount : (wcount / interval_s);
    uint64_t rios = interval_s == 0 ? rio : (rio / interval_s);
    uint64_t wios = interval_s == 0 ? wio : (wio / interval_s);
    int nowtm = time(nullptr);
    int delay = nowtm - _timestamp;
    int chk_delay = nowtm - _checkpoint;

    // TODO... bytes rate

    ss.str("");
    ss << "Counter:[Span:" << interval_ms << "ms][Delay:" << delay << "," << chk_delay << "][RCNT:" << rcount
       << "][RTPS:" << rtps << "][RIOS:" << rios << "][WCNT:" << wcount << "][WTPS:" << wtps << "][WIOS:" << wios
       << "]";
    for (auto& count : _counts) {
      uint64_t c = count.count.load();
      ss << "[" << count.name << ":" << c << "]";
      count.count.fetch_sub(c);
    }
    for (auto& entry : _gauges) {
      ss << "[" << entry.first << ":" << entry.second() << "]";
    }
    OMS_INFO << ss.str();

    // sub count that logged
    _read_count.fetch_sub(rcount);
    _write_count.fetch_sub(wcount);
    _read_io.fetch_sub(rio);
    _write_io.fetch_sub(wio);
  }

  OMS_INFO << "#### Counter thread stop, tid: " << tid();
}

void Counter::register_gauge(const std::string& key, const std::function<int64_t()>& func)
{
  _gauges.emplace(key, func);
}

void Counter::count_read(int count)
{
  _read_count.fetch_add(count);
}

void Counter::count_write(int count)
{
  _write_count.fetch_add(count);
}

void Counter::count_read_io(int bytes)
{
  _read_io.fetch_add(bytes);
}

void Counter::count_write_io(int bytes)
{
  _write_io.fetch_add(bytes);
}

void Counter::count_key(Counter::CountKey key, uint64_t count)
{
  _counts[key].count.fetch_add(count);
}

void Counter::mark_timestamp(int timestamp)
{
  _timestamp = timestamp;
}

void Counter::mark_checkpoint(uint64_t checkpoint)
{
  _checkpoint = checkpoint;
}

void Counter::sleep()
{
  // condition variable as SLEEP which could be gracefully interrupted
  std::unique_lock<std::mutex> lk(_sleep_cv_lk);
  _sleep_cv.wait_for(lk, std::chrono::seconds(_sleep_interval_s));
}

}  // namespace logproxy
}  // namespace oceanbase
