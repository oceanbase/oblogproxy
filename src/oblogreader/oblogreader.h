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

#include "common/common.h"
#include "communication/communicator.h"
#include "oblogreader/oblog_access.h"
#include "oblogreader/oblog_config.h"
#include "oblogreader/reader_routine.h"
#include "oblogreader/sender_routine.h"

namespace oceanbase {
namespace logproxy {

class ObLogReader {
  OMS_SINGLETON(ObLogReader);
  OMS_AVOID_COPY(ObLogReader);

public:
  virtual ~ObLogReader();

  int init(const std::string& id, MessageVersion packet_version, Channel* ch, const OblogConfig& config);

  int stop();

  void join();

  int start();

private:
  OblogAccess _oblog;

  BlockingQueue<ILogRecord*> _queue{Config::instance().record_queue_size.val()};
  ReaderRoutine _reader{_oblog, _queue};
  SenderRoutine _sender{_oblog, _queue};
};

}  // namespace logproxy
}  // namespace oceanbase
