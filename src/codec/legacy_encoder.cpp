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

#include "lz4.h"
#include "MsgHeader.h"

#include "common/config.h"
#include "common/guard.hpp"
#include "codec/encoder.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

static int compress_data(const RecordDataMessage& msg, MsgBuf& buffer, size_t& raw_len)
{
  std::vector<std::pair<const char*, size_t>> ptrs;
  ptrs.reserve(msg.count());

  uint32_t total_size = 0;
  for (size_t i = 0; i < msg.count(); ++i) {
    auto& record = msg.records[i + msg.offset()];

    size_t size = 0;
    // got independ address
    const char* logmsg_buf = record->getFormatedString(&size);
    if (logmsg_buf == nullptr) {
      OMS_ERROR << "Failed to serialize logmsg";
      return OMS_FAILED;
    }
    if (_s_config.verbose_packet.val()) {
      //      const MsgHeader* header = (const MsgHeader*)(logmsg_buf);
      //      OMS_DEBUG << "Encode logmsg Header, type: " << header->m_msgType << ", version: " << header->m_version
      //                << ", size: " << header->m_size;
    }
    ptrs.emplace_back(logmsg_buf, size);
    total_size += (size + 8);
  }

  char* raw = (char*)malloc(total_size);
  FreeGuard<char*> fg_raw(raw);
  if (raw == nullptr) {
    OMS_ERROR << "Failed to allocate raw buffer to compress, size:" << total_size;
    return OMS_FAILED;
  }

  int bound_size = LZ4_COMPRESSBOUND(total_size);
  char* compressed = (char*)malloc(bound_size);
  FreeGuard<char*> fg(compressed);
  if (compressed == nullptr) {
    OMS_ERROR << "Failed to allocate LZ4 bound buffer, size:" << bound_size;
    return OMS_FAILED;
  }

  uint32_t idx = msg.idx;
  size_t offset = 0;
  for (auto& ptr : ptrs) {
    size_t block_size = ptr.second;
    uint32_t seq_be = cpu_to_be(idx++);
    uint32_t size_be = cpu_to_be((uint32_t)block_size);
    memcpy(raw + offset, &seq_be, 4);
    memcpy(raw + offset + 4, &size_be, 4);
    memcpy(raw + offset + 8, ptr.first, block_size);
    offset += (block_size + 8);
  }

  int compressed_size = LZ4_compress_fast(raw, compressed, total_size, bound_size, 1);
  if (compressed_size <= 0) {
    OMS_ERROR << "Failed to compress logmsg, raw size:" << total_size << ", bound size:" << bound_size;
    return OMS_FAILED;
  }
  if (_s_config.verbose.val()) {
    OMS_DEBUG << "compress packet raw from size:" << total_size << " to compressed size:" << compressed_size;
  }

  uint32_t packet_len_be = cpu_to_be((uint32_t)compressed_size + 9);
  uint32_t orginal_size_be = cpu_to_be(total_size);
  uint32_t compressed_size_be = cpu_to_be((uint32_t)compressed_size);
  char* buf = (char*)malloc(13);
  memcpy(buf, &packet_len_be, 4);
  memset(buf + 4, (uint8_t)CompressType::LZ4, 1);
  memcpy(buf + 5, &orginal_size_be, 4);
  memcpy(buf + 9, &compressed_size_be, 4);
  buffer.push_back(buf, 13);

  raw_len = total_size + 13;

  // transfer ownership to Msgbuf
  fg.release();
  buffer.push_back(compressed, compressed_size);
  return OMS_OK;
}

LegacyEncoder::LegacyEncoder()
{
  /*
   * [4] response code
   * [1+varstr] Server IP
   * [1+varstr] Server Version
   */
  _funcs.emplace((int8_t)MessageType::HANDSHAKE_RESPONSE_CLIENT, [](const Message& in_msg, MsgBuf& buffer, size_t&) {
    const ClientHandshakeResponseMessage& msg = (const ClientHandshakeResponseMessage&)in_msg;
    size_t len = 4 + 1 + msg.server_ip.size() + 1 + msg.server_version.size();
    char* buf = (char*)malloc(len);
    if (buf == nullptr) {
      OMS_ERROR << "Failed to encode handshake request due to failed to alloc memory";
      return OMS_FAILED;
    }

    OMS_INFO << "Encode handshake response to send:" << msg.debug_string();

    // Response code
    memset(buf, 0, 4);

    // Server IP
    size_t offset = 4;
    uint8_t varlen = msg.server_ip.size();
    memcpy(buf + offset, &varlen, 1);
    offset += 1;
    memcpy(buf + offset, msg.server_ip.c_str(), varlen);
    offset += varlen;

    // Server version
    varlen = msg.server_version.size();
    memcpy(buf + offset, &varlen, 1);
    offset += 1;
    memcpy(buf + offset, msg.server_version.c_str(), varlen);

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
  _funcs.emplace((int8_t)MessageType::DATA_CLIENT, [](const Message& in_msg, MsgBuf& buffer, size_t& raw_len) {
    const RecordDataMessage& msg = (const RecordDataMessage&)in_msg;

    if (msg.compress_type == CompressType::LZ4) {
      return compress_data(msg, buffer, raw_len);
    }

    uint32_t idx = msg.idx;
    uint32_t total_size = 0;
    for (size_t i = 0; i < msg.count(); ++i) {
      auto& record = msg.records[i + msg.offset()];
      size_t size = 0;
      // got independ address
      const char* logmsg_buf = record->getFormatedString(&size);
      if (logmsg_buf == nullptr) {
        OMS_ERROR << "Failed to serialize log record";
        return OMS_FAILED;
      }
      if (_s_config.verbose_packet.val()) {
        const MsgHeader* header = (const MsgHeader*)(logmsg_buf);
        OMS_DEBUG << "Encode logmsg Header, type: " << header->m_msgType << ", version: " << header->m_version
                  << ", size: " << header->m_size;
      }

      uint32_t seq_be = cpu_to_be(idx++);
      uint32_t size_be = cpu_to_be((uint32_t)size);
      buffer.push_back_copy((char*)&seq_be, 4);
      buffer.push_back_copy((char*)&size_be, 4);
      buffer.push_back((char*)logmsg_buf, size, false);

      total_size += (size + 8);
    }

    uint32_t packet_len_be = cpu_to_be(total_size + 9);
    total_size = cpu_to_be(total_size);

    char* buf = (char*)malloc(4 + 1 + 4 + 4);
    memcpy(buf, &packet_len_be, 4);
    memset(buf + 4, (uint8_t)CompressType::PLAIN, 1);
    memcpy(buf + 5, &total_size, 4);
    memcpy(buf + 9, &total_size, 4);
    buffer.push_front(buf, 13);
    return OMS_OK;
  });

  _funcs.emplace((int8_t)MessageType::STATUS, [](const Message& in_msg, MsgBuf& buffer, size_t&) { return OMS_OK; });

  /*
   * [4] Reponse code
   * [4+varstr] Error message
   */
  _funcs.emplace((int8_t)MessageType::ERROR_RESPONSE, [](const Message& in_msg, MsgBuf& buffer, size_t&) {
    const ErrorMessage& msg = (const ErrorMessage&)in_msg;

    size_t len = 4 + 4 + msg.message.size();
    char* buf = (char*)malloc(len);
    if (buf == nullptr) {
      OMS_ERROR << "Failed to encode error message due to failed to alloc memory";
      return OMS_FAILED;
    }

    // Error message
    uint32_t code_be = cpu_to_be((uint32_t)msg.code);
    memcpy(buf, &code_be, 4);
    uint32_t varlen_be = cpu_to_be((uint32_t)msg.message.size());
    memcpy(buf + 4, &varlen_be, 4);
    if (msg.message.size() != 0) {
      memcpy(buf + 8, msg.message.c_str(), msg.message.size());
    }

    // buf's ownership transfered to buffer
    buffer.push_back(buf, len);
    return OMS_OK;
  });
}

int LegacyEncoder::encode(const Message& msg, MsgBuf& buffer, size_t& raw_len)
{
  int ret = _funcs[(int8_t)msg.type()](msg, buffer, raw_len);
  if (ret == OMS_FAILED) {
    return OMS_FAILED;
  }

  // append header
  size_t len = 2 + 4;
  char* buf = (char*)malloc(len);

  // version code
  memset(buf, 0, 2);

  // response type code
  uint32_t msg_type_be = cpu_to_be((uint32_t)msg.type());
  memcpy(buf + 2, &msg_type_be, 4);
  buffer.push_front(buf, len);
  return ret;
}

}  // namespace logproxy
}  // namespace oceanbase