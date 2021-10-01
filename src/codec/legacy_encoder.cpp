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

#include "MsgHeader.h"

#include "common/config.h"
#include "codec/encoder.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

LegacyEncoder::LegacyEncoder()
{
  /*
   * [4] reponse code
   * [1+varstr] Server IP
   * [1+varstr] Server Version
   */
  _funcs.emplace((int8_t)MessageType::HANDSHAKE_RESPONSE_CLIENT, [](const Message& in_msg, MessageBuffer& buffer) {
    const ClientHandshakeResponseMessage& msg = (const ClientHandshakeResponseMessage&)in_msg;
    size_t len = 4 + 1 + msg.ip.size() + 1 + msg.version.size();
    char* buf = (char*)malloc(len);
    if (buf == nullptr) {
      OMS_ERROR << "Failed to encode handshake request due to failed to alloc memory";
      return OMS_FAILED;
    }
    // response code
    size_t offset = 0;
    memset(buf, 0, 4);
    offset += 4;

    // Server IP
    uint8_t varlen = msg.ip.size();
    memcpy(buf + offset, &varlen, 1);
    offset += 1;
    memcpy(buf + offset, msg.ip.c_str(), varlen);
    offset += varlen;

    // Server version
    varlen = msg.version.size();
    memcpy(buf + offset, &varlen, 1);
    offset += 1;
    memcpy(buf + offset, msg.version.c_str(), varlen);

    buffer.push_back(buf, len);
    return OMS_OK;
  });

  /*
   * [4] Packet len
   * [1] Compress type
   * [4] Original data len
   * [4] Compressed data len
   * [Compressed data len] Datas
   * ===== for each message in Datas
   *    [4] Message ID
   *    [4] Message Len
   *    [Message Len] LogMsg
   */
  _funcs.emplace((int8_t)MessageType::DATA_CLIENT, [](const Message& in_msg, MessageBuffer& buffer) {
    const RecordDataMessage& msg = (const RecordDataMessage&)in_msg;

    uint32_t total_size = 0;
    for (size_t i = 0; i < msg.records.size(); ++i) {
      ILogRecord* record = msg.records[i];

      size_t size = 0;
      // got independ address
      const char* log_record_buffer = record->getFormatedString(&size);
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
          OMS_WARN << "LogMessage header size:" << calc_size << " != toString size:" << size
                   << ". adjust to header size";
        }
        size = calc_size;
      }

      total_size += calc_size;

      uint32_t seq = cpu_to_be<uint32_t>(i);
      uint32_t len = cpu_to_be<uint32_t>(calc_size);
      buffer.push_back_copy((char*)&seq, 4);
      buffer.push_back_copy((char*)&len, 4);
      buffer.push_back((char*)log_record_buffer, calc_size, false);
    }

    uint32_t packet_len = cpu_to_be<uint32_t>(total_size + 7);
    total_size = cpu_to_be<uint32_t>(total_size);

    char* buf = (char*)malloc(13);
    memcpy(buf, &packet_len, sizeof(packet_len));
    memset(buf + 4, 0, 1);  // CompressType::PLAIN
    memcpy(buf + 5, &total_size, sizeof(total_size));
    memcpy(buf + 9, &total_size, sizeof(total_size));
    buffer.push_front(buf, 13);
    return OMS_OK;
  });

  _funcs.emplace((int8_t)MessageType::STATUS, [](const Message& in_msg, MessageBuffer& buffer) { return OMS_OK; });

  /*
   * [4] Reponse code
   * [4+varstr] Error message
   */
  _funcs.emplace((int8_t)MessageType::ERROR_RESPONSE, [](const Message& in_msg, MessageBuffer& buffer) {
    const ErrorMessage& msg = (const ErrorMessage&)in_msg;

    size_t len = 4 + 4 + msg.message.size();
    char* buf = (char*)malloc(len);
    if (buf == nullptr) {
      OMS_ERROR << "Failed to encode error message due to failed to alloc memory";
      return OMS_FAILED;
    }
    // response code
    int code = cpu_to_be<int>(msg.code);
    memcpy(buf, &code, 4);

    size_t offset = 4;

    // Error message
    uint32_t varlen = cpu_to_be<uint32_t>(msg.message.size());
    memcpy(buf + offset, &varlen, 4);
    offset += 4;
    memcpy(buf + offset, msg.message.c_str(), varlen);

    buffer.push_back(buf, len);
    return OMS_OK;
  });
}

int LegacyEncoder::encode(const Message& msg, MessageBuffer& buffer)
{
  return _funcs[(int8_t)msg.type()](msg, buffer);
}

}  // namespace logproxy
}  // namespace oceanbase