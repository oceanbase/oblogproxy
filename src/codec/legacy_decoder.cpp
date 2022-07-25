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

#include <stdint.h>
#include "common/guard.hpp"
#include "communication/channel.h"
#include "codec/decoder.h"

#include "legacy.pb.h"

namespace oceanbase {
namespace logproxy {

static PacketError decode_v1(Channel* ch, Message*& message)
{
  // V1 is protobuf handshake packet:
  // [4] pb packet length
  // [pb packet length] pb buffer
  uint32_t payload_size = 0;
  if (ch->readn((char*)(&payload_size), 4) != OMS_OK) {
    OMS_ERROR << "Failed to read message payload size, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return PacketError::NETWORK_ERROR;
  }
  payload_size = be_to_cpu(payload_size);
  // FIXME.. use an mem pool
  char* payload_buf = (char*)malloc(payload_size);
  if (nullptr == payload_buf) {
    OMS_ERROR << "Failed to malloc memory for message data. size:" << payload_size << ", ch:" << ch->peer().id();
    return PacketError::OUT_OF_MEMORY;
  }
  FreeGuard<char*> payload_buf_guard(payload_buf);
  if (ch->readn(payload_buf, payload_size) != 0) {
    OMS_ERROR << "Failed to read message. ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return PacketError::NETWORK_ERROR;
  }
  payload_buf_guard.release();

  legacy::PbPacket pb_packet;
  bool ret = pb_packet.ParseFromArray(payload_buf, payload_size);
  if (!ret) {
    OMS_ERROR << "Failed to parse payload, ch:" << ch->peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  if ((MessageType)pb_packet.type() != MessageType::HANDSHAKE_REQUEST_CLIENT) {
    OMS_ERROR << "Invalid packet type:" << pb_packet.type() << ", ch:" << ch->peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  legacy::ClientHandShake handshake;
  ret = handshake.ParseFromString(pb_packet.payload());
  if (!ret) {
    OMS_ERROR << "Failed to parse handshake, ch:" << ch->peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  ClientHandshakeRequestMessage* msg = new (std::nothrow) ClientHandshakeRequestMessage;
  msg->log_type = handshake.log_type();
  msg->ip = handshake.client_ip();
  msg->id = handshake.client_id();
  msg->version = handshake.client_version();
  msg->configuration = handshake.configuration();
  message = msg;
  return PacketError::SUCCESS;
}

/*
 * =========== Message Header ============
 * [4] type
 */
PacketError LegacyDecoder::decode(Channel* ch, MessageVersion version, Message*& message)
{
  OMS_DEBUG << "Legacy decode with, version: " << (int)version;

  if (version == MessageVersion::V1) {
    return decode_v1(ch, message);
  }

  // type
  uint32_t type = -1;
  if (ch->readn((char*)&type, 4) != OMS_OK) {
    OMS_ERROR << "Failed to read message header, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return PacketError::NETWORK_ERROR;
  }
  type = be_to_cpu(type);
  if (!is_type_available(type)) {
    OMS_ERROR << "Invalid packet type:" << type << ", ch:" << ch->peer().id();
    return PacketError::PROTOCOL_ERROR;
  }
  OMS_DEBUG << "Legacy message type:" << type;

  int ret = OMS_OK;
  switch ((MessageType)type) {
    case MessageType::HANDSHAKE_REQUEST_CLIENT:
      ret = decode_handshake_request(ch, message);
      break;
    default:
      // We don not care other request type as a server decoder
      return PacketError::IGNORE;
  }

  return ret == OMS_OK ? PacketError::SUCCESS : PacketError::PROTOCOL_ERROR;
}

static int read_varstr(Channel* ch, std::string& val)
{
  uint32_t len = 0;
  if (ch->readn((char*)&len, 4) != OMS_OK) {
    return OMS_FAILED;
  }
  len = be_to_cpu(len);

  char* buf = (char*)malloc(len);
  FreeGuard<char*> ff(buf);

  if (ch->readn(buf, len) != OMS_OK) {
    return OMS_FAILED;
  }
  val.assign(buf, len);
  return OMS_OK;
}

/*
 * [1] log type
 * [4+varstr] Client IP
 * [4+varstr] Client ID
 * [4+varstr] Client Version
 * [4+varstr] Configuration
 */
int LegacyDecoder::decode_handshake_request(Channel* ch, Message*& message)
{
  ClientHandshakeRequestMessage* msg = new (std::nothrow) ClientHandshakeRequestMessage;
  if (ch->readn((char*)&msg->log_type, 1) != OMS_OK) {
    OMS_ERROR << "Failed to read message log_type, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  OMS_DEBUG << "log type:" << (int)msg->log_type;

  if (read_varstr(ch, msg->ip) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  OMS_DEBUG << "client ip: " << msg->ip;

  if (read_varstr(ch, msg->id) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  OMS_DEBUG << "client id: " << msg->id;

  if (read_varstr(ch, msg->version) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  OMS_DEBUG << "client version: " << msg->version;

  if (read_varstr(ch, msg->configuration) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  OMS_DEBUG << "configuration: " << msg->configuration;
  message = msg;
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
