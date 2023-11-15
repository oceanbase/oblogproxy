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

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "jsonutil.hpp"
#include "log.h"
namespace oceanbase {
namespace logproxy {
struct MemoryStatus : public Json::Value {
  uint64_t mem_total_size_mb = 0;
  uint64_t mem_used_size_mb = 0;
  float mem_used_ratio = 0;
  bool operator==(const MemoryStatus& rhs) const
  {
    return std::tie(mem_total_size_mb, mem_used_size_mb, mem_used_ratio) ==
           std::tie(rhs.mem_total_size_mb, rhs.mem_used_size_mb, rhs.mem_used_ratio);
  }

  constexpr static auto properties = std::make_tuple(property(&MemoryStatus::mem_total_size_mb, "mem_total_size_mb"),
      property(&MemoryStatus::mem_used_size_mb, "mem_used_size_mb"),
      property(&MemoryStatus::mem_used_ratio, "mem_used_ratio"));
};

struct CpuStatus : public Json::Value {
  uint32_t cpu_count = 0;
  float cpu_used_ratio = 0;
  bool operator==(const CpuStatus& rhs) const
  {
    return std::tie(cpu_count, cpu_used_ratio) == std::tie(rhs.cpu_count, rhs.cpu_used_ratio);
  }

  constexpr static auto properties = std::make_tuple(
      property(&CpuStatus::cpu_count, "cpu_count"), property(&CpuStatus::cpu_used_ratio, "cpu_used_ratio"));
};

struct DiskStatus : public Json::Value {
  // unit MB
  uint64_t disk_total_size_mb = 0;
  // unit MB
  uint64_t disk_used_size_mb = 0;
  float disk_used_ratio = 0;
  uint64_t disk_usage_size_process_mb = 0;
  bool operator==(const DiskStatus& rhs) const
  {
    return std::tie(disk_total_size_mb, disk_used_size_mb, disk_used_ratio, disk_usage_size_process_mb) ==
           std::tie(rhs.disk_total_size_mb, rhs.disk_used_size_mb, rhs.disk_used_ratio, rhs.disk_usage_size_process_mb);
  }

  constexpr static auto properties = std::make_tuple(property(&DiskStatus::disk_total_size_mb, "disk_total_size_mb"),
      property(&DiskStatus::disk_used_size_mb, "disk_used_size_mb"),
      property(&DiskStatus::disk_used_ratio, "disk_used_ratio"),
      property(&DiskStatus::disk_usage_size_process_mb, "disk_usage_size_process_mb"));
};

struct NetworkStatus : public Json::Value {
  uint64_t network_rx_bytes = 0;
  uint64_t network_wx_bytes = 0;

  bool operator==(const NetworkStatus& rhs) const
  {
    return std::tie(network_rx_bytes, network_wx_bytes) == std::tie(rhs.network_rx_bytes, rhs.network_wx_bytes);
  }

  constexpr static auto properties = std::make_tuple(property(&NetworkStatus::network_rx_bytes, "network_rx_bytes"),
      property(&NetworkStatus::network_wx_bytes, "network_wx_bytes"));
};

struct LoadStatus : public Json::Value {
  float load_1 = 0;
  float load_5 = 0;

  bool operator==(const LoadStatus& rhs) const
  {
    return std::tie(load_1, load_5) == std::tie(rhs.load_1, rhs.load_5);
  }

  constexpr static auto properties =
      std::make_tuple(property(&LoadStatus::load_1, "load_1"), property(&LoadStatus::load_5, "load_5"));
};

struct ProcessMetric {
  uint64_t pid = 0;
  std::string client_id;
  MemoryStatus memory_status;
  CpuStatus cpu_status;
  DiskStatus disk_status;
  NetworkStatus network_status;

  bool operator==(const ProcessMetric& rhs) const
  {
    return std::tie(pid, client_id) == std::tie(rhs.pid, rhs.client_id);
  }

  constexpr static auto properties =
      std::make_tuple(property(&ProcessMetric::pid, "pid"), property(&ProcessMetric::client_id, "client_id"));

  std::string serialize()
  {
    Json::Value p_json;
    p_json["pid"] = this->pid;
    p_json["client_id"] = this->client_id;
    p_json["memory_status"] = to_json(this->memory_status);
    p_json["cpu_status"] = to_json(this->cpu_status);
    p_json["disk_status"] = to_json(this->disk_status);
    p_json["network_status"] = to_json(this->network_status);

    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, p_json);
  }

  Json::Value serialize_to_json_value() const
  {
    Json::Value p_json;
    p_json["pid"] = this->pid;
    p_json["client_id"] = this->client_id;
    p_json["memory_status"] = to_json(this->memory_status);
    p_json["cpu_status"] = to_json(this->cpu_status);
    p_json["disk_status"] = to_json(this->disk_status);
    p_json["network_status"] = to_json(this->network_status);
    return p_json;
  }
};

struct ProcessGroupMetric {
  std::string item_names[4] = {"logproxy", "oblogreader", "binlog", "binlog_converter"};
  std::map<std::string, ProcessMetric*> metric_group;
};

struct SysMetric {
  std::string ip;
  uint16_t port;
  LoadStatus load_status;
  MemoryStatus memory_status;
  CpuStatus cpu_status;
  DiskStatus disk_status;
  NetworkStatus network_status;
  ProcessGroupMetric process_group_metric;
};

/*
 * @param runtime_status
 * @return null
 * @description collect runtime metrics information
 * @date 2022/9/20 16:15
 */
bool collect_metric(SysMetric& metric, ProcessGroupMetric& pro_group_metric);

bool collect_process_metric(ProcessGroupMetric& proc_group_metric);

void get_network_stat(NetworkStatus& network_status);

extern logproxy::SysMetric g_metric;
extern ProcessGroupMetric g_proc_metric;
}  // namespace logproxy
}  // namespace oceanbase
