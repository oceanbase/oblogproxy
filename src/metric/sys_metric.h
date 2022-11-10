// Copyright (c) 2021 OceanBase
// OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at:
//           http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

#pragma once

#include <stdint.h>
#include <string>

namespace oceanbase {
namespace logproxy {

struct MemoryStatus {
  uint64_t mem_total_size_mb = 0;
  uint64_t mem_used_size_mb = 0;
  float mem_used_ratio = 0;
};

struct CpuStatus {
  uint32_t cpu_count = 0;
  float cpu_used_ratio = 0;
};

struct DiskStatus {
  // unit MB
  uint64_t disk_total_size_mb = 0;
  // unit MB
  uint64_t disk_used_size_mb = 0;
  float disk_used_ratio = 0;
};

struct NetworkStatus {
  uint64_t network_rx_bytes = 0;
  uint64_t network_wx_bytes = 0;
};

struct LoadStatus {
  float load_1 = 0;
  float load_5 = 0;
};

struct SysMetric {
  std::string ip;
  uint16_t port;
  LoadStatus load_status;
  MemoryStatus memory_status;
  CpuStatus cpu_status;
  DiskStatus disk_status;
  NetworkStatus network_status;
};

bool init_metric();

/*
 * @param runtime_status
 * @return null
 * @description collect runtime metrics information
 * @date 2022/9/20 16:15
 */
bool collect_metric(SysMetric& metric);

}  // namespace logproxy
}  // namespace oceanbase
