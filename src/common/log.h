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

#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <sstream>
#include <iostream>
#if __cplusplus >= 201703L
#include <list>
#endif
#include <sys/time.h>

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

template <typename T>
class LogMessage {
public:
  explicit LogMessage(int level, const std::string& file, const int line)
  {
    if (_stream == nullptr) {
      _stream = new T(level, file, line);
    }
  }

  ~LogMessage()
  {
    if (_stream != nullptr) {
      _stream->flush();
    }
  }

  T& stream()
  {
    return *_stream;
  }

private:
  T* _stream = nullptr;
};

void init_log(const char* argv0, bool restart = false);

}  // namespace logproxy
}  // namespace oceanbase

#ifdef WITH_GLOG

#include "glog/logging.h"

#define OMS_DEBUG DLOG(INFO)
#define OMS_INFO LOG(INFO)
#define OMS_WARN LOG(WARNING)
#define OMS_ERROR LOG(ERROR)
#define OMS_FATAL LOG(ERROR)
#else
#define OMS_DEBUG oceanbase::logproxy::LogMessage<oceanbase::logproxy::LogStream>(0, __FILE__, __LINE__).stream()
#define OMS_INFO oceanbase::logproxy::LogMessage<oceanbase::logproxy::LogStream>(1, __FILE__, __LINE__).stream()
#define OMS_WARN oceanbase::logproxy::LogMessage<oceanbase::logproxy::LogStream>(2, __FILE__, __LINE__).stream()
#define OMS_ERROR oceanbase::logproxy::LogMessage<oceanbase::logproxy::LogStream>(3, __FILE__, __LINE__).stream()
#define OMS_FATAL oceanbase::logproxy::LogMessage<oceanbase::logproxy::LogStream>(4, __FILE__, __LINE__).stream()
#endif
