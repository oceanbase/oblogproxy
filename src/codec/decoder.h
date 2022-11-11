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

#include "codec/message.h"
#include "codec/msg_buf.h"

namespace oceanbase {
namespace logproxy {

class Channel;

class MessageDecoder {
public:
  virtual PacketError decode(Channel& ch, MessageVersion version, Message*& message) = 0;

protected:
  size_t _header_len;
};

class LegacyDecoder : public MessageDecoder {
  OMS_AVOID_COPY(LegacyDecoder);
  OMS_SINGLETON(LegacyDecoder);

public:
  PacketError decode(Channel& ch, MessageVersion version, Message*& message) override;

private:
  static int decode_handshake_request(Channel& ch, Message*& message);
};

class ProtobufDecoder : public MessageDecoder {
  OMS_AVOID_COPY(ProtobufDecoder);
  OMS_SINGLETON(ProtobufDecoder);

public:
  PacketError decode(Channel& ch, MessageVersion version, Message*& message) override;

private:
  int decode_payload(MessageType type, const MsgBuf& buffer, Message*& msg);

  static int decode_handshake_request(MsgBufReader& buffer_reader, Message*& msg);

  static int decode_handshake_response(MsgBufReader& buffer_reader, Message*& msg);

  static int decode_runtime_status(MsgBufReader& buffer_reader, Message*& msg);

  static int decode_data_client(MsgBufReader& buffer_reader, Message*& msg);
};

}  // namespace logproxy
}  // namespace oceanbase
