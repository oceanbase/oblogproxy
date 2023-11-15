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

#include "thread.h"
#include "sys_metric.h"
#include "timer.h"
#include <functional>

namespace oceanbase {
namespace logproxy {
class StatusThread : public Thread {
public:
  explicit StatusThread(SysMetric& metric, ProcessGroupMetric& proc_group_metric)
      : _metric(metric), _proc_group_metric(proc_group_metric)
  {}

  void stop() override;

  void register_gauge(const std::string& key, const std::function<int64_t()>& func);

protected:
  void run() override;

private:
  Timer _timer;

  SysMetric& _metric;
  ProcessGroupMetric& _proc_group_metric;
  std::vector<std::pair<std::string, std::function<int64_t()>>> _gauges;
};
}  // namespace logproxy
}  // namespace oceanbase
