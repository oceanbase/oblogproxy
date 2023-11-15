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

#include "timer.h"

namespace oceanbase {
namespace logproxy {
void Timer::sleep(uint64_t interval_us)
{
  sleepto(now() + interval_us);
}

void Timer::sleepto(uint64_t nexttime_us)
{
  _next_schedule_time_us = nexttime_us;

  uint64_t current = now();

  while (current < _next_schedule_time_us) {
    timeval tv;
    timespec timeout;
    gettimeofday(&tv, nullptr);

    if (tv.tv_usec < 999500) {
      timeout.tv_sec = tv.tv_sec;
      timeout.tv_nsec = (tv.tv_usec + 500) * 1000;
    } else {
      timeout.tv_sec = tv.tv_sec + 1;
      timeout.tv_nsec = (tv.tv_usec + 500 - 1000000) * 1000;
    }

    pthread_mutex_lock(&_lock);
    pthread_cond_timedwait(&_cond, &_lock, &timeout);
    pthread_mutex_unlock(&_lock);
    current = now();
  }
}

void Timer::interrupt()
{
  _next_schedule_time_us = 0;
  pthread_cond_signal(&_cond);
}

}  // namespace logproxy
}  // namespace oceanbase
