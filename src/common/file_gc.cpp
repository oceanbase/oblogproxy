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
#include "file_gc.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

FileGcRoutine::FileGcRoutine(const std::string& path, const std::set<std::string>& prefixs)
    : _path(path), _prefixs(prefixs)
{}

void FileGcRoutine::run()
{
  OMS_INFO << "file gc size quota: " << _s_config.log_quota_size_mb.val() * 1024;
  for (auto& prefix : _prefixs) {
    OMS_INFO << "file gc prefixs: " << prefix;
  }

  while (is_run()) {
    DIR* dir = ::opendir(_path.c_str());
    if (dir == nullptr) {
      sleep(_s_config.log_gc_interval_s.val());
      continue;
    }

    // <mode time, <file, file size in KB, is_del>>
    std::map<uint32_t, std::tuple<std::string, uint64_t, bool>> todels;

    time_t nowtime = time(nullptr);

    struct dirent* dent = nullptr;
    while ((dent = readdir(dir)) != nullptr) {
      for (auto& prefix : _prefixs) {
        if (dent->d_type != DT_REG) {
          continue;
        }

        if (strncmp(dent->d_name, prefix.c_str(), prefix.size()) != 0) {
          continue;
        }

        std::string filename = _path + "/" + dent->d_name;

        struct stat st;
        if (::stat(filename.c_str(), &st) != 0) {
          OMS_INFO << "file to gc stat failed: " << filename;
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

    uint64_t size_count = 0;
    for (auto iter = todels.rbegin(); iter != todels.rend(); ++iter) {
      if (std::get<2>(iter->second)) {
        continue;
      }
      size_count += std::get<1>(iter->second);
      if (size_count >= (_s_config.log_quota_size_mb.val() * 1024LL)) {
        std::get<2>(iter->second) = true;
      }
    }

    for (auto& todel : todels) {
      if (std::get<2>(todel.second)) {
        OMS_INFO << "file gc: " << std::get<0>(todel.second);
        int ret = remove(std::get<0>(todel.second).c_str());
        if (ret != 0) {
          OMS_WARN << "Failed to remove: " << std::get<0>(todel.second) << ". error=" << strerror(errno);
        }
      }
    }

    sleep(_s_config.log_gc_interval_s.val());
  }
}

}  // namespace logproxy
}  // namespace oceanbase
