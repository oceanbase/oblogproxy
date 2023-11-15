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

#include <string>
#include <cassert>
#include <csignal>
#include "shell_executor.h"
#include "timer.h"
#include "config.h"
#include "fs_util.h"
#include "str.h"
#include "sys_metric.h"
#include "guard.hpp"
/**
 * FIXME... MACOS compatitable
 */
namespace oceanbase {
namespace logproxy {
logproxy::SysMetric g_metric;
ProcessGroupMetric g_proc_metric;
#define CPU_START_POS 14
#define READ_BUF_SIZE 512
#define DATA_DIR "/data/"

// The maximum value of cgoup memory limit, in KB. Greater than or equal to this value means that docker has no limit on
// memory
static const int64_t MEM_DEFAULT = 9223372036854771712;
static const int64_t UNIT_GB = 1024 * 1024 * 1024L;
static const int64_t UNIT_MB = 1024 * 1024L;
static const int64_t UNIT_KB = 1024L;

NetworkStatus _last_net_st;

static int get_cpu_core_count()
{
  const std::string& filename = "/sys/fs/cgroup/cpuacct/cpuacct.usage_percpu";
  std::string core_str;
  if (!FsUtil::read_file(filename, core_str) || core_str.empty()) {
    OMS_STREAM_ERROR << "Failed to get cpu core count for failed to read: " << filename;
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
    OMS_STREAM_ERROR << "Failed to get_limit_cpu_core_count_from_cpuset for failed to read:" << filename;
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
    OMS_STREAM_ERROR << "Failed to read cpu.cfs_quota_us";
    return 0;
  }
  if (quota == -1) {
    return get_limit_cpu_core_count_from_cpuset();
  }

  int64_t period = 0;
  bool ret = FsUtil::read_number("/sys/fs/cgroup/cpu/cpu.cfs_period_us", period);
  if (!ret || period <= 0) {
    OMS_STREAM_ERROR << "Failed to read cfs_period_us";
    return 0;
  }
  return quota / period;
}

/*
 * @description get the CPU time slice used by the operating system in the current state
 * @date 2022/9/19 16:20
 */
static int64_t get_sys_cpu_usage()
{
  const std::string& filename = "/proc/stat";
  std::vector<std::string> lines;
  if (!FsUtil::read_lines(filename, lines) || lines.empty()) {
    OMS_STREAM_ERROR << "Failed to get_sys_cpu_usage for failed to read:" << filename;
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
    OMS_STREAM_ERROR << "Failed to get cpu tick for failed to exec cmd: getconf CLK_TCK";
    return 0;
  }
  return atoll(result.c_str());
}

static int64_t get_cpu_tick_nano(int64_t tick)
{
  return 1000 * 1000 * 1000 / tick;
}

bool collect_cpu(CpuStatus& cpu_status)
{
  int cpu_core_count = get_cpu_core_count();
  if (cpu_core_count <= 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to get_cpu_core_count";
    return false;
  }
  int64_t limit_cpu_core_count = get_limit_cpu_core_count();
  if (limit_cpu_core_count <= 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to get_limit_cpu_core_count";
    return false;
  }

  int64_t cpu_usage_pre = 0;
  bool ret = FsUtil::read_number("/sys/fs/cgroup/cpuacct/cpuacct.usage", cpu_usage_pre);
  if (!ret || cpu_usage_pre <= 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to read cpuacct.usage";
    return false;
  }
  int64_t sys_cpu_usage_pre = get_sys_cpu_usage();
  if (sys_cpu_usage_pre < 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to failed to get_sys_cpu_usage";
    return false;
  }

  usleep(500 * 1000);

  int64_t cpu_usage = 0;
  ret = FsUtil::read_number("/sys/fs/cgroup/cpuacct/cpuacct.usage", cpu_usage);
  if (!ret || cpu_usage <= 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to read cpuacct.usage again";
    return false;
  }
  int64_t sys_cpu_usage = get_sys_cpu_usage();
  if (sys_cpu_usage < 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to failed to get_sys_cpu_usage again";
    return false;
  }

  int64_t cpu_tick = get_cpu_tick();
  if (cpu_tick <= 0) {
    OMS_STREAM_ERROR << "Failed to collect cpu for failed to get_cpu_tick";
    return false;
  }
  int64_t sys_cpu_delta = (sys_cpu_usage - sys_cpu_usage_pre) * get_cpu_tick_nano(cpu_tick);
  if (sys_cpu_delta > 1) {
    int64_t cpu_delta = cpu_usage - cpu_usage_pre;
    cpu_status.cpu_used_ratio =
        ((float)cpu_delta / (float)sys_cpu_delta) * ((float)cpu_core_count / limit_cpu_core_count);
  }
  cpu_status.cpu_count = limit_cpu_core_count;
  return true;
}

bool collect_load(LoadStatus& load_status)
{
  std::string result;
  if (!FsUtil::read_file("/proc/loadavg", result) || result.empty()) {
    OMS_STREAM_ERROR << "Failed to collect load for failed to read /proc/loadavg";
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

bool collect_mem(MemoryStatus& memory_status)
{
  uint64_t mem_usage_bytes = 0;
  if (!FsUtil::read_number(
          "/sys/fs/cgroup/memory/memory.usage_in_bytes", reinterpret_cast<int64_t&>(mem_usage_bytes))) {
    OMS_STREAM_ERROR << "Failed to collect memory for failed to read memory.usage_in_bytes";
    return false;
  }

  std::map<std::string, std::string> mem_stat;
  if (!FsUtil::read_kvs("/sys/fs/cgroup/memory/memory.stat", " ", mem_stat)) {
    OMS_STREAM_ERROR << "Failed to collect memory for failed to read memory.stat";
    return false;
  }
  auto entry = mem_stat.find("total_inactive_file");
  if (entry != mem_stat.end()) {
    int64_t mem_total_inactive_file = atoll(entry->second.c_str());
    mem_usage_bytes = mem_usage_bytes - mem_total_inactive_file;
  }

  int64_t mem_limit = 0;
  if (!FsUtil::read_number("/sys/fs/cgroup/memory/memory.limit_in_bytes", mem_limit)) {
    OMS_STREAM_ERROR << "Failed to collect memory for failed to read memory.limit_in_bytes";
    return false;
  }
  if (mem_limit >= MEM_DEFAULT) {
    mem_limit = get_mem_total() * 1024;
  }

  memory_status.mem_used_ratio = (float)mem_usage_bytes / (float)mem_limit;
  memory_status.mem_total_size_mb = mem_limit / UNIT_MB;
  memory_status.mem_used_size_mb = mem_usage_bytes / UNIT_MB;
  return true;
}

bool collect_disk(DiskStatus& disk_status, const std::string& path)
{
  uint64_t folder_size_bytes = FsUtil::folder_size(path);
  disk_status.disk_usage_size_process_mb = folder_size_bytes / UNIT_MB;
  FsUtil::disk_info disk_info = FsUtil::space(path);

  disk_status.disk_total_size_mb = disk_info.capacity / UNIT_MB;
  disk_status.disk_used_size_mb = (disk_info.capacity - disk_info.available) / UNIT_MB;
  if (disk_status.disk_total_size_mb != 0) {
    disk_status.disk_used_ratio = (float)disk_status.disk_used_size_mb / disk_status.disk_total_size_mb;
  }
  return true;
}

void get_total_recv_transmit_bytes(std::ifstream& file, uint64_t& recv_bytes, uint64_t& transmit_bytes)
{
  std::string line;
  getline(file, line);
  getline(file, line);

  uint64_t total_recv_bytes = 0;
  uint64_t total_transmit_bytes = 0;
  while (getline(file, line)) {
    std::stringstream ss(line);
    std::string iface, recv_bytes_str, recv_packets, recv_errs, recv_drop, recv_fifo, recv_frame, recv_compressed,
        recv_multicast;
    std::string transmit_bytes_str, transmit_packets, transmit_errs, transmit_drop, transmit_fifo, transmit_colls,
        transmit_carrier, transmit_compressed;

    ss >> iface >> recv_bytes_str >> recv_packets >> recv_errs >> recv_drop >> recv_fifo >> recv_frame >>
        recv_compressed >> recv_multicast >> transmit_bytes_str >> transmit_packets >> transmit_errs >> transmit_drop >>
        transmit_fifo >> transmit_colls >> transmit_carrier >> transmit_compressed;

    total_recv_bytes += atoll(recv_bytes_str.c_str());
    total_transmit_bytes += atoll(transmit_bytes_str.c_str());
  }
  recv_bytes = total_recv_bytes;
  transmit_bytes = total_transmit_bytes;
}

void get_network_stat(NetworkStatus& network_status)
{
  std::ifstream file("/proc/net/dev");
  if (file.bad()) {
    OMS_STREAM_ERROR << "Failed to open file:"
                     << "/proc/net/dev";
    return;
  }
  uint64_t recv_bytes = 0;
  uint64_t transmit_bytes = 0;
  get_total_recv_transmit_bytes(file, recv_bytes, transmit_bytes);
  network_status.network_rx_bytes = recv_bytes;
  network_status.network_wx_bytes = transmit_bytes;
}

void collect_network(NetworkStatus& network_status)
{
  NetworkStatus net_st;
  if (_last_net_st.network_rx_bytes == 0 && _last_net_st.network_wx_bytes == 0) {
    // first round, reset
    get_network_stat(net_st);
    _last_net_st.network_rx_bytes = net_st.network_rx_bytes;
    _last_net_st.network_wx_bytes = net_st.network_wx_bytes;
    network_status.network_rx_bytes = 0;
    network_status.network_wx_bytes = 0;
  }

  get_network_stat(net_st);
  network_status.network_rx_bytes =
      (net_st.network_rx_bytes - _last_net_st.network_rx_bytes) / Config::instance().metric_interval_s.val();
  network_status.network_wx_bytes =
      (net_st.network_wx_bytes - _last_net_st.network_wx_bytes) / Config::instance().metric_interval_s.val();
  _last_net_st.network_rx_bytes = net_st.network_rx_bytes;
  _last_net_st.network_wx_bytes = net_st.network_wx_bytes;
}

int64_t get_pro_cpu_time(unsigned int pid)
{
  char filename[1024];
  std::memset(filename, 0, sizeof(filename));
  sprintf(filename, "/proc/%d/stat", pid);
  std::string content;
  if (!FsUtil::read_file(filename, content)) {
    OMS_STREAM_ERROR << "Failed to read stat:" << filename;
    return 0;
  }
  std::vector<std::string> cpu_stat;
  split(content, ' ', cpu_stat);

  if (cpu_stat.size() < CPU_START_POS + 4) {
    return 0;
  }
  return atol(cpu_stat.at(CPU_START_POS).c_str()) + atol(cpu_stat.at(CPU_START_POS + 1).c_str()) +
         atol(cpu_stat.at(CPU_START_POS + 2).c_str()) + atol(cpu_stat.at(CPU_START_POS + 3).c_str());
}

void get_cpu_stat(unsigned int pid, CpuStatus& cpu_status)
{
  long s_cur_pro_cpu, s_pre_pro_cpu;
  long s_cur_sys_cpu, s_pre_sys_cpu;
  float ratio = 0.0;
  s_pre_pro_cpu = get_pro_cpu_time(pid);
  s_pre_sys_cpu = get_sys_cpu_usage();

  sleep(1);
  s_cur_pro_cpu = get_pro_cpu_time(pid);
  s_cur_sys_cpu = get_sys_cpu_usage();

  if ((s_cur_pro_cpu == s_pre_pro_cpu) || (s_cur_sys_cpu == s_pre_sys_cpu) || (s_cur_pro_cpu == 0) ||
      (s_cur_sys_cpu == 0)) {
    ratio = 0;
  } else {
    ratio = (100.0f * float(s_cur_pro_cpu - s_pre_pro_cpu)) / float(s_cur_sys_cpu - s_pre_sys_cpu);
  }

  s_pre_pro_cpu = s_cur_pro_cpu;
  s_pre_sys_cpu = s_cur_sys_cpu;

  cpu_status.cpu_used_ratio = ratio;
  cpu_status.cpu_count = get_limit_cpu_core_count();
}

void collect_by_pid_name(const std::string& pid_name, ProcessGroupMetric& proc_group_metric)
{
  DIR* proc_dir;
  struct dirent* ent;
  char filename[READ_BUF_SIZE];

  proc_dir = opendir("/proc");
  assert(proc_dir != nullptr);

  while ((ent = readdir(proc_dir)) != nullptr) {
    if (strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    if (!isdigit(*ent->d_name)) {
      continue;
    }

    sprintf(filename, "/proc/%s/status", ent->d_name);

    std::map<std::string, std::string> status;
    if (!FsUtil::read_kvs(filename, "\t", status)) {
      continue;
    }

    if (status.find("Name:") != status.end()) {
      std::string proc_name = status.find("Name:")->second;
      if (std::equal(proc_name.begin(), proc_name.end(), pid_name.c_str())) {
        int pid = atoi(ent->d_name);
        // Path to build symbolic links
        char link_path[PATH_MAX];
        std::memset(link_path, 0, sizeof(link_path));
        std::snprintf(link_path, sizeof(link_path), "/proc/%d/cwd", pid);
        // collect disk
        // Read the target path pointed to by the symbolic link
        char cwd[PATH_MAX];
        std::memset(cwd, 0, sizeof(cwd));
        if (readlink(link_path, cwd, sizeof(cwd)) == -1) {
          continue;
        }
        ProcessMetric* process_metric = nullptr;
        std::string path = std::string(cwd);
        if (proc_group_metric.metric_group.find(path) != proc_group_metric.metric_group.end()) {
          process_metric = proc_group_metric.metric_group.find(path)->second;
        } else {
          process_metric = new ProcessMetric();
          process_metric->pid = pid;
        }
        // collect mem
        if (status.find("VmRSS:") != status.end()) {
          std::string vmrss_str = status.find("VmRSS:")->second;
          logproxy::trim(vmrss_str);
          std::vector<std::string> vmrss_parts;
          logproxy::split(vmrss_str, ' ', vmrss_parts);
          if (!vmrss_parts.empty()) {
            uint64_t vmrss = std::stoul(vmrss_parts.at(0));
            process_metric->memory_status.mem_used_size_mb = vmrss / 1024L;
            process_metric->memory_status.mem_total_size_mb = get_mem_total() / 1024L;
            process_metric->memory_status.mem_used_ratio =
                process_metric->memory_status.mem_used_size_mb / process_metric->memory_status.mem_total_size_mb;
          }
        }
        // collect cpu
        get_cpu_stat(pid, process_metric->cpu_status);
        // collect disk
        /*!
         * @brief Binlog mode collects resource indicators of non-main processes
         */
        if (Config::instance().binlog_mode.val() && !std::equal(proc_name.begin(), proc_name.end(), "logproxy")) {
          collect_disk(process_metric->disk_status, path + DATA_DIR);
        } else {
          collect_disk(process_metric->disk_status, path);
        }
        process_metric->client_id = path;
        proc_group_metric.metric_group.emplace(path, process_metric);
      }
    }
  }

  closedir(proc_dir);
}

bool collect_metric(SysMetric& metric, ProcessGroupMetric& pro_group_metric)
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

  collect_disk(temp.disk_status, Config::instance().oblogreader_path.val());

  collect_network(temp.network_status);

  metric = temp;

  if (!collect_process_metric(pro_group_metric)) {
    return false;
  }

  return true;
}

/*
 * @params
 * @returns Whether the collection is successful
 * @description Collect all resource metric whose process names are logproxy, logreader and binlog
 * @date 2022/12/4 20:40
 */
bool collect_process_metric(ProcessGroupMetric& proc_group_metric)
{
  // Clean up dead process indicators
  for (auto iter = proc_group_metric.metric_group.begin(); iter != proc_group_metric.metric_group.end(); ++iter) {
    if (iter->second != nullptr && 0 != kill(iter->second->pid, 0)) {
      OMS_INFO("The current process {} is no longer alive,pid:{},remove the process from indicator collection",
          iter->second->client_id,
          iter->second->pid);
      delete iter->second;
      iter->second = nullptr;
      proc_group_metric.metric_group.erase(iter);
    }
  }

  for (const std::string& proc_name : proc_group_metric.item_names) {
    collect_by_pid_name(proc_name, proc_group_metric);
  }
  return true;
}

}  // namespace logproxy
}  // namespace oceanbase