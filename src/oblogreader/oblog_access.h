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

#include <string>
#include <map>
#include <stdint.h>

// this three MUST BE ordered like this
#include "LogRecord.h"
#ifdef USE_OBCDC_NS
#include "libobcdc.h"

#ifdef USE_LIBOBLOG_3

using oceanbase::liboblog::IObLog;
using oceanbase::liboblog::ObLogError;
using oceanbase::liboblog::ObLogFactory;

#else

typedef oceanbase::libobcdc::IObCDCInstance IObLog;
typedef oceanbase::libobcdc::ObCDCFactory ObLogFactory;
typedef oceanbase::libobcdc::ObCDCError ObLogError;

#define construct_oblog construct_obcdc

#endif

#else
#include "liboblog.h"

using oceanbase::liboblog::IObLog;
using oceanbase::liboblog::ObLogError;
using oceanbase::liboblog::ObLogFactory;

#endif

#ifndef OB_TIMEOUT
#define OB_TIMEOUT -4012
#endif

#ifndef OB_SUCCESS
#define OB_SUCCESS 0
#endif

namespace oceanbase {
namespace logproxy {

#ifndef NEED_MAPPING_CLASS
using namespace oceanbase::logmessage;
#endif

class OblogAccess {
public:
  OblogAccess();

  virtual ~OblogAccess();

  int init(const std::map<std::string, std::string>& configs, uint64_t start_timestamp);

  int init_with_us(const std::map<std::string, std::string>& configs, uint64_t start_timestamp_us);

  int start();

  void stop();

  int fetch(ILogRecord*& record);

  int fetch(ILogRecord*& record, uint64_t timeout_us);

  void release(ILogRecord* record);

  uint64_t start_timestamp() const
  {
    return _start_timestamp;
  }

  void set_start_timestamp(uint64_t start_timestamp)
  {
    _start_timestamp = start_timestamp;
  }

  uint64_t wait_timeout_us() const
  {
    return _wait_timeout_us;
  }

  void set_wait_timeout_us(uint64_t wait_timeout_us)
  {
    _wait_timeout_us = wait_timeout_us;
  }

private:
  static void handle_error(const ObLogError& error);

private:
  // state
  uint64_t _start_timestamp = 0;
  uint64_t _start_timestamp_us = 0;

  // params
  uint64_t _wait_timeout_us = 100;

  // liboblog
  IObLog* _oblog;
  ObLogFactory* _oblog_factory;
};

}  // namespace logproxy
}  // namespace oceanbase
