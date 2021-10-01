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

#include "communication/channel.h"
#include "codec/decoder.h"

namespace oceanbase {
namespace logproxy {

/*
 * =========== Message Header ============
 * [4] type
 */
PacketError LegacyDecoder::decode(Channel* ch, MessageVersion version, Message*& message)
{
  // type
  int32_t type = -1;
  if (ch->readn((char*)&type, 4) != OMS_OK) {
    OMS_ERROR << "Failed to read message header, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return PacketError::NETWORK_ERROR;
  }
  type = be_to_cpu<int8_t>(type);
  if (!is_type_available(type)) {
    OMS_ERROR << "Invalid packet type:" << type << ", ch:" << ch->peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  int ret = OMS_OK;
  switch ((MessageType)type) {
    case MessageType::HANDSHAKE_REQUEST_CLIENT:
      ret = decode_handshake_request(ch, message);
      break;
    default:
      // We don not care other request type as a server decoder
      break;
  }

  return ret == OMS_OK ? PacketError::SUCCESS : PacketError::PROTOCOL_ERROR;
}

static int read_varstr(Channel* ch, std::string& val)
{
  uint32_t len = 0;
  if (ch->readn((char*)&len, 4) != OMS_OK) {
    return OMS_FAILED;
  }
  len = be_to_cpu<uint32_t>(len);

  val.reserve(len);
  if (ch->readn((char*)val.data(), len) != OMS_OK) {
    return OMS_FAILED;
  }
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
  if (read_varstr(ch, msg->ip) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  if (read_varstr(ch, msg->id) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  if (read_varstr(ch, msg->version) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  if (read_varstr(ch, msg->configuration) != OMS_OK) {
    OMS_ERROR << "Failed to read message Client IP, ch:" << ch->peer().id() << ", error:" << strerror(errno);
    return OMS_FAILED;
  }
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
