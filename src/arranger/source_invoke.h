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

#include "common/config.h"
#include "common/thread.h"
#include "arranger/client_meta.h"

namespace oceanbase {
namespace logproxy {

class SourceInvoke {

public:
  static int invoke(Communicator& communicator, const ClientMeta& client, const std::string& configuration);
};

class SourceWaiter {
  OMS_SINGLETON(SourceWaiter);
  OMS_AVOID_COPY(SourceWaiter);

public:
  void add(int pid, const ClientMeta& client);

private:
  void remove(int pid);

private:
  class WaitThread : public Thread {
  public:
    WaitThread(int pid, const ClientMeta& client);

    void run() override;

  private:
    const int _pid;
    ClientMeta _client;
  };

private:
  std::mutex _op_lock;
  std::unordered_map<int, WaitThread*> _childs;
};

}  // namespace logproxy
}  // namespace oceanbase
