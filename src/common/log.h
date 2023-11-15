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

#include <sys/time.h>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <set>
#include <unordered_set>
#include <list>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "rotating_file_with_compress_sink.hpp"
#include "config.h"
#include "common.h"

namespace oceanbase {
namespace logproxy {

class LogStream {
public:
  explicit LogStream(int level, const std::string& file, const int line, std::ostream* os = &std::cout)
      : _level(level), _file(file), _line(line), _os(os)
  {
    char tmbuf[2 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 6 + 1 + 1 + 5 + 1 + 1];

    if (_level == 0) {
#ifndef NDEBUG
      prefix(tmbuf, sizeof(tmbuf));
      _pre_str = "D" + std::string(tmbuf) + ")[" + file + ":" + std::to_string(line) + "] ";
#endif
      return;
    }

    prefix(tmbuf, sizeof(tmbuf));
    if (_level == 1) {
      _pre_str = "I" + std::string(tmbuf) + ")[" + file + ":" + std::to_string(line) + "] ";
    } else if (_level == 2) {
      _pre_str = "W" + std::string(tmbuf) + ")[" + file + ":" + std::to_string(line) + "] ";
    } else if (_level == 3) {
      _pre_str = "E" + std::string(tmbuf) + ")[" + file + ":" + std::to_string(line) + "] ";
    } else if (_level == 4) {
      _pre_str = "F" + std::string(tmbuf) + ")[" + file + ":" + std::to_string(line) + "] ";
    }
  }

  virtual ~LogStream()
  {
    flush();
  }

  void flush()
  {
    if (_level == 0) {
#ifndef NDEBUG
      if (_os != nullptr) {
        *_os << _pre_str << _ss.str() << std::endl;
      }
      _ss.clear();
#endif
      return;
    }

    if (_os != nullptr) {
      *_os << _pre_str << _ss.str() << std::endl;
    }
    _ss.clear();
  }

  std::string str(bool prefix = false)
  {
    return prefix ? (_pre_str + _ss.str()) : _ss.str();
  }

  inline void prefix(char* pre_buf, size_t len)
  {
    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    struct tm ltm;
    ::localtime_r(&tv.tv_sec, &ltm);

    // tuncate tailing pthread id
    snprintf(pre_buf,
        len,
        "%02d%02d %02d:%02d:%02d.%06ld (%5lx",
        ltm.tm_mon + 1,
        ltm.tm_mday,
        ltm.tm_hour,
        ltm.tm_min,
        ltm.tm_sec,
        tv.tv_usec,
        (uint64_t)pthread_self());
  }

  inline LogStream& operator<<(int16_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(int val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(int64_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(uint16_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(uint32_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(uint64_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(float val)
  {
    _ss << val;
    return *this;
  }

#ifdef __APPLE__

  inline LogStream& operator<<(size_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(ssize_t val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(pthread_t tid)
  {
    _ss << (uint64_t)tid;
    return *this;
  }

#endif

  inline LogStream& operator<<(double val)
  {
    _ss << val;
    return *this;
  }

  inline LogStream& operator<<(const std::string& val)
  {
    _ss << val;
    return *this;
  }

  template <typename T>
  inline LogStream& operator<<(const std::vector<T>& t)
  {
    iter_list(t);
    return *this;
  }

  template <typename T>
  inline LogStream& operator<<(const std::deque<T>& t)
  {
    iter_list(t);
    return *this;
  }

#if __cplusplus >= 201703L
  template <typename T>
  inline LogStream& operator<<(const std::list<T>& t)
  {
    iter_list(t);
    return *this;
  }
#endif

  template <typename T>
  inline LogStream& operator<<(const std::set<T>& t)
  {
    iter_list(t);
    return *this;
  }

  template <typename T>
  inline LogStream& operator<<(const std::unordered_set<T>& t)
  {
    iter_list(t);
    return *this;
  }

  template <typename K, typename V>
  inline LogStream& operator<<(const std::map<K, V>& t)
  {
    iter_kv(t);
    return *this;
  }

  template <typename K, typename V>
  inline LogStream& operator<<(const std::unordered_map<K, V>& t)
  {
    iter_kv(t);
    return *this;
  }

  template <typename K, typename V>
  inline LogStream& operator<<(const std::pair<K, V>& t)
  {
    *this << t.first << "," << t.second;
    return *this;
  }

  template <typename T>
  void iter_list(const T& t)
  {
    *this << "[";
    size_t i = 0;
    for (auto iter = t.begin(); iter != t.end(); ++iter) {
      *this << *iter;
      if (i++ != t.size() - 1) {
        *this << ",";
      }
    }
    *this << "]";
  }

  template <typename T>
  void iter_kv(const T& t)
  {
    *this << "{";
    size_t i = 0;
    for (auto iter = t.begin(); iter != t.end(); ++iter) {
      *this << iter->first << ":" << iter->second;
      if (i++ != t.size() - 1) {
        *this << ",";
      }
    }
    *this << "}";
  }

private:
  std::stringstream _ss;
  int _level;
  const std::string& _file;
  const int _line;
  std::string _pre_str;
  std::ostream* _os = nullptr;
};

class Logger {
  OMS_SINGLETON(Logger);

private:
  Logger(const Logger&) = delete;
  void operator=(const Logger&) = delete;

public:
  bool init(
      std::string logger_name, std::string_view log_file_path, uint16_t log_max_file_size_mb, uint16_t log_retention_h)
  {
    // check log path and try to create log directory
    namespace fs = std::filesystem;
    fs::path log_path(log_file_path);
    fs::path log_dir = log_path.parent_path();
    if (!fs::exists(log_path)) {
      fs::create_directories(log_dir);
    }

    // create logger
    _default_logger =
        rotating_with_compress_logger_mt(logger_name, log_path, log_max_file_size_mb * 1024 * 1024, log_retention_h);
    if (nullptr != _default_logger) {
      _default_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %s(%#): %v");
      _default_logger->flush_on(spdlog::level::level_enum(Config::instance().log_level.val()));
      _default_logger->set_level(spdlog::level::level_enum(Config::instance().log_level.val()));
      return true;
    }

    return false;
  }

  std::shared_ptr<spdlog::logger> default_logger()
  {
    // return spdlog::default_logger if not yet initialized
    if (nullptr == _default_logger) {
      return spdlog::default_logger();
    }

    return _default_logger;
  }

private:
  std::shared_ptr<spdlog::logger> _default_logger;
};

class StreamLogger : public std::ostringstream {
public:
  explicit StreamLogger(spdlog::source_loc source_loc, spdlog::level::level_enum log_level)
      : _source_loc(source_loc), _log_level(log_level)
  {}

  ~StreamLogger() override
  {
    flush();
  }

  void flush()
  {
    Logger::instance().default_logger()->log(_source_loc, _log_level, str().c_str());
  }

private:
  spdlog::source_loc _source_loc;
  spdlog::level::level_enum _log_level;
};

class EmptyStreamLogger : public std::ostringstream {
public:
  EmptyStreamLogger() = default;

  ~EmptyStreamLogger() override
  {
    /*do nothing*/
  }

  void flush()
  {
    /*do nothing*/
  }
};

void init_log(const char* log_basename);

void replace_spdlog_default_logger();

}  // namespace logproxy
}  // namespace oceanbase

#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5

#define LOG_BASE(level, fmt, ...)                                \
  oceanbase::logproxy::Logger::instance().default_logger()->log( \
      {__FILE__, __LINE__, __FUNCTION__}, level, fmt, ##__VA_ARGS__)

/* !!! HINT: the macros starts with "OMS_STREAM_" are for compatibility with the previous usage of glog,
 * and will be unified into the macros like "OMS_INFO" eventually.
 * Please call macros like "OMS_INFO" from now on. !!! */

#if (LOGGER_LEVEL <= LOG_LEVEL_TRACE)
#define OMS_TRACE(fmt, ...) LOG_BASE(spdlog::level::trace, fmt, ##__VA_ARGS__)
#define OMS_STREAM_TRACE oceanbase::logproxy::StreamLogger({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace)
#else
#define OMS_TRACE(fmt, ...)
#define OMS_STREAM_TRACE oceanbase::logproxy::EmptyStreamLogger()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_DEBUG)
#define OMS_DEBUG(fmt, ...) LOG_BASE(spdlog::level::debug, fmt, ##__VA_ARGS__)
#define OMS_STREAM_DEBUG oceanbase::logproxy::StreamLogger({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug)
#else
#define OMS_DEBUG(fmt, ...)
#define OMS_STREAM_DEBUG oceanbase::logproxy::EmptyStreamLogger()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_INFO)
#define OMS_INFO(fmt, ...) LOG_BASE(spdlog::level::info, fmt, ##__VA_ARGS__)
#define OMS_STREAM_INFO oceanbase::logproxy::StreamLogger({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info)
#else
#define OMS_INFO(fmt, ...)
#define OMS_STREAM_INFO oceanbase::logproxy::EmptyStreamLogger()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_WARN)
#define OMS_WARN(fmt, ...) LOG_BASE(spdlog::level::warn, fmt, ##__VA_ARGS__)
#define OMS_STREAM_WARN oceanbase::logproxy::StreamLogger({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn)
#else
#define OMS_WARN(fmt, ...)
#define OMS_STREAM_WARN oceanbase::logproxy::EmptyStreamLogger()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_ERROR)
#define OMS_ERROR(fmt, ...) LOG_BASE(spdlog::level::err, fmt, ##__VA_ARGS__)
#define OMS_STREAM_ERROR oceanbase::logproxy::StreamLogger({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err)
#else
#define OMS_ERROR(fmt, ...)
#define OMS_STREAM_ERROR oceanbase::logproxy::EmptyStreamLogger()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_FATAL)
#define OMS_FATAL(fmt, ...) LOG_BASE(spdlog::level::critical, fmt, ##__VA_ARGS__)
#define OMS_STREAM_FATAL oceanbase::logproxy::StreamLogger({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical)
#else
#define OMS_FATAL(fmt, ...)
#define OMS_STREAM_FATAL oceanbase::logproxy::EmptyStreamLogger()
#endif