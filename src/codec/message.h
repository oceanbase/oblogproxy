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
#include <vector>

#include "log_record.h"

#include "model.h"

namespace oceanbase {
namespace logproxy {
extern const std::string _s_logmsg_type;

enum class MessageVersion : uint16_t {
  V0 = 0,
  V1 = 1,
  V2 = 2,
};

bool is_version_available(uint16_t version_val);

enum class MessageType : int8_t {
  ERROR_RESPONSE = -1,
  UNKNOWN = 0,
  HANDSHAKE_REQUEST_CLIENT = 1,
  HANDSHAKE_RESPONSE_CLIENT = 2,
  //  HANDSHAKE_REQUEST_LOGREADER = 3,
  //  HANDSHAKE_RESPONSE_LOGREADER = 4,
  //  DATA_LOGREADER = 5,
  DATA_CLIENT = 6,
  STATUS = 7,
  //  STATUS_LOGREADER = 8,

  SET_GLOBAL_CONFIG = 100,
  SET_READER_CONFIG = 101,
  SET_LIBOBLOG_CONFIG = 102,

};

bool is_type_available(int8_t type_val);

enum class CompressType {
  PLAIN = 0,
  LZ4 = 1,
};

enum class PacketError {
  SUCCESS,
  IGNORE,
  OUT_OF_MEMORY,
  PROTOCOL_ERROR,
  NETWORK_ERROR,
};

enum ErrorCode {
  ////////// 0~499: process error ////////////
  /**
   * general error
   */
  NONE = 0,

  /**
   * inner error
   */
  E_INNER = 1,

  /**
   * failed to connect
   */
  E_CONNECT = 2,

  /**
   * exceed max retry connect count
   */
  E_MAX_RECONNECT = 3,

  /**
   * user callback throws exception
   */
  E_USER = 4,

  ////////// 500~: recv data error ////////////
  /**
   * unknown data protocol
   */
  E_PROTOCOL = 500,

  /**
   * unknown _header type
   */
  E_HEADER_TYPE = 501,

  /**
   * failed to auth
   */
  NO_AUTH = 502,

  /**
   * unknown compress type
   */
  E_COMPRESS_TYPE = 503,

  /**
   * length not match
   */
  E_LEN = 504,

  /**
   * failed to parse data
   */
  E_PARSE = 505
};

constexpr char PACKET_MAGIC[] = {'x', 'i', '5', '3', 'g', ']', 'q'};
constexpr size_t PACKET_MAGIC_SIZE = sizeof(PACKET_MAGIC);
const size_t PACKET_VERSION_SIZE = 2;

class Message {
public:
  explicit Message(MessageType message_type);

  virtual ~Message() = default;

  inline MessageType type() const
  {
    return _type;
  }

  inline MessageVersion version() const
  {
    return _version;
  }

  void set_version(MessageVersion version)
  {
    _version = version;
  }

  virtual uint64_t size()
  {
    return 0;
  }

  virtual const std::string& debug_string() const;

protected:
  MessageType _type;
  MessageVersion _version = MessageVersion::V2;
};

class ErrorMessage : public Message, public Model {
public:
  ErrorMessage();

  ErrorMessage(int code, const std::string& message);

private:
  OMS_MF(int, code);
  OMS_MF(std::string, message);
};

class ClientHandshakeRequestMessage : public Message, public Model {
public:
  ClientHandshakeRequestMessage();

  ClientHandshakeRequestMessage(int log_type, const char* ip, const char* id, const char* version, bool enable_monitor,
      const char* configuration);

  ~ClientHandshakeRequestMessage() override = default;

  OMS_MF_DFT(uint8_t, log_type, 0);
  OMS_MF(std::string, id);
  OMS_MF(std::string, ip);
  OMS_MF(std::string, version);
  OMS_MF(std::string, configuration);
  OMS_MF_DFT(bool, enable_monitor, false);
};

class ClientHandshakeResponseMessage : public Message, public Model {
public:
  ClientHandshakeResponseMessage(int code, const std::string& in_ip, const std::string& in_version);

  ~ClientHandshakeResponseMessage() override = default;

private:
  OMS_MF_DFT(int, code, -1);
  OMS_MF(std::string, server_ip);
  OMS_MF(std::string, server_version);
};

class RuntimeStatusMessage : public Message, public Model {
public:
  RuntimeStatusMessage();

  RuntimeStatusMessage(const char* ip, int port, int stream_count, int worker_count);

  ~RuntimeStatusMessage() override = default;

private:
  OMS_MF(std::string, ip);
  OMS_MF_DFT(int, port, -1);
  OMS_MF_DFT(int, stream_count, -1);
  OMS_MF_DFT(int, worker_count, -1);
};

class MsgBuf;

class RecordDataMessage : public Message {
public:
  ~RecordDataMessage() override;

  explicit RecordDataMessage(std::vector<ILogRecord*>& records);

  RecordDataMessage(std::vector<ILogRecord*>& records, size_t offset, size_t count);

  inline size_t offset() const
  {
    return _offset;
  }

  inline size_t count() const
  {
    return _count;
  }

  int encode_log_records(MsgBuf& buffer, size_t& raw_len) const;

  int decode_log_records(CompressType compress_type, const char* buffer, size_t size, size_t raw_len, int expect_count);

protected:
  int decode_log_records_plain(const char* buffer, size_t size, int expect_count);

  int decode_log_records_lz4(const char* buffer, size_t size, size_t raw_size, int expect_count);

  int encode_log_records_plain(MsgBuf& buffer) const;

  int encode_log_records_lz4(MsgBuf& buffer, size_t& raw_len) const;

public:
  CompressType compress_type = CompressType::PLAIN;
  std::vector<ILogRecord*>& records;
  size_t _offset = 0;
  size_t _count = 0;

  // index to count message seq for a client session
  uint32_t idx = 0;
};

}  // namespace logproxy
}  // namespace oceanbase
