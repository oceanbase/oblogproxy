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

    int64_t interval = _timer.elapsed() / 1000;
    uint64_t rcount = _read_count.fetch_and(0);
    uint64_t wcount = _write_count.fetch_and(0);
    uint64_t rio = _read_io.fetch_and(0);
    uint64_t wio = _write_io.fetch_and(0);
    uint64_t rtps = rcount / (interval / 1000);
    uint64_t wtps = wcount / (interval / 1000);
    uint64_t rios = rio / (interval / 1000);
    uint64_t wios = wio / (interval / 1000);
    int delay = (int)time(nullptr) - _timestamp;
    int chk_delay = time(nullptr) - _checkpoint;

    // TODO... bytes rate

    ss.str("");
    ss << "Counter:[Interval:" << interval << "ms][Delay:" << delay << "," << chk_delay << "][Read:" << rcount
       << "][RTPS:" << rtps << "][RIOS:" << rios << "][Write:" << wcount << "][WTPS:" << wtps << "][WIOS:" << wios
       << "]";
    for (auto& entry : _gauges) {
      ss << "[" << entry.first << ":" << entry.second() << "]";
    }
    OMS_INFO << ss.str();
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
