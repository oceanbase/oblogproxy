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

#include "thread.h"
#include "timer.h"
#include "blocking_queue.hpp"

namespace oceanbase {
namespace logproxy {

class ObLogReader;

class SenderRoutine : public Thread {
public:
  SenderRoutine(ObLogReader& reader, BlockingQueue<ILogRecord*>& rqueue);

  int init(MessageVersion packet_version, const Peer& peer, IObCdcAccess* obcdc);

  void stop() override;

private:
  void run() override;

  int do_send(std::vector<ILogRecord*>& records, size_t offset, size_t count);

private:
  ObLogReader& _reader;
  IObCdcAccess* _obcdc;

  BlockingQueue<ILogRecord*>& _rqueue;

  Comm _comm;

  MessageVersion _packet_version;
  Peer _client_peer;

  Timer _stage_timer;

  uint32_t _msg_seq = 0;
};

}  // namespace logproxy
}  // namespace oceanbase
