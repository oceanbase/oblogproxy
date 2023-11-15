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

#include "google/protobuf/io/zero_copy_stream.h"
#include "common/guard.hpp"
#include "common/config.h"
#include "communication/channel.h"
#include "codec/decoder.h"

#include "logproxy.pb.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

/*
 * =========== Message Header ============
 * [1] type
 * [4] packet size
 * [packet_size] pb payload
 */
const size_t MESSAGE_HEADER_SIZE_V2 = 1 + 4;

PacketError ProtobufDecoder::decode(Channel& ch, MessageVersion version, Message*& message)
{
  char header_buf[MESSAGE_HEADER_SIZE_V2];
  if (ch.readn(header_buf, MESSAGE_HEADER_SIZE_V2) != OMS_OK) {
    OMS_ERROR << "Failed to read message header, ch:" << ch.peer().id() << ", error:" << strerror(errno);
    return PacketError::NETWORK_ERROR;
  }

  // type
  int8_t type = -1;
  memcpy(&type, header_buf, 1);
  if (!is_type_available(type)) {
    OMS_ERROR << "Invalid packet type:" << type << ", ch:" << ch.peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  // payload size
  uint32_t payload_size = 0;
  memcpy(&payload_size, header_buf + 1, 4);
  payload_size = be_to_cpu(payload_size);

  // TODO... suppose that no large message
  if (payload_size > Config::instance().max_packet_bytes.val()) {
    OMS_ERROR << "Too large message size: " << payload_size
              << ", exceed max: " << Config::instance().max_packet_bytes.val();
    return PacketError::PROTOCOL_ERROR;
  }

  // FIXME.. use an mem pool
  char* payload_buf = (char*)malloc(payload_size);
  if (nullptr == payload_buf) {
    OMS_ERROR << "Failed to malloc memory for message data. size:" << payload_size << ", ch:" << ch.peer().id();
    return PacketError::OUT_OF_MEMORY;
  }
  FreeGuard<char*> payload_buf_guard(payload_buf);
  if (ch.readn(payload_buf, payload_size) != 0) {
    OMS_ERROR << "Failed to read message. ch:" << ch.peer().id() << ", error:" << strerror(errno);
    return PacketError::NETWORK_ERROR;
  }

  payload_buf_guard.release();

  MsgBuf buffer;
  buffer.push_back(payload_buf, payload_size);
  // Transfer buffer owner to message_buffer
  // buffer's memory will freed by MessageBuffer

  int ret = decode_payload((MessageType)type, buffer, message);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to decode_payload message, ch:" << ch.peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  return PacketError::SUCCESS;
}

int ProtobufDecoder::decode_payload(MessageType type, const MsgBuf& buffer, Message*& msg)
{
  MsgBufReader reader(buffer);

  switch ((MessageType)type) {
    case MessageType::HANDSHAKE_REQUEST_CLIENT: {
      return decode_handshake_request(reader, msg);
    }
    case MessageType::HANDSHAKE_RESPONSE_CLIENT: {
      return decode_handshake_response(reader, msg);
    }
    case MessageType::STATUS: {
      return decode_runtime_status(reader, msg);
    }
    case MessageType::DATA_CLIENT: {
      return decode_data_client(reader, msg);
    }
    default: {
      OMS_ERROR << "Unknown message type: " << (int)type;
    } break;
  }
  // should not go here
  return OMS_FAILED;
}

class ZeroCopyStreamAdapter : public ::google::protobuf::io::ZeroCopyInputStream {
public:
  explicit ZeroCopyStreamAdapter(MsgBufReader& buffer_reader) : _buffer_reader(buffer_reader)
  {}

  ~ZeroCopyStreamAdapter() override = default;

  bool Next(const void** data, int* size) override
  {
    return 0 == _buffer_reader.next((const char**)data, size);
  }

  void BackUp(int count) override
  {
    _buffer_reader.backward(count);
  }

  bool Skip(int count) override
  {
    return 0 == _buffer_reader.forward(count);
  }

  ::google::protobuf::int64 ByteCount() const override
  {
    return _buffer_reader.read_size();
  }

private:
  MsgBufReader& _buffer_reader;
};

int ProtobufDecoder::decode_handshake_request(MsgBufReader& buffer_reader, Message*& out_msg)
{
  ZeroCopyStreamAdapter zero_copy_stream(buffer_reader);
  ClientHandshakeRequest pb_msg;
  bool result = pb_msg.ParseFromZeroCopyStream(&zero_copy_stream);
  if (!result) {
    OMS_ERROR << "Failed to parse protobuf message from buffer";
    return OMS_FAILED;
  }

  ClientHandshakeRequestMessage* msg = new (std::nothrow) ClientHandshakeRequestMessage((int)pb_msg.log_type(),
      pb_msg.ip().c_str(),
      pb_msg.id().c_str(),
      pb_msg.version().c_str(),
      pb_msg.enable_monitor(),
      pb_msg.configuration().c_str());

  if (nullptr == msg) {
    OMS_ERROR << "Failed to create client_hand_shake_request_message.";
    return OMS_FAILED;
  }

  out_msg = msg;
  return OMS_OK;
}

int ProtobufDecoder::decode_handshake_response(MsgBufReader& buffer_reader, Message*& msg)
{
  ZeroCopyStreamAdapter zero_copy_stream(buffer_reader);
  ClientHandshakeResponse pb_msg;
  bool result = pb_msg.ParseFromZeroCopyStream(&zero_copy_stream);
  if (!result) {
    OMS_ERROR << "Failed to parse protobuf message from buffer";
    return OMS_FAILED;
  }

  // copy field
  ClientHandshakeResponseMessage* response_msg = new (std::nothrow)
      ClientHandshakeResponseMessage((int)pb_msg.code(), pb_msg.ip().c_str(), pb_msg.version().c_str());

  if (nullptr == response_msg) {
    OMS_ERROR << "Failed to create ClientHandShakeResponseMessage.";
    return OMS_FAILED;
  }

  msg = response_msg;
  return OMS_OK;
}

int ProtobufDecoder::decode_runtime_status(MsgBufReader& buffer_reader, Message*& _msg)
{
  ZeroCopyStreamAdapter zero_copy_stream(buffer_reader);
  RuntimeStatus pb_msg;
  bool result = pb_msg.ParseFromZeroCopyStream(&zero_copy_stream);
  if (!result) {
    OMS_ERROR << "Failed to parse protobuf message from buffer";
    return OMS_FAILED;
  }

  RuntimeStatusMessage* msg = new (std::nothrow) RuntimeStatusMessage(
      pb_msg.ip().c_str(), (int)pb_msg.port(), (int)pb_msg.stream_count(), (int)pb_msg.worker_count());

  if (nullptr == msg) {
    OMS_ERROR << "Failed to create RuntimeStatusMessage.";
    return OMS_FAILED;
  }

  _msg = msg;
  return OMS_OK;
}

int ProtobufDecoder::decode_data_client(MsgBufReader& buffer_reader, Message*& _msg)
{
  RecordData pb_msg;
  //  bool ret = pb_msg.ParseFromArray(str.c_str(), size);
  ZeroCopyStreamAdapter zero_copy_stream(buffer_reader);
  bool ret = pb_msg.ParseFromZeroCopyStream(&zero_copy_stream);
  if (!ret) {
    OMS_ERROR << "Failed to parse protobuf message from buffer";
    return OMS_FAILED;
  }

  std::vector<ILogRecord*> records;
  RecordDataMessage* msg = new (std::nothrow) RecordDataMessage(records);
  if (nullptr == msg) {
    OMS_ERROR << "Failed to create RecordDataMessage.";
    return OMS_FAILED;
  }
  ret = msg->decode_log_records((CompressType)pb_msg.compress_type(),
      pb_msg.records().data(),
      pb_msg.records().size(),
      pb_msg.raw_len(),
      pb_msg.count());
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to decode log record";
    delete msg;
    return ret;
  }

  ILogRecord* record = msg->records[msg->offset()];
  if (_s_config.verbose_packet.val()) {
    OMS_INFO << "Fetched record from LogProxy, "
             << "compress type: " << pb_msg.compress_type() << ","
             << "raw_len: " << pb_msg.raw_len() << ","
             << "compressed_len: " << pb_msg.compressed_len() << ","
             << "count: " << pb_msg.count() << ","
             << "records size: " << pb_msg.records().size() << ","
             << "record_type: " << record->recordType() << ","
             << "timestamp: " << record->getTimestamp() << ","
             << "checkpoint: " << record->getFileNameOffset() << ","
             << "dbname: " << record->dbname() << ","
             << "tbname: " << record->tbname();
  }

  _msg = msg;
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase