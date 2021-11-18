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

#include <assert.h>
#include "common/common.h"
#include "common/log.h"
#include "oblogreader/oblog_access.h"

namespace oceanbase {
namespace logproxy {

OblogAccess::OblogAccess()
{
  _oblog_factory = new oceanbase::liboblog::ObLogFactory();
  assert(_oblog_factory != nullptr);
  _oblog = _oblog_factory->construct_oblog();
  assert(_oblog != nullptr);
}

OblogAccess::~OblogAccess()
{
  if (_oblog != nullptr) {
    assert(_oblog_factory != nullptr);
    _oblog->destroy();
    _oblog_factory->deconstruct(_oblog);
    _oblog = nullptr;
  }
  if (_oblog_factory != nullptr) {
    delete _oblog_factory;
    _oblog_factory = nullptr;
  }
}

void OblogAccess::handle_error(const liboblog::ObLogError& error)
{
  // TODO...
}

int OblogAccess::init(const std::map<std::string, std::string>& configs, uint64_t start_timestamp)
{
  _start_timestamp = start_timestamp;

  OMS_INFO << "======== Start liboblog configs ======== ";
  for (auto& entry : configs) {
    if (entry.first == "start_timestamp") {
      OMS_INFO << entry.first << "=" << start_timestamp;
    } else {
      OMS_INFO << entry.first << "=" << entry.second;
    }
  }
  OMS_INFO << "======== End Start liboblog configs ======== ";
  int ret = _oblog->init(configs, start_timestamp, &handle_error);
  if (ret != 0) {
    OMS_FATAL << "Failed to init liboblog, ret: " << ret;
    return ret;
  }
  OMS_INFO << "Successfully init liboblog";
  return OMS_OK;
}

int OblogAccess::start()
{
  int ret = _oblog->launch();
  if (ret != 0) {
    OMS_FATAL << "Failed to launch liboblog, ret: " << ret;
    return ret;
  }
  OMS_INFO << "Successfully start liboblog";
  return OMS_OK;
}

void OblogAccess::stop()
{
  _oblog->stop();
}

int OblogAccess::fetch(ILogRecord*& record)
{
  return fetch(record, _wait_timeout_us);
}

int OblogAccess::fetch(ILogRecord*& record, uint64_t timeout_us)
{
  return _oblog->next_record(&record, timeout_us);
}

void OblogAccess::release(ILogRecord* record)
{
  _oblog->release_record(record);
}

}  // namespace logproxy
}  // namespace oceanbase
