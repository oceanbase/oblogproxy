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
#include <cstdint>
#include "log_record.h"

namespace oceanbase {
namespace logproxy {

class IObCdcAccess {

public:
  IObCdcAccess() : _so_handle(nullptr){};
  virtual ~IObCdcAccess() = default;

  virtual int init(const std::map<std::string, std::string>& configs, uint64_t start_timestamp) = 0;
  virtual int init_with_us(const std::map<std::string, std::string>& configs, uint64_t start_timestamp_us) = 0;
  virtual int start() = 0;
  virtual void stop() = 0;
  virtual int fetch(ILogRecord*& record) = 0;
  virtual int fetch(ILogRecord*& record, uint64_t timeout_us) = 0;
  virtual void release(ILogRecord* record) = 0;

public:
  void set_handle(void* handle)
  {
    this->_so_handle = handle;
  }

  void* get_handle()
  {
    return _so_handle;
  }

private:
  void* _so_handle;
};

extern "C" IObCdcAccess* create(void* handle);
extern "C" void destroy(IObCdcAccess* obcdc);

}  // namespace logproxy
}  // namespace oceanbase
