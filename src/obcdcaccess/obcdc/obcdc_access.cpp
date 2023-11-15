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

#include <cassert>

#include "common.h"
#include "log.h"
#include "obcdc_access.h"

namespace oceanbase {
namespace logproxy {

ObCdcAccess::ObCdcAccess()
{
  _obcdc_factory = new ObCdcFactory();
  assert(nullptr != _obcdc_factory);
  _obcdc = construct_obcdc_instance(_obcdc_factory);
  assert(nullptr != _obcdc);
}

ObCdcAccess::~ObCdcAccess()
{
  if (nullptr != _obcdc) {
    assert(nullptr != _obcdc_factory);
    _obcdc->stop();
//    _obcdc->destroy();
//    _obcdc_factory->deconstruct(_obcdc);
    _obcdc = nullptr;
  }

  if (_obcdc_factory != nullptr) {
    delete _obcdc_factory;
    _obcdc_factory = nullptr;
  }
}

int ObCdcAccess::init(const std::map<std::string, std::string>& configs, uint64_t start_timestamp)
{
  _start_timestamp = start_timestamp;

  OMS_INFO("======== Start libobcdc configs ======== ");
  for (auto& entry : configs) {
    if (entry.first == "first_start_timestamp") {
      OMS_INFO("{}={}", entry.first, _start_timestamp);
    } else {
      OMS_INFO("{}={}", entry.first, entry.second);
    }
  }
  OMS_INFO("======== End Start libobcdc configs ========");

  int ret = _obcdc->init(configs, _start_timestamp, &handle_error);
  if (ret != 0) {
    OMS_FATAL("Failed to init libobcdc, ret: {}", ret);
    return ret;
  }

  OMS_INFO("Successfully init libobcdc.");
  return OMS_OK;
}

int ObCdcAccess::init_with_us(const std::map<std::string, std::string>& configs, uint64_t start_timestamp_us)
{
#ifndef WITH_US_TIMESTAMP
  OMS_FATAL("Unsupported start time in us for current version");
  return OMS_FAILED;
#else
  _start_timestamp_us = start_timestamp_us;

  OMS_INFO("======== Start libobcdc configs with us ========");
  for (auto& entry : configs) {
    if (entry.first == "first_start_timestamp_us") {
      OMS_INFO("{}={}", entry.first, _start_timestamp_us);
    } else {
      OMS_INFO("{}={}", entry.first, entry.second);
    }
  }
  OMS_INFO("======== End Start libobcdc configs ========");
#ifdef _OBCDC_V2_
  OMS_INFO("Init liboblog with version 2 with start timestamp: {}, and the given start timestamp(us): {}",
      _start_timestamp_us / 1000,
      _start_timestamp_us);
  int ret = _obcdc->init(configs, _start_timestamp_us / 1000, &handle_error);
#else
  int ret = _obcdc->init_with_start_tstamp_usec(configs, _start_timestamp_us, &handle_error);
#endif
  if (ret != 0) {
    OMS_FATAL("Failed to init libobcdc, ret: {}", ret);
    return ret;
  }

  OMS_INFO("Successfully init libobcdc");
  return OMS_OK;
#endif
}

int ObCdcAccess::start()
{
  int ret = _obcdc->launch();
  if (ret != 0) {
    OMS_FATAL("Failed to launch libobcdc, ret: {}", ret);
    return ret;
  }

  OMS_INFO("Successfully start libobcdc.");
  return OMS_OK;
}

void ObCdcAccess::stop()
{
  _obcdc->stop();
}

int ObCdcAccess::fetch(ILogRecord*& record)
{
  return fetch(record, _wait_timeout_us);
}

int ObCdcAccess::fetch(ILogRecord*& record, uint64_t timeout_us)
{
  return _obcdc->next_record(&record, (int64_t)timeout_us);
}

void ObCdcAccess::release(ILogRecord* record)
{
  _obcdc->release_record(record);
}

void ObCdcAccess::handle_error(const ObCdcError& error)
{
  // TODO...
  OMS_ERROR("Failed to init libobcdc, error: {}", error.errmsg_);
}

// work for dlopen
IObCdcAccess* create(void* handle)
{
  IObCdcAccess* obcdc = new ObCdcAccess();
  obcdc->set_handle(handle);
  return obcdc;
}

void destroy(IObCdcAccess* obcdc)
{
  delete obcdc;
}

}  // namespace logproxy
}  // namespace oceanbase
