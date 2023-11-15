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

#include "client_meta.h"

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

int ClientMeta::init_from_json(const rapidjson::Value& json)
{
  type = static_cast<LogType>(json["type"].GetInt());
  id = json["id"].GetString();
  ip = json["ip"].GetString();
  version = json["version"].GetString();
  configuration = json["configuration"].GetString();

  peer.from_json(json["peer"]);

  register_time = json["register_time"].GetInt64();
  enable_monitor = json["enable_monitor"].GetBool();
  packet_version = static_cast<MessageVersion>(json["packet_version"].GetInt());
  return OMS_OK;
}

void ClientMeta::to_json(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const
{
  writer.Key(CLIENT_META);
  writer.StartObject();
  writer.Key("type");writer.Int(type);
  writer.Key("id");writer.String(id.c_str());
  writer.Key("ip");writer.String(ip.c_str());
  writer.Key("version");writer.String(version.c_str());
  writer.Key("configuration");writer.String(configuration.c_str());
  peer.to_json(writer);
  writer.Key("register_time");writer.Uint64(register_time);
  writer.Key("enable_monitor");writer.Bool(enable_monitor);
  writer.Key("packet_version");writer.Int((int)packet_version);
  writer.EndObject();
}

const string& ClientMeta::to_string() const
{
  return Model::to_string();
}

LogStream& operator<<(LogStream& ss, MessageVersion version)
{
  ss << (uint16_t)version;
  return ss;
}

}  // namespace logproxy
}  // namespace oceanbase
