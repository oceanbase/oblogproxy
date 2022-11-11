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

#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

namespace oceanbase {
namespace logproxy {

class Timer {
public:
  Timer()
  {
    reset();
    pthread_mutex_init(&_lock, nullptr);
    pthread_cond_init(&_cond, nullptr);
  }

  ~Timer()
  {
    pthread_mutex_destroy(&_lock);
    pthread_cond_destroy(&_cond);
  }

  inline void reset()
  {
    _start_time_us = now();
  }

  inline void reset(uint64_t us)
  {
    _start_time_us = us;
  }

  static inline uint64_t now()
  {
    struct timeval tm;
    gettimeofday(&tm, nullptr);
    return (tm.tv_sec * 1000000 + tm.tv_usec);
  }

  inline int64_t elapsed() const
  {
    return now() - _start_time_us;
  }

  inline int64_t elapsed(uint64_t us) const
  {
    return us - _start_time_us;
  }

  inline int64_t stopwatch()
  {
    reset();
    return elapsed();
  }

  // sleep & interrupt
  void sleep(uint64_t interval_us);

  void sleepto(uint64_t nexttime_us);

  void interrupt();

  inline uint64_t start_time() const
  {
    return _start_time_us;
  }

private:
  uint64_t _start_time_us = 0;
  pthread_mutex_t _lock;
  pthread_cond_t _cond;
  uint64_t _next_schedule_time_us;
};

}  // namespace logproxy
}  // namespace oceanbase
