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

#include "common/thread.h"
#include "common/blocking_queue.hpp"
#include "obaccess/oblog_config.h"
#include "obaccess/clog_meta_routine.h"
#include "oblogreader/oblog_access.h"

namespace oceanbase {
namespace logproxy {

class ClogMetaRoutine;
class ObLogReader;

class ReaderRoutine : public Thread {
public:
  ReaderRoutine(ObLogReader&, OblogAccess&, BlockingQueue<ILogRecord*>&);

  int init(const OblogConfig& config);

  void stop() override;

private:
  void run() override;

private:
  ObLogReader& _reader;
  OblogAccess& _oblog;

  ClogMetaRoutine _clog_meta;

  BlockingQueue<ILogRecord*>& _queue;
};

}  // namespace logproxy
}  // namespace oceanbase
