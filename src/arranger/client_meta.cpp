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

#include "arranger/client_meta.h"

namespace oceanbase {
namespace logproxy {

ClientMeta ClientMeta::from_handshake(const Peer& peer, ClientHandshakeRequestMessage& handshake)
{
  ClientMeta meta;
  meta.type = (LogType)handshake.log_type;
  meta.id = handshake.id;
  meta.ip = handshake.ip;
  meta.version = handshake.version;
  meta.configuration = handshake.configuration;

  meta.peer = peer;

  meta.register_time = time(nullptr);
  return meta;
}

LogStream& operator<<(LogStream& ss, MessageVersion version)
{
  ss << (uint16_t)version;
  return ss;
}
}  // namespace logproxy
}  // namespace oceanbase
