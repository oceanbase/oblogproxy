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

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "config.h"
#include "log.h"
#include "str.h"
#include "shell_executor.h"
#include "fs_util.h"
#include "file_gc.h"

namespace oceanbase {
namespace logproxy {
static Config& _s_config = Config::instance();

FileGcRoutine::FileGcRoutine(std::string path, std::set<std::string> prefixs)
{
  _path = std::move(path);
  _prefixs = std::move(prefixs);
}

static void _list_oblogreader_path(std::map<std::string, uint32_t>& paths)
{
  std::vector<std::string> lines;
  int ret = exec_cmd("bash ./bin/list_logreader_path.sh " + _s_config.oblogreader_path.val(), lines);
  if (ret != 0) {
    OMS_STREAM_ERROR << "Failed to list logreader path, ret " << ret << ", error: " << (lines.empty() ? "" : lines[0]);
    return;
  }

  for (auto& line : lines) {
    // mod_time, path
    std::vector<std::string> secs;
    split(line, '\t', secs);
    if (secs.size() != 2) {
      OMS_STREAM_WARN << "Invalid oblogreader process line: " << line;
      continue;
    }

    uint32_t mod_time = atoi(secs[0].c_str());
    std::string& path = secs[1];
    paths.emplace(path, mod_time);
  }
}

static void _list_oblogreader_processes(std::map<std::string, std::pair<int, uint32_t>>& process)
{
  std::vector<std::string> lines;
  int ret = exec_cmd("bash ./bin/list_logreader_process.sh " + _s_config.oblogreader_path.val(), lines);
  if (ret != 0) {
    OMS_STREAM_ERROR << "Failed to list oblogreader process, ret " << ret << ", error: " << (lines.empty() ? "" : lines[0]);
    return;
  }

  for (std::string& line : lines) {
    // pid, path, start_time
    std::vector<std::string> secs;
    split(line, '\t', secs);
    if (secs.size() != 3) {
      OMS_STREAM_WARN << "Invalid oblogreader process line: " << line;
      continue;
    }

    int pid = atoi(secs[0].c_str());
    uint32_t start_time = atoi(secs[2].c_str());
    process.emplace(secs[1], std::make_pair(pid, start_time));
  }
}

static void _do_gc_child_path()
{
  time_t now_tm = ::time(nullptr);

  std::map<std::string, std::pair<int, uint32_t>> processes;
  _list_oblogreader_processes(processes);

  std::map<std::string, uint32_t> paths;
  _list_oblogreader_path(paths);
  for (auto& path_entry : paths) {
    const std::string& path = path_entry.first;
    uint32_t mod_time = path_entry.second;

    const auto& child_process = processes.find(path);
    if (child_process != processes.end()) {
      // still alive process, skip
      continue;
    }

    uint32_t max_retain_seconds = _s_config.oblogreader_path_retain_hour.val() * 3600;
    if (now_tm - mod_time >= max_retain_seconds) {
      OMS_STREAM_WARN << "Died oblogreader path: " << path << " exist time exceed max retain seconds: " << max_retain_seconds
               << ", about to delete";
      if (!FsUtil::remove(path)) {
        OMS_STREAM_WARN << "Failed to remove died oblogreader path: " << path;
      }
    }
  }
}

static void _do_gc(const std::string& path, const std::set<std::string>& prefixs)
{
  DIR* dir = ::opendir(path.c_str());
  if (dir == nullptr) {
    OMS_STREAM_WARN << "Failed to path to gc: " << path;
    return;
  }

  // <mode time, <file, file size in KB, is_del>>
  std::map<uint32_t, std::tuple<std::string, uint64_t, bool>> todels;

  time_t nowtime = time(nullptr);

  struct dirent* dent = nullptr;
  while ((dent = readdir(dir)) != nullptr) {
    for (auto& prefix : prefixs) {
      if (dent->d_type != DT_REG) {
        continue;
      }

      if (strncmp(dent->d_name, prefix.c_str(), prefix.size()) != 0) {
        continue;
      }

      std::string filename = path + "/" + dent->d_name;

      struct stat st;
      if (::stat(filename.c_str(), &st) != 0) {
        OMS_STREAM_INFO << "File to gc stat failed: " << filename;
        continue;
      }

      bool isdel = false;
      if (nowtime - st.st_mtime >= (_s_config.log_quota_day.val() * 86400)) {
        isdel = true;
      }

      todels.emplace((uint32_t)st.st_mtime, std::make_tuple(filename, (uint64_t)st.st_size / 1024, isdel));
    }
  }
  closedir(dir);

  // from new to old
  uint64_t size_count = 0;
  for (auto iter = todels.rbegin(); iter != todels.rend(); ++iter) {
    if (std::get<2>(iter->second)) {
      continue;
    }
    size_count += std::get<1>(iter->second);
    if (size_count >= (_s_config.log_quota_size_mb.val() * 1024LL)) {
      std::get<2>(iter->second) = true;
      // rest of files all will be marked to delete as size_count are still accumulating
    }
  }

  for (auto& todel : todels) {
    if (std::get<2>(todel.second)) {
      OMS_STREAM_INFO << "File gc: " << std::get<0>(todel.second);
      int ret = remove(std::get<0>(todel.second).c_str());
      if (ret != 0) {
        OMS_STREAM_WARN << "Failed to remove: " << std::get<0>(todel.second) << ", error: " << strerror(errno);
      }
    }
  }
}

void FileGcRoutine::run()
{
  OMS_STREAM_INFO << "file gc size quota MB: " << _s_config.log_quota_size_mb.val() * 1024;
  OMS_STREAM_INFO << "file gc time quota day: " << _s_config.log_quota_day.val();
  OMS_STREAM_INFO << "file gc path: " << _path;
  OMS_STREAM_INFO << "file gc oblogreader path: " << _s_config.oblogreader_path.val();
  OMS_STREAM_INFO << "file gc oblogreader path retain hour: " << _s_config.oblogreader_path_retain_hour.val();
  for (auto& prefix : _prefixs) {
    OMS_STREAM_INFO << "file gc prefixs: " << prefix;
  }

  while (is_run()) {
    // main process
    _do_gc(_path, _prefixs);

    // oblogreader process
    std::map<std::string, uint32_t> paths;
    _list_oblogreader_path(paths);
    for (auto& path_entry : paths) {
      const std::string& path = path_entry.first;
      _do_gc(path + "/" + _path, _prefixs);
    }

    // oblogreader path
    _do_gc_child_path();

    sleep(_s_config.log_gc_interval_s.val());
  }
}

}  // namespace logproxy
}  // namespace oceanbase
