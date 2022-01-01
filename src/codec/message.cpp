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

#include "codec/message.h"
#include "codec/msg_buf.h"
#include "common/log.h"
#include "common/common.h"
#include "common/config.h"
#include "LogMsgFactory.h"
#include "LogRecord.h"
#include "MsgHeader.h"
#include "lz4.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

#ifndef NEED_MAPPING_CLASS
const std::string _s_logmsg_type = "LogRecordImpl";
#else
const std::string _s_logmsg_type = "BinlogRecordImpl";
#endif

bool is_version_available(uint16_t version_val)
{
  return version_val >= 0 && version_val <= 2;
}

bool is_type_available(int8_t type_val)
{
  return type_val >= -1 && type_val <= 8;
}

Message::Message(MessageType type) : _type(type)
{}

const std::string& Message::debug_string() const
{
  switch (_type) {
    case MessageType::HANDSHAKE_REQUEST_CLIENT:
      return ((ClientHandshakeRequestMessage*)this)->to_string();
    case MessageType::HANDSHAKE_RESPONSE_CLIENT:
      return ((ClientHandshakeResponseMessage*)this)->to_string();
    case MessageType::ERROR_RESPONSE:
      return ((ErrorMessage*)this)->to_string();
    case MessageType::DATA_CLIENT: {
      static std::string msg = "DATA_CLIENT";
      return msg;
    }
    case MessageType::STATUS: {
      // TODO
      static std::string msg = "RUNTIME STATUS";
      return msg;
    }
    case MessageType::UNKNOWN:
    default: {
      static std::string msg = "UNKNOWN MESSAGE";
      return msg;
    }
  }
}

ErrorMessage::ErrorMessage() : Message(MessageType::ERROR_RESPONSE)
{}

ErrorMessage::ErrorMessage(int code, const std::string& message)
    : Message(MessageType::ERROR_RESPONSE), code(code), message(message)
{}

ClientHandshakeRequestMessage::ClientHandshakeRequestMessage() : Message(MessageType::HANDSHAKE_REQUEST_CLIENT)
{}

ClientHandshakeRequestMessage::ClientHandshakeRequestMessage(
    int log_type, const char* ip, const char* id, const char* version, bool enable_monitor, const char* configuration)
    : Message(MessageType::HANDSHAKE_REQUEST_CLIENT),
      log_type(log_type),
      ip(ip),
      id(id),
      version(version),
      enable_monitor(enable_monitor),
      configuration(configuration)
{}

ClientHandshakeResponseMessage::ClientHandshakeResponseMessage(
    int in_code, const std::string& in_ip, const std::string& in_version)
    : Message(MessageType::HANDSHAKE_RESPONSE_CLIENT), code(in_code), server_ip(in_ip), server_version(in_version)
{}

RuntimeStatusMessage::RuntimeStatusMessage() : Message(MessageType::STATUS)
{}

RuntimeStatusMessage::RuntimeStatusMessage(const char* ip, int port, int stream_count, int worker_count)
    : Message(MessageType::STATUS), ip(ip), port(port), stream_count(stream_count), worker_count(worker_count)
{}

RecordDataMessage::RecordDataMessage() : Message(MessageType::DATA_CLIENT)
{}

RecordDataMessage::RecordDataMessage(std::vector<ILogRecord*>&& in_records)
    : Message(MessageType::DATA_CLIENT), records(in_records)
{}

RecordDataMessage::RecordDataMessage(const std::vector<ILogRecord*>& in_records, size_t offset, size_t count)
    : Message(MessageType::DATA_CLIENT)
{
  records.reserve(count);
  records.assign(in_records.begin() + offset, in_records.begin() + count);
}

RecordDataMessage::~RecordDataMessage() = default;

int RecordDataMessage::encode_log_records(MsgBuf& buffer, size_t& raw_len) const
{
  switch (compress_type) {
    case CompressType::PLAIN: {
      int ret = encode_log_records_plain(buffer);
      if (OMS_OK == ret) {
        raw_len = buffer.byte_size();
      }
      return ret;
    }
    default: {
      OMS_ERROR << "Unsupported compress type: " << (int)compress_type;
      return OMS_FAILED;
    }
  }
}

int RecordDataMessage::encode_log_records_plain(MsgBuf& buffer) const
{
  const size_t count = records.size();
  MsgBuf tmp_buffer;

  for (size_t i = 0; i < count; ++i) {
    ILogRecord* log_record = records[i];

    size_t size = 0;
    // got independ address
    const char* log_record_buffer = log_record->getFormatedString(&size);
    if (nullptr == log_record_buffer) {
      OMS_ERROR << "Failed to serialize log record";
      return OMS_FAILED;
    }

    const MsgHeader* header = (const MsgHeader*)(log_record_buffer);
    if (_s_config.verbose_packet.val()) {
      OMS_DEBUG << "Encode LogMessage Header, type: " << header->m_msgType << ", version: " << header->m_version
                << ", size: " << header->m_size;
    }

    size_t calc_size = header->m_size + sizeof(MsgHeader);
    if (calc_size != size) {
      if (calc_size > size) {
        OMS_FATAL << "LogMessage Invalid, header calc size:" << calc_size << " > buffer size:" << size;
        return OMS_FAILED;
      }
      if (_s_config.verbose_packet.val()) {
        OMS_WARN << "LogMessage header size:" << calc_size << " != toString size:" << size << ". adjust to header size";
      }
      size = calc_size;
    }

    // LogMessage buffer freed outside
    tmp_buffer.push_back((char*)log_record_buffer, size, false);
  }

  buffer.swap(tmp_buffer);
  return OMS_OK;
}

int RecordDataMessage::encode_log_records_lz4(MsgBuf& buffer, size_t& raw_len) const
{
  int ret = encode_log_records_plain(buffer);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode log records(plain) in lz4 mode";
    return ret;
  }
  if (buffer.count() == 0) {
    OMS_WARN << "No log records";
    return OMS_OK;
  }

  raw_len = buffer.byte_size();
  char* plain_buffer = nullptr;
  if (buffer.count() == 1) {
    plain_buffer = buffer.begin()->buffer();
  } else {
    plain_buffer = (char*)malloc(raw_len);
    if (nullptr == plain_buffer) {
      OMS_ERROR << "Failed to alloc memory. size=" << raw_len;
      return OMS_FAILED;
    }
    MsgBufReader reader(buffer);
    ret = reader.read(plain_buffer, raw_len);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to read buffer. size=" << raw_len;
      free(plain_buffer);
      return ret;
    }
  }

  const int compress_bound = LZ4_compressBound(raw_len);
  char* compressed_buffer = (char*)malloc(compress_bound);
  if (nullptr == compressed_buffer) {
    OMS_ERROR << "Failed to alloc compressed buffer. size=" << compress_bound;
    if (buffer.count() != 1) {
      free(plain_buffer);
    }
    return OMS_FAILED;
  }

  const int compressed_size = LZ4_compress_default(plain_buffer, compressed_buffer, raw_len, compress_bound);
  if (compressed_size <= 0) {
    OMS_ERROR << "LZ4 compress failed. src size=" << raw_len << ", compressed bound=" << compress_bound
              << ", compress return=" << compressed_size;
    if (buffer.count() != 1) {
      free(plain_buffer);
    }
    free(compressed_buffer);
    return OMS_FAILED;
  }

  if (buffer.count() != 1) {
    free(plain_buffer);
  }

  OMS_DEBUG << "Encode client data success(lz4). raw_len=" << raw_len << ", compressed_size=" << compressed_size;

  MsgBuf compressed_message_buffer;
  compressed_message_buffer.push_back(compressed_buffer, compressed_size);
  buffer.swap(compressed_message_buffer);
  return OMS_OK;
}

int RecordDataMessage::decode_log_records(
    CompressType in_compress_type, const char* buffer, size_t buffer_size, size_t raw_len, int expect_count)
{
  compress_type = in_compress_type;

  switch (compress_type) {
    case CompressType::PLAIN: {
      return decode_log_records_plain(buffer, buffer_size, expect_count);
    }
    case CompressType::LZ4: {
      return decode_log_records_lz4(buffer, buffer_size, raw_len, expect_count);
    }
    default: {
      OMS_ERROR << "Unsupported compress type: " << (int)compress_type;
      return OMS_FAILED;
    }
  }
}

int RecordDataMessage::decode_log_records_plain(const char* buffer, size_t size, int expect_count)
{
  if (nullptr == buffer || 0 == size) {
    OMS_ERROR << "Invalid argument. buffer=" << (intptr_t)buffer << ", buffer_size=" << size;
    return OMS_FAILED;
  }

  std::vector<ILogRecord*> log_records;

  int count = 0;
  size_t offset = 0;
  while (offset < size) {
    if (size - offset < sizeof(MsgHeader)) {
      OMS_ERROR << "Buffer is not enough for a log record. size=" << size - offset;
      return OMS_FAILED;
    }

    const MsgHeader* header = (const MsgHeader*)(buffer + offset);
    if (_s_config.verbose_packet.val()) {
      OMS_DEBUG << "Decode LogMessage Header, type: " << header->m_msgType << ", version: " << header->m_version
                << ", size: " << header->m_size;
    }

    uint32_t log_record_size = header->m_size + sizeof(MsgHeader);
    ILogRecord* log_record = LogMsgFactory::createLogRecord(_s_logmsg_type, buffer + offset, log_record_size);
    if (nullptr == log_record) {
      OMS_ERROR << "Failed to create log record";
      return OMS_FAILED;
    }
    log_records.push_back(log_record);

    offset += log_record_size;
    ++count;
  }

  if (count != expect_count) {
    OMS_ERROR << "Expect " << expect_count << " record, but " << log_records.size() << " parsed";
    return OMS_FAILED;
  }

  OMS_DEBUG << "Total " << log_records.size() << " log records have been decoded from buffer with size: " << size;
  records.swap(log_records);

  return OMS_OK;
}

int RecordDataMessage::decode_log_records_lz4(const char* buffer, size_t buffer_size, size_t raw_size, int expect_count)
{
  char* decompressed_buffer = (char*)malloc(raw_size);
  if (nullptr == decompressed_buffer) {
    OMS_ERROR << "Failed to alloc memory. count=" << raw_size;
    return OMS_FAILED;
  }

  size_t decompressed_size = LZ4_decompress_safe(buffer, decompressed_buffer, buffer_size, raw_size);
  if (decompressed_size != raw_size) {
    OMS_ERROR << "Failed to decompress log record buffer. compressed_size=" << buffer_size << ". raw_len=" << raw_size
              << ". lz4 decompres return=" << decompressed_size;
    free(decompressed_buffer);
    return OMS_FAILED;
  }

  OMS_DEBUG << "Decompress log record buffer success";

  int ret = decode_log_records_plain(decompressed_buffer, raw_size, expect_count);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to decode log record(plain)";
  }
  free(decompressed_buffer);
  return ret;
}

}  // namespace logproxy
}  // namespace oceanbase
