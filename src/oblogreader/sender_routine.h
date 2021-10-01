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
#include "oblogreader/oblog_access.h"

namespace oceanbase {
namespace logproxy {

class SenderRoutine : public Thread {
public:
  SenderRoutine(OblogAccess&, BlockingQueue<ILogRecord*>&);

  int init(MessageVersion packet_version, Channel* ch);

  void stop() override;

private:
  void run() override;

  int do_send(const std::vector<ILogRecord*>& records, size_t offset, size_t count);

private:
  OblogAccess& _oblog;

  BlockingQueue<ILogRecord*>& _rqueue;

  Communicator _comm;

  MessageVersion _packet_version;
  PeerInfo _client_peer;
};

}  // namespace logproxy
}  // namespace oceanbase
