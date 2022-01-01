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

#include <unordered_map>
#include <mutex>
#include "common/common.h"
#include "arranger/source_meta.h"
#include "arranger/client_meta.h"

namespace oceanbase {
namespace logproxy {

class Arranger {
  OMS_SINGLETON(Arranger);
  OMS_AVOID_COPY(Arranger);

public:
  int init();

  int run_foreground();

  int create(const ClientMeta& client);

  int close_client(const ClientMeta& client, const std::string& msg = "");

private:
  EventResult on_msg(const PeerInfo&, const Message&);

  int auth(ClientMeta& client, std::string& errmsg);

  int start_source(const ClientMeta& client, const std::string& configuration);

  void response_error(const PeerInfo&, MessageVersion version, ErrorCode code, const std::string&);

  int close_client_locked(const ClientMeta& client, const std::string& msg);

private:
  /**
   * <ClientId, sink_peer>
   */
  std::unordered_map<ClientId, PeerInfo, ClientId> _client_peers;

private:
  std::mutex _op_mutex;

  Communicator _accepter;

  std::string _localhost;
  std::string _localip;
};

}  // namespace logproxy
}  // namespace oceanbase
