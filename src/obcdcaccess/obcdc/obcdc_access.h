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

#include "obcdc_entry.h"

// handle diff header
#ifdef _OBCDC_H_
#include "libobcdc.h"
#else
#include "liboblog.h"
#endif

// handle diff namespace
#ifdef _OBCDC_NS_
using ObCdcFactory = oceanbase::libobcdc::ObCDCFactory;
using IObCdc = oceanbase::libobcdc::IObCDCInstance;
using ObCdcError = oceanbase::libobcdc::ObCDCError;
using ObCdcErrorCb = oceanbase::libobcdc::ERROR_CALLBACK;

inline IObCdc* construct_obcdc_instance(ObCdcFactory* factory)
{
  return factory->construct_obcdc();
}
#else
using ObCdcFactory = oceanbase::liboblog::ObLogFactory;
using IObCdc = oceanbase::liboblog::IObLog;
using ObCdcError = oceanbase::liboblog::ObLogError;
using ObCdcErrorCb = oceanbase::liboblog::ERROR_CALLBACK;

inline IObCdc* construct_obcdc_instance(ObCdcFactory* factory)
{
  return factory->construct_oblog();
}
#endif

namespace oceanbase {
namespace logproxy {

class ObCdcAccess : public IObCdcAccess {
public:
  ObCdcAccess();

  ~ObCdcAccess() override;

  int init(const std::map<std::string, std::string>& configs, uint64_t start_timestamp) override;

  int init_with_us(const std::map<std::string, std::string>& configs, uint64_t start_timestamp_us) override;

  int start() override;

  void stop() override;

  int fetch(ILogRecord*& record) override;

  int fetch(ILogRecord*& record, uint64_t timeout_us) override;

  void release(ILogRecord* record) override;

public:
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
  static void handle_error(const ObCdcError& error);

private:
  // state
  uint64_t _start_timestamp = 0;
  uint64_t _start_timestamp_us = 0;

  // params
  uint64_t _wait_timeout_us = 100;

  // ObCdc
  IObCdc* _obcdc;
  ObCdcFactory* _obcdc_factory;
};

}  // namespace logproxy
}  // namespace oceanbase
