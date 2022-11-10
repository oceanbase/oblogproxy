// Copyright (c) 2021 OceanBase
// OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at:
//           http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

#include <iomanip>
#include "common/log.h"
#include "common/config.h"
#include "common/timer.h"
#include "metric/sys_metric.h"
#include "metric/status_thread.h"

namespace oceanbase {
namespace logproxy {

void StatusThread::stop()
{
  Thread::stop();
}

void StatusThread::register_gauge(const std::string& key, const std::function<int64_t()>& func)
{
  _gauges.emplace_back(std::make_pair(key, func));
}

void StatusThread::run()
{
  if (!init_metric()) {
    OMS_ERROR << "Failed to initiate metric, exit StatusThread";
    return;
  }

  std::stringstream ss;
  while (is_run()) {
    _timer.sleep(Config::instance().metric_interval_s.val() * 1000000);

    ss.str("");
    ss << "COUNTS:";
    for (auto& entry : _gauges) {
      ss << "[" << entry.first << ":" << entry.second() << "]";
    }
    OMS_INFO << ss.str();

    if (!collect_metric(_metric)) {
      OMS_ERROR << "Failed to collect metric";
      continue;
    }
    ss.str("");
    ss << "METRICS:";
    ss << "[MEM:" << _metric.memory_status.mem_used_size_mb << "/" << _metric.memory_status.mem_total_size_mb << "MB,"
       << std::setprecision(4) << _metric.memory_status.mem_used_ratio * 100 << "%]"
       << "[UDISK:" << _metric.disk_status.disk_used_size_mb << "/" << _metric.disk_status.disk_total_size_mb << "MB,"
       << std::setprecision(4) << _metric.disk_status.disk_used_ratio * 100 << "%]"
       << "[CPU:" << _metric.cpu_status.cpu_count << "," << std::setprecision(4)
       << _metric.cpu_status.cpu_used_ratio * 100 << "%]"
       << "[LOAD1,5:" << _metric.load_status.load_1 * 100 << "%," << _metric.load_status.load_5 * 100 << "%]"
       << "[NETIO:" << _metric.network_status.network_rx_bytes / 1024 << "KB/s,"
       << _metric.network_status.network_wx_bytes / 1024 << "KB/s]";
    OMS_INFO << ss.str();
  }
}

}  // namespace logproxy
}  // namespace oceanbase
