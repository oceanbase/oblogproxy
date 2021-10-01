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

#include "google/protobuf/message.h"

#include "common/log.h"
#include "common/common.h"
#include "common/config.h"
#include "codec/message.h"
#include "codec/message_buffer.h"
#include "codec/codec_endian.h"
#include "codec/encoder.h"
#include "logproxy.pb.h"

namespace oceanbase {
namespace logproxy {

static const size_t PB_PACKET_HEADER_SIZE = PACKET_VERSION_SIZE + 1 /*type*/ + 4 /*packet size */;
static const size_t PB_PACKET_HEADER_SIZE_MAGIC = PACKET_MAGIC_SIZE + PB_PACKET_HEADER_SIZE;

int ProtobufEncoder::encode(const Message& msg, MessageBuffer& buffer)
{
  switch (msg.type()) {
    case MessageType::ERROR_RESPONSE: {
      return encode_error_response(msg, buffer);
    }
    case MessageType::HANDSHAKE_REQUEST_CLIENT: {
      return encode_client_handshake_request(msg, buffer);
    }
    case MessageType::HANDSHAKE_RESPONSE_CLIENT: {
      return encode_client_handshake_response(msg, buffer);
    }
    case MessageType::STATUS: {
      return encode_runtime_status(msg, buffer);
    }
    case MessageType::DATA_CLIENT: {
      return encode_data_client(msg, buffer);
    }
    default: {
      OMS_ERROR << "Unknown message type: " << (int)msg.type();
      return OMS_FAILED;
    }
  }
}

static char* encode_message_header(MessageType type, int packet_size, bool magic)
{
  size_t header_len = magic ? PB_PACKET_HEADER_SIZE_MAGIC : PB_PACKET_HEADER_SIZE;
  char* buffer = (char*)malloc(header_len);
  if (nullptr == buffer) {
    OMS_ERROR << "Failed to alloc memory for message header. size=" << header_len;
    return nullptr;
  }

  int offset = 0;
  if (magic) {
    memcpy(buffer, PACKET_MAGIC, sizeof(PACKET_MAGIC));
    offset = sizeof(PACKET_MAGIC);
  }

  int16_t version = cpu_to_be<int16_t>((int16_t)MessageVersion::V2);
  memcpy(buffer + offset, &version, sizeof(version));
  offset += sizeof(version);

  int8_t message_type = (int8_t)type;
  memcpy(buffer + offset, &message_type, sizeof(message_type));
  offset += sizeof(message_type);

  int32_t pb_packet_size = cpu_to_be<int32_t>(packet_size);
  memcpy(buffer + offset, &pb_packet_size, sizeof(pb_packet_size));
  return buffer;
}

int ProtobufEncoder::encode_message(
    const google::protobuf::Message& pb_msg, MessageType type, MessageBuffer& buffer, bool magic)
{
  const size_t serialize_size = pb_msg.ByteSizeLong();
  // TODO max message size
  char* data_buffer = (char*)malloc(serialize_size);
  if (nullptr == data_buffer) {
    OMS_ERROR << "Failed to alloc memory. size=" << serialize_size;
    return OMS_FAILED;
  }

  bool result = pb_msg.SerializeToArray(data_buffer, serialize_size);
  if (!result) {
    OMS_ERROR << "Failed to serialize protobuf message. size=" << serialize_size;
    free(data_buffer);
    return OMS_FAILED;
  }
  //  Md5 md5(data_buffer, serialize_size);
  //  OMS_INFO << "Serialized protobuf message, size: " << serialize_size << ", MD5: " << md5.done();

  char* header_buffer = encode_message_header(type, (int)serialize_size, magic);
  if (nullptr == header_buffer) {
    OMS_ERROR << "Failed to encode client_hand_shake_request 's message header";
    free(data_buffer);
    return OMS_FAILED;
  }

  buffer.push_back(header_buffer, magic ? PB_PACKET_HEADER_SIZE_MAGIC : PB_PACKET_HEADER_SIZE);
  buffer.push_back(data_buffer, serialize_size);
  return OMS_OK;
}

int ProtobufEncoder::encode_error_response(const Message& msg, MessageBuffer& buffer)
{
  const ErrorMessage oms_msg = (const ErrorMessage&)msg;
  ErrorResponse pb_msg;
  pb_msg.set_code(oms_msg.code);
  pb_msg.set_message(oms_msg.message);
  return encode_message(pb_msg, oms_msg.type(), buffer, false);
}

int ProtobufEncoder::encode_client_handshake_request(const Message& msg, MessageBuffer& buffer)
{
  const ClientHandshakeRequestMessage& request_message = (const ClientHandshakeRequestMessage&)msg;
  ClientHandshakeRequest pb_msg;
  pb_msg.set_log_type(request_message.log_type);
  pb_msg.set_ip(request_message.ip);
  pb_msg.set_id(request_message.id);
  pb_msg.set_version(request_message.version);
  pb_msg.set_enable_monitor(request_message.enable_monitor);
  pb_msg.set_configuration(request_message.configuration);
  return encode_message(pb_msg, request_message.type(), buffer, true);
}

int ProtobufEncoder::encode_client_handshake_response(const Message& msg, MessageBuffer& buffer)
{
  const ClientHandshakeResponseMessage& response_message = (const ClientHandshakeResponseMessage&)msg;
  ClientHandshakeResponse pb_msg;
  pb_msg.set_code(response_message.code);
  pb_msg.set_ip(response_message.ip);
  pb_msg.set_version(response_message.version);
  return encode_message(pb_msg, response_message.type(), buffer, false);
}

int ProtobufEncoder::encode_runtime_status(const Message& msg, MessageBuffer& buffer)
{
  const RuntimeStatusMessage& runtime_status_message = (const RuntimeStatusMessage&)msg;
  RuntimeStatus pb_msg;
  pb_msg.set_ip(runtime_status_message.ip);
  pb_msg.set_port(runtime_status_message.port);
  pb_msg.set_stream_count(runtime_status_message.stream_count);
  pb_msg.set_worker_count(runtime_status_message.worker_count);
  return encode_message(pb_msg, runtime_status_message.type(), buffer, false);
}

int ProtobufEncoder::encode_data_client(const Message& msg, MessageBuffer& buffer)
{
  RecordDataMessage& record_data_message = (RecordDataMessage&)msg;
  size_t raw_len = 0;
  MessageBuffer records_buffer;
  int ret = record_data_message.encode_log_records(records_buffer, raw_len);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode log records. ret=" << ret;
    return ret;
  }

  MessageBufferReader records_buffer_reader(records_buffer);
  const size_t records_size = records_buffer_reader.byte_size();

  std::string pb_record_string(records_size, 0);
  // FIXME... big data packet copy here due to perf laging
  ret = records_buffer_reader.read((char*)pb_record_string.c_str(), records_size);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read buffer from records buffer, size=" << records_size;
    return OMS_FAILED;
  }

  RecordData pb_msg;
  pb_msg.set_records(pb_record_string);
  pb_msg.set_compress_type((int)record_data_message.compress_type);
  pb_msg.set_raw_len(raw_len);
  pb_msg.set_compressed_len(records_size);
  pb_msg.set_count(record_data_message.count());
  return encode_message(pb_msg, record_data_message.type(), buffer, false);
}

}  // namespace logproxy
}  // namespace oceanbase
