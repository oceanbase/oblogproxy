// Copyright (c) 2021 OceanBase
// OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at:
//           http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

#include <string>

#include "common/shell_executor.h"
#include "common/timer.h"
#include "common/config.h"
#include "common/fs_util.h"
#include "common/str.h"
#include "metric/sys_metric.h"

/**
 * FIXME... MACOS compatitable
 */
namespace oceanbase {
namespace logproxy {

// The maximum value of cgoup memory limit, in KB. Greater than or equal to this value means that docker has no limit on
// memory
static const int64_t MEM_DEFAULT = 9223372036854771712;
static const int64_t UNIT_GB = 1024 * 1024 * 1024L;
static const int64_t UNIT_MB = 1024 * 1024L;

static std::string _netif;
NetworkStatus _last_net_st;

bool init_metric()
{
  /**
   * expect output: default via 10.0.0.123 dev bond0
   */
  std::string result;
  int ret = exec_cmd("ip -o -4 route show to default", result);
  trim(result);
  if (ret != 0 || result.empty()) {
    OMS_ERROR << "Failed to collect network for ip command failures, ret:" << ret;
    return false;
  }
  std::vector<std::string> nets;
  split_by_str(result, " ", nets);
  if (nets.size() < 5 || nets[4].empty()) {
    OMS_ERROR << "Failed to collect network for invalid content:" << result;
    return false;
  }
  _netif.assign(nets[4]);

  return true;
}

static int get_cpu_core_count()
{
  const std::string& filename = "/sys/fs/cgroup/cpuacct/cpuacct.usage_percpu";
  std::string core_str;
  if (!FsUtil::read_file(filename, core_str) || core_str.empty()) {
    OMS_ERROR << "Failed to get cpu core count for failed to read: " << filename;
    return 0;
  }
  std::vector<std::string> parts;
  split_by_str(core_str, " ", parts);
  return parts.size();
}

static int get_limit_cpu_core_count_from_cpuset()
{
  const std::string& filename = "/sys/fs/cgroup/cpuset/cpuset.cpus";
  std::string core_str;
  if (!FsUtil::read_file(filename, core_str)) {
    OMS_ERROR << "Failed to get_limit_cpu_core_count_from_cpuset for failed to read:" << filename;
    return 0;
  }

  std::vector<std::string> parts;
  split_by_str(core_str, ",", parts);

  int core_num = 0;
  for (const std::string& part : parts) {
    std::vector<std::string> core_part;
    split_by_str(part, "-", core_part);
    if (core_part.empty()) {
      return 0;
    }

    if (core_part.size() == 1) {
      core_num++;
      continue;
    }

    if (core_part.size() > 2) {
      return 0;
    }
    core_num += abs(atoi(core_part[0].c_str()) - atoi(core_part[1].c_str())) + 1;
  }
  return core_num;
}

static int64_t get_limit_cpu_core_count()
{
  int64_t quota = 0;
  if (!FsUtil::read_number("/sys/fs/cgroup/cpu/cpu.cfs_quota_us", quota)) {
    OMS_ERROR << "Failed to read cpu.cfs_quota_us";
    return 0;
  }
  if (quota == -1) {
    return get_limit_cpu_core_count_from_cpuset();
  }

  int64_t period = 0;
  bool ret = FsUtil::read_number("/sys/fs/cgroup/cpu/cpu.cfs_period_us", period);
  if (!ret || period <= 0) {
    OMS_ERROR << "Failed to read cfs_period_us";
    return 0;
  }
  return quota / period;
}

/*
 * @description get the CPU time slice used by the operating system in the current state
 * @date 2022/9/19 16:20
 */
static int64_t get_sys_spu_usage()
{
  const std::string& filename = "/proc/stat";
  std::vector<std::string> lines;
  if (!FsUtil::read_lines(filename, lines) || lines.empty()) {
    OMS_ERROR << "Failed to get_sys_spu_usage for failed to read:" << filename;
    return -1;
  }

  // find line of prefix equals "cpu "
  std::string prefix = "cpu ";
  for (const std::string& line : lines) {
    if (line.empty()) {
      continue;
    }
    size_t pos = line.find_first_of(prefix);
    if (pos != 0) {
      continue;
    }

    std::string cpu;
    cpu = line.substr(pos + prefix.size());
    trim(cpu);

    std::vector<std::string> units;
    split_by_str(cpu, " ", units);

    int64_t total = 0;
    for (const std::string& unit : units) {
      if (unit.empty()) {
        continue;
      }
      total += atof(unit.c_str());
    }
    return total;
  }
  return 0;
}

static int64_t get_cpu_tick()
{
  std::string result;
  int ret = exec_cmd("getconf CLK_TCK", result);
  if (ret != 0 || result.empty()) {
    OMS_ERROR << "Failed to get cpu tick for failed to exec cmd: getconf CLK_TCK";
    return 0;
  }
  return atoll(result.c_str());
}

static int64_t get_cpu_tick_nano(int64_t tick)
{
  return 1000.0 * 1000.0 * 1000.0 / (int64_t)tick;
}

bool collect_cpu(CpuStatus& cpu_status)
{
  int cpu_core_count = get_cpu_core_count();
  if (cpu_core_count <= 0) {
    OMS_ERROR << "Failed to collect cpu for failed to get_cpu_core_count";
    return false;
  }
  int64_t limit_cpu_core_count = get_limit_cpu_core_count();
  if (limit_cpu_core_count <= 0) {
    OMS_ERROR << "Failed to collect cpu for failed to get_limit_cpu_core_count";
    return false;
  }

  int64_t cpu_usage_pre = 0;
  bool ret = FsUtil::read_number("/sys/fs/cgroup/cpuacct/cpuacct.usage", cpu_usage_pre);
  if (!ret || cpu_usage_pre <= 0) {
    OMS_ERROR << "Failed to collect cpu for failed to read cpuacct.usage";
    return false;
  }
  int64_t sys_cpu_usage_pre = get_sys_spu_usage();
  if (sys_cpu_usage_pre < 0) {
    OMS_ERROR << "Failed to collect cpu for failed to failed to get_sys_spu_usage";
    return false;
  }

  usleep(500 * 1000);

  int64_t cpu_usage = 0;
  ret = FsUtil::read_number("/sys/fs/cgroup/cpuacct/cpuacct.usage", cpu_usage);
  if (!ret || cpu_usage <= 0) {
    OMS_ERROR << "Failed to collect cpu for failed to read cpuacct.usage again";
    return false;
  }
  int64_t sys_cpu_usage = get_sys_spu_usage();
  if (sys_cpu_usage < 0) {
    OMS_ERROR << "Failed to collect cpu for failed to failed to get_sys_spu_usage again";
    return false;
  }

  int64_t cpu_tick = get_cpu_tick();
  if (cpu_tick <= 0) {
    OMS_ERROR << "Failed to collect cpu for failed to get_cpu_tick";
    return false;
  }
  int64_t sys_cpu_delta = (sys_cpu_usage - sys_cpu_usage_pre) * get_cpu_tick_nano(cpu_tick);
  if (sys_cpu_delta > 1) {
    int64_t cpu_delta = cpu_usage - cpu_usage_pre;
    cpu_status.cpu_used_ratio = ((float)cpu_delta / sys_cpu_delta) * ((float)cpu_core_count / limit_cpu_core_count);
  }
  cpu_status.cpu_count = limit_cpu_core_count;
  return true;
}

bool collect_load(LoadStatus& load_status)
{
  std::string result;
  if (!FsUtil::read_file("/proc/loadavg", result) || result.empty()) {
    OMS_ERROR << "Failed to collect load for failed to read /proc/loadavg";
    return false;
  }
  std::vector<std::string> load_items;
  split_by_str(result, " ", load_items);
  if (load_items.size() >= 3) {
    load_status.load_1 = atof(load_items[0].c_str());
    load_status.load_5 = atof(load_items[1].c_str());
  }
  return true;
}

static int64_t get_mem_total()
{
  std::string result;
  int ret = exec_cmd("cat /proc/meminfo | grep 'MemTotal' | awk '{print $(NF-1)}'", result);
  if (ret != 0 || result.empty()) {
    return MEM_DEFAULT;
  }
  return atoll(result.c_str());
}

// static int64_t get_mem_available()
//{
//   std::string ret = exec("cat /proc/meminfo | grep 'MemAvailable' | awk '{print $(NF-1)}'");
//   if (ret.empty()) {
//     return 0;
//   }
//   return atoll(ret.c_str());
// }

bool collect_mem(MemoryStatus& memory_status)
{
  uint64_t mem_usage_bytes = 0;
  if (!FsUtil::read_number(
          "/sys/fs/cgroup/memory/memory.usage_in_bytes", reinterpret_cast<int64_t&>(mem_usage_bytes))) {
    OMS_ERROR << "Failed to collect memory for failed to read memory.usage_in_bytes";
    return false;
  }

  std::map<std::string, std::string> mem_stat;
  if (!FsUtil::read_kvs("/sys/fs/cgroup/memory/memory.stat", " ", mem_stat)) {
    OMS_ERROR << "Failed to collect memory for failed to read memory.stat";
    return false;
  }
  auto entry = mem_stat.find("total_inactive_file");
  if (entry != mem_stat.end()) {
    int64_t mem_total_inactive_file = atoll(entry->second.c_str());
    mem_usage_bytes = mem_usage_bytes - mem_total_inactive_file;
  }

  int64_t mem_limit = 0;
  if (!FsUtil::read_number("/sys/fs/cgroup/memory/memory.limit_in_bytes", mem_limit)) {
    OMS_ERROR << "Failed to collect memory for failed to read memory.limit_in_bytes";
    return false;
  }
  if (mem_limit >= MEM_DEFAULT) {
    mem_limit = get_mem_total() * 1024;
  }

  memory_status.mem_used_ratio = (float)mem_usage_bytes / mem_limit;
  memory_status.mem_total_size_mb = mem_limit / UNIT_MB;
  memory_status.mem_used_size_mb = mem_usage_bytes / UNIT_MB;
  return true;
}

bool collect_disk(DiskStatus& disk_status, const std::string& path)
{
  std::string cmd = "df -P " + path + " | tail -1 | awk '{ print $3,$4}'";
  std::string result;
  int ret = exec_cmd(cmd, result);
  if (ret != 0 || result.empty()) {
    OMS_ERROR << "Failed to collect disk for failed to exec command: " << cmd;
    return false;
  }

  std::vector<std::string> parts;
  split_by_str(result, " ", parts);
  if (parts.size() < 2) {
    OMS_ERROR << "Failed to collect disk metric for invalid content: " << result;
    return false;
  }

  disk_status.disk_used_size_mb = atoll(parts[0].c_str()) / 1024;
  disk_status.disk_total_size_mb = disk_status.disk_used_size_mb + (atoll(parts[1].c_str()) / 1024);
  if (disk_status.disk_total_size_mb != 0) {
    disk_status.disk_used_ratio = (float)disk_status.disk_used_size_mb / disk_status.disk_total_size_mb;
  }
  return true;
}

static bool get_network_stat(const std::string& eth_interface, NetworkStatus& network_status)
{
  int64_t accept = 0;
  if (!FsUtil::read_number("/sys/class/net/" + eth_interface + "/statistics/rx_bytes", accept)) {
    OMS_ERROR << "Failed to collect network for failed to read rx_bytes";
    return false;
  }

  int64_t send = 0;
  if (!FsUtil::read_number("/sys/class/net/" + eth_interface + "/statistics/tx_bytes", send)) {
    OMS_ERROR << "Failed to collect network for failed to read tx_bytes";
    return false;
  }

  network_status.network_rx_bytes = accept;
  network_status.network_wx_bytes = send;
  return true;
}

bool collect_network(NetworkStatus& network_status)
{
  NetworkStatus net_st;
  if (_last_net_st.network_rx_bytes == 0 || _last_net_st.network_wx_bytes == 0) {
    // first round, reset
    if (!get_network_stat(_netif, net_st)) {
      _last_net_st.network_rx_bytes = 0;
      _last_net_st.network_wx_bytes = 0;
      return false;
    }
    _last_net_st.network_rx_bytes = net_st.network_rx_bytes;
    _last_net_st.network_wx_bytes = net_st.network_wx_bytes;
    network_status.network_rx_bytes = 0;
    network_status.network_wx_bytes = 0;
    return true;
  }

  if (!get_network_stat(_netif, net_st)) {
    _last_net_st.network_rx_bytes = 0;
    _last_net_st.network_wx_bytes = 0;
    return false;
  }
  network_status.network_rx_bytes =
      (net_st.network_rx_bytes - _last_net_st.network_rx_bytes) / Config::instance().metric_interval_s.val();
  network_status.network_wx_bytes =
      (net_st.network_wx_bytes - _last_net_st.network_wx_bytes) / Config::instance().metric_interval_s.val();
  _last_net_st.network_rx_bytes = net_st.network_rx_bytes;
  _last_net_st.network_wx_bytes = net_st.network_wx_bytes;
  return true;
}

bool collect_metric(SysMetric& metric)
{
  SysMetric temp;

  if (!collect_cpu(temp.cpu_status)) {
    return false;
  }

  if (!collect_load(temp.load_status)) {
    return false;
  }

  if (!collect_mem(temp.memory_status)) {
    return false;
  }

  if (!collect_disk(temp.disk_status, Config::instance().oblogreader_path.val())) {
    return false;
  }

  if (!collect_network(temp.network_status)) {
    return false;
  }

  metric = temp;
  return true;
}

}  // namespace logproxy
}  // namespace oceanbase