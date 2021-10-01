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

#include <string>
#include <ostream>

#include "common/model.h"
#include "common/config.h"
#include "communication/communicator.h"
#include "codec/message.h"

namespace oceanbase {
namespace logproxy {

struct ClientId {

public:
  static ClientId of(const std::string& id)
  {
    ClientId clientId;
    clientId.set(id);
    return clientId;
  }

  bool operator==(const ClientId& rhs) const
  {
    return _id == rhs._id;
  }

  bool operator<(const ClientId& rhs) const
  {
    return _id < rhs._id;
  }

  std::size_t operator()(const ClientId& a) const
  {
    return std::hash<std::string>{}(a._id);
  }

  friend LogStream& operator<<(LogStream& ss, const ClientId& id)
  {
    ss << id._id;
    return ss;
  }

  const std::string& to_string() const
  {
    return _id;
  }

  std::string get() const
  {
    return _id;
  }

  void set(const std::string& id)
  {
    _id = id;
  }

private:
  std::string _id;
};

struct ClientMeta : public Model {
  OMS_MF_ENABLE_COPY(ClientMeta);

public:
  ClientMeta() = default;

  OMS_MF(LogType, type);
  OMS_MF(ClientId, id);

  OMS_MF(std::string, ip);
  OMS_MF(std::string, version);
  OMS_MF(std::string, configuration);

  OMS_MF(PeerInfo, peer);

  OMS_MF(time_t, register_time);
  OMS_MF_DFT(bool, enable_monitor, false);
  OMS_MF_DFT(MessageVersion, packet_version, MessageVersion::V2);

public:
  static ClientMeta from_handshake(const PeerInfo&, ClientHandshakeRequestMessage&);
};

LogStream& operator<<(LogStream& ss, MessageVersion version);

}  // namespace logproxy
}  // namespace oceanbase
