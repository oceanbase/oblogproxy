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

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <queue>
#include <list>
#include <future>
#include "dirent.h"
#include "zlib.h"

#include <spdlog/common.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/os.h>
#include <spdlog/details/circular_q.h>
#include <spdlog/details/synchronous_factory.h>

namespace oceanbase {
namespace logproxy {

template <typename Mutex>
class rotating_file_with_compress_sink final : public spdlog::sinks::base_sink<Mutex> {
public:
  rotating_file_with_compress_sink(spdlog::filename_t base_filename, size_t max_file_size, uint16_t retention,
      size_t compress_buf_size, const spdlog::file_event_handlers& event_handlers = {})
      : _base_filename(std::move(base_filename)),
        _file_helper{event_handlers},
        _max_file_size(max_file_size),
        _retention(retention),
        _compress_buf_size(compress_buf_size),
        _filenames_v(),
        _compress_futures()
  {
    if (max_file_size == 0) {
      spdlog::throw_spdlog_ex("rotating with compress sink constructor: max_size arg cannot be zero.");
    }

    init_filename_v();
    _file_helper.open(_base_filename, false);
    _current_size = 0;
  }

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
    auto new_size = _current_size + formatted.size();

    if (new_size > _max_file_size) {
      rotate();

      _file_helper.open(_base_filename);
      new_size = formatted.size();
    }

    _file_helper.write(formatted);
    _current_size = new_size;
  }

  void flush_() override
  {
    _file_helper.flush();
  }

private:
  void init_filename_v()
  {
    spdlog::filename_t dir_path;
    spdlog::filename_t basename;
    std::tie(dir_path, basename) = split_path(_base_filename);
    spdlog::filename_t basename_prefix;
    spdlog::filename_t ext;
    std::tie(basename_prefix, ext) = spdlog::details::file_helper::split_by_extension(basename);

    DIR* dir = opendir(dir_path.c_str());
    if (nullptr != dir) {
      std::vector<spdlog::filename_t> need_compressed_logs;
      struct dirent* entry;
      while (nullptr != (entry = readdir(dir))) {
        distinguish_log_file_and_deal(dir_path, entry, basename_prefix, ext, need_compressed_logs);
      }
      closedir(dir);

      for (const auto& need_compressed_log : need_compressed_logs) {
        submit_async_compress_task(need_compressed_log);
      }
    }
  }

  void distinguish_log_file_and_deal(const spdlog::filename_t dir_path, const dirent* entry,
      const spdlog::filename_t& basename_prefix, const spdlog::filename_t& basename_suffix,
      std::vector<spdlog::filename_t>& need_compressed_logs)
  {
    const spdlog::filename_t d_name = entry->d_name;
    const spdlog::filename_t d_name_with_path = fullname(dir_path, d_name);
    // check d_name starts with "basename_prefix."
    if ((entry->d_type == DT_REG) && (d_name[0] != '.') && (d_name.find(basename_prefix) == 0)) {
      auto end_pos = d_name.find_last_of("0123456789");
      if (end_pos - basename_prefix.length() == 18)  // satisfy the format of gz log file
      {
        const spdlog::filename_t d_suffix = d_name.substr(end_pos + 1, d_name.length());
        if ((basename_suffix + ".gz.compressing") == d_suffix || is_expired(d_name, basename_prefix, _retention)) {
          spdlog::details::os::remove_if_exists(d_name_with_path);
        } else if ((basename_suffix + ".gz") == d_suffix) {
          _filenames_v.push_back(d_name_with_path);
        } else if (d_suffix == basename_suffix) {
          need_compressed_logs.push_back(d_name_with_path);
        }
      } else if (d_name == (basename_prefix + basename_suffix)) {
        auto now = spdlog::log_clock::now();
        spdlog::filename_t filename_with_timestamp = calc_filename(d_name_with_path, now_tm(now), now_millis(now));
        rename_file(d_name_with_path, filename_with_timestamp);
        need_compressed_logs.push_back(filename_with_timestamp);
      }
    }
  }

  inline spdlog::filename_t fullname(const spdlog::filename_t& dir_path, const spdlog::filename_t& f_name)
  {
    return dir_path + spdlog::details::os::folder_seps_filename + f_name;
  }

  inline std::tuple<spdlog::filename_t, spdlog::filename_t> split_path(const spdlog::filename_t& path)
  {
    auto pos = path.find_last_of(spdlog::details::os::folder_seps_filename);
    if (pos == spdlog::filename_t::npos) {
      return std::make_tuple(".", path);
    }
    if (pos == path.length() - 1) {
      return std::make_tuple(path.substr(0, pos), "");
    }

    return std::make_tuple(path.substr(0, pos), path.substr(pos + 1, path.length() - pos));
  }

  void rotate()
  {
    try_clear_previous_compress_futures_(std::chrono::milliseconds(100));

    spdlog::filename_t origin_name = _file_helper.filename();
    _file_helper.close();

    auto now = spdlog::log_clock::now();
    spdlog::filename_t file_to_compress = calc_filename(origin_name, now_tm(now), now_millis(now));
    rename_file(origin_name, file_to_compress);
    submit_async_compress_task(file_to_compress);
  }

  void submit_async_compress_task(const spdlog::filename_t& file_to_compress)
  {
    std::shared_future<void> compress_future(std::async(std::launch::async,
        compress_and_delete_old,
        file_to_compress,
        _base_filename,
        &_filenames_v,
        _compress_buf_size,
        _retention));
    _compress_futures.push_back(compress_future);
  }

  void try_clear_previous_compress_futures_(const std::chrono::milliseconds& wait_time_ms)
  {
    auto iter = _compress_futures.begin();
    while (iter != _compress_futures.end()) {
      if (iter->wait_for(wait_time_ms) == std::future_status::ready) {
        iter = _compress_futures.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  static inline void compress_and_delete_old(const spdlog::filename_t& file_to_compress,
      const spdlog::filename_t& base_filename, std::vector<spdlog::filename_t>* filenames_v, size_t compress_buf_size,
      uint16_t retention)
  {
    const std::string compressed_filename(spdlog::details::os::filename_to_str(file_to_compress) + ".gz");
    bool is_success = try_compress_file(file_to_compress, compressed_filename, compress_buf_size);
    if (is_success) {
      save_and_delete_old(compressed_filename, base_filename, filenames_v, retention);
    } else {
      std::perror(("Failed to compress file: " + file_to_compress).c_str());
    }
  }

  static inline bool try_compress_file(
      const spdlog::filename_t& file_to_compress, const spdlog::filename_t& compressed_filename, std::size_t buf_size)
  {
    std::string const file_to_compress_str = spdlog::details::os::filename_to_str(file_to_compress);

    std::FILE* in;
    spdlog::details::os::fopen_s(&in, file_to_compress, "rb");
    if (nullptr == in) {
      std::perror(("Error opening file " + file_to_compress_str).c_str());
      return false;
    }

    const std::string compressing_filename(compressed_filename + ".compressing");
    gzFile out = gzopen(compressing_filename.c_str(), "wd");
    if (nullptr == out) {
      std::perror(("Error opening gzip file " + compressing_filename).c_str());
      std::fclose(in);
      return false;
    }

    char* buf = new char[buf_size];
    std::size_t len;
    while (static_cast<int>(len = std::fread(buf, 1, buf_size, in)) > 0) {
      if (static_cast<std::size_t>(gzwrite(out, buf, len)) != len) {
        int err_num = 0;
        const char* err_msg = gzerror(out, &err_num);
        if (Z_ERRNO == err_num) {
          std::perror(("Failed to write to gz file " + compressed_filename).c_str());
        } else {
          std::perror(("Failed to write to gz file " + compressing_filename + ": " + err_msg).c_str());
        }
        release_resource_util(in, out, buf, compressing_filename, true);
        return false;
      }
    }

    if (std::ferror(in) != 0) {
      std::perror(("Failed to read file " + file_to_compress_str).c_str());
      release_resource_util(in, out, buf, compressing_filename, true);
      return false;
    }

    release_resource_util(in, out, buf, compressing_filename, false);
    spdlog::details::os::remove(file_to_compress);

    bool ret = rename_file(compressing_filename, compressed_filename);
    return ret;
  }

  static inline void release_resource_util(
      std::FILE* in, gzFile out, const char* buf, const std::string& out_file_name, bool delete_out_file)
  {
    if (nullptr != in) {
      std::fclose(in);
    }
    if (nullptr != out) {
      gzclose(out);
    }

    delete[] buf;

    if (delete_out_file) {
      spdlog::details::os::remove(out_file_name);
    }
  }

  static inline bool rename_file(const spdlog::filename_t& src_filename, const spdlog::filename_t& target_filename)
  {
    // try to delete the target file in case it already exists.
    (void)spdlog::details::os::remove(target_filename);
    bool const ret = spdlog::details::os::rename(src_filename, target_filename) == 0;
    if (!ret) {
      std::perror(("Failed to rename src file: " + src_filename + "to target file: " + target_filename).c_str());
    }
    return ret;
  }

  static inline void save_and_delete_old(const spdlog::filename_t& compressed_filename,
      const spdlog::filename_t& base_filename, std::vector<spdlog::filename_t>* filenames_v, const uint16_t retention)
  {
    std::lock_guard<Mutex> lock(_filename_v_mutex);
    auto iter = filenames_v->begin();
    while (iter != filenames_v->end()) {
      spdlog::filename_t old_filename = *iter;
      if (is_expired(old_filename, base_filename, retention)) {
        if (!spdlog::details::os::path_exists(old_filename))  // avoid manual gunzip log file
        {
          old_filename = old_filename.substr(0, old_filename.size() - 3);
        }
        spdlog::details::os::remove_if_exists(old_filename);
        filenames_v->erase(iter);
      } else {
        ++iter;
      }
    }
    filenames_v->push_back(compressed_filename);
  }

  static inline bool is_expired(
      const spdlog::filename_t& filename, const spdlog::filename_t& base_filename, const uint16_t retention)
  {
    spdlog::filename_t basename_;
    spdlog::filename_t ext;
    std::tie(basename_, ext) = spdlog::details::file_helper::split_by_extension(base_filename);
    const std::string create_time_sec_str = filename.substr(basename_.length() + 1, 15);
    std::tm create_time{};
    strptime(create_time_sec_str.c_str(), "%Y%m%d.%H%M%S", &create_time);
    const uint32_t t_interval = (spdlog::log_clock::to_time_t(spdlog::log_clock::now()) - mktime(&create_time));
    return t_interval >= retention * 3600;
  }

  static spdlog::filename_t calc_filename(const spdlog::filename_t& filename, tm now_tm, uint32_t millis)
  {
    spdlog::filename_t basename;
    spdlog::filename_t ext;
    std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(filename);
    return spdlog::fmt_lib::format(SPDLOG_FILENAME_T("{}.{:04d}{:02d}{:02d}.{:02d}{:02d}{:02d}{:03d}{}"),
        basename,
        now_tm.tm_year + 1900,
        now_tm.tm_mon + 1,
        now_tm.tm_mday,
        now_tm.tm_hour,
        now_tm.tm_min,
        now_tm.tm_sec,
        millis,
        ext);
  }

  static tm now_tm(spdlog::log_clock::time_point t_pt)
  {
    time_t const t_now = spdlog::log_clock::to_time_t(t_pt);
    return spdlog::details::os::localtime(t_now);
  }

  static uint32_t now_millis(spdlog::log_clock::time_point t_pt)
  {
    auto millis = spdlog::details::fmt_helper::time_fraction<std::chrono::milliseconds>(t_pt);
    return static_cast<uint32_t>(millis.count());
  }

  spdlog::filename_t _base_filename;
  spdlog::details::file_helper _file_helper;
  size_t _max_file_size;
  size_t _current_size;
  uint16_t _retention;
  std::vector<spdlog::filename_t> _filenames_v;
  size_t _compress_buf_size;
  std::list<std::shared_future<void>> _compress_futures;
  static std::mutex _filename_v_mutex;
};

template <typename Mutex>
std::mutex rotating_file_with_compress_sink<Mutex>::_filename_v_mutex;

using rotating_file_with_compress_sink_mt = rotating_file_with_compress_sink<std::mutex>;
using rotating_file_with_compress_sink_st = rotating_file_with_compress_sink<spdlog::details::null_mutex>;
}  // namespace logproxy

// factory functions
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<spdlog::logger> rotating_with_compress_logger_mt(const std::string& logger_name,
    const spdlog::filename_t& filename, size_t max_file_size = 512 * 1024 * 1024, uint16_t retention = 168,
    size_t compress_buf_size = 1 << 19, const spdlog::file_event_handlers& event_handlers = {})
{
  return Factory::template create<logproxy::rotating_file_with_compress_sink_mt>(
      logger_name, filename, max_file_size, retention, compress_buf_size, event_handlers);
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<spdlog::logger> rotating_with_compress_logger_st(const std::string& logger_name,
    const spdlog::filename_t& filename, size_t max_file_size = 512 * 1024 * 1024, size_t retention = 168,
    size_t compress_buf_size = 1 << 19, const spdlog::file_event_handlers& event_handlers = {})
{
  return Factory::template create<logproxy::rotating_file_with_compress_sink_st>(
      logger_name, filename, max_file_size, retention, compress_buf_size, event_handlers);
}

}  // namespace oceanbase