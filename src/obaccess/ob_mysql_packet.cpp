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

#include <limits.h>
#include <poll.h>

#include "obaccess/ob_mysql_packet.h"
#include "codec/message_buffer.h"
#include "communication/io.h"
#include "common/log.h"
#include "common/common.h"

// https://dev.mysql.com/doc/internals/en/capability-flags.html
// copy from include/mysql_com.h (mariadb source code)
#define CLIENT_LONG_PASSWORD 0               /* obsolete flag */
#define CLIENT_MYSQL 1ULL                    /* mysql/old mariadb server/client */
#define CLIENT_FOUND_ROWS 2ULL               /* Found instead of affected rows */
#define CLIENT_LONG_FLAG 4ULL                /* Get all column flags */
#define CLIENT_CONNECT_WITH_DB 8ULL          /* One can specify db on connect */
#define CLIENT_NO_SCHEMA 16ULL               /* Don't allow database.table.column */
#define CLIENT_COMPRESS 32ULL                /* Can use compression protocol */
#define CLIENT_ODBC 64ULL                    /* Odbc client */
#define CLIENT_LOCAL_FILES 128ULL            /* Can use LOAD DATA LOCAL */
#define CLIENT_PROTOCOL_41 512ULL            /* New 4.1 protocol */
#define CLIENT_INTERACTIVE 1024ULL           /* This is an interactive client */
#define CLIENT_SSL 2048ULL                   /* Switch to SSL after handshake */
#define CLIENT_IGNORE_SIGPIPE 4096ULL        /* IGNORE sigpipes */
#define CLIENT_TRANSACTIONS 8192ULL          /* Client knows about transactions */
#define CLIENT_SECURE_CONNECTION 32768ULL    /* New 4.1 authentication */
#define CLIENT_MULTI_STATEMENTS (1ULL << 16) /* Enable/disable multi-stmt support */
#define CLIENT_MULTI_RESULTS (1ULL << 17)    /* Enable/disable multi-results */
#define CLIENT_PS_MULTI_RESULTS (1ULL << 18) /* Multi-results in PS-protocol */
#define CLIENT_PLUGIN_AUTH (1ULL << 19)      /* Client supports plugin authentication */
#define CLIENT_CONNECT_ATTRS (1ULL << 20)    /* Client supports connection attributes */
/* Enable authentication response packet to be larger than 255 bytes. */
#define CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA (1ULL << 21)
/* Don't close the connection for a connection with expired password. */
#define CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS (1ULL << 22)
/**
  Capable of handling server state change information. Its a hint to the
  server to include the state change information in Ok packet.
*/
#define CLIENT_SESSION_TRACK (1ULL << 23)
/* Client no longer needs EOF packet */
#define CLIENT_DEPRECATE_EOF (1ULL << 24)

#define CLIENT_SUPPORT_ORACLE_MODE (1ULL << 27)  // for oracle mode only
#define CLIENT_PROGRESS_OBSOLETE (1ULL << 29)
#define CLIENT_SSL_VERIFY_SERVER_CERT (1ULL << 30)

namespace oceanbase {
namespace logproxy {

int recv_mysql_packet(int fd, int timeout, uint32_t& packet_length, uint8_t& sequence, MessageBuffer& message_buffer)
{
  struct pollfd pollfd;
  pollfd.fd = fd;
  pollfd.events = POLLIN | POLLERR;
  pollfd.revents = 0;
  int ret = poll(&pollfd, 1, timeout);
  if (0 == ret) {
    OMS_DEBUG << "Timeout when receiving mysql packet. time(in millisecond)=" << timeout;
    return OMS_TIMEOUT;
  }
  if (ret < 0) {
    OMS_ERROR << "Failed to receive mysql packet. poll return error. system error=" << strerror(errno);
    return OMS_FAILED;
  }

  if (pollfd.revents & POLLERR) {
    OMS_ERROR << "Failed to receive mysql packet because of poll.revents & POLLERR";
    return OMS_FAILED;
  }

  uint32_t packet_header = 0;
  ret = readn(fd, &packet_header, 4);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read packet length. error=" << strerror(errno);
    return ret;
  }

  sequence = packet_header & 0xFF000000;
  packet_length = le_to_cpu<uint32_t>(packet_header & 0x00FFFFFF);
  if (packet_length >= UINT32_MAX - sizeof(packet_header)) {
    OMS_ERROR << "Got invalid packet length, too length. length=" << packet_length;
    return OMS_FAILED;
  }

  char* buffer = (char*)malloc(packet_length + sizeof(packet_header));
  if (nullptr == buffer) {
    OMS_ERROR << "Failed to malloc memory for mysql hand shake packet. length=" << packet_length;
    return OMS_FAILED;
  }

  ret = readn(fd, buffer + sizeof(packet_header), packet_length);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read mysql hand shake packet. length=" << packet_length << ", error=" << strerror(errno);
    free(buffer);
    return ret;
  }

  memcpy(buffer, &packet_header, sizeof(packet_header));
  message_buffer.push_back(buffer, packet_length + sizeof(packet_header));
  return OMS_OK;
}
int MysqlInitialHandShakePacket::decode(const MessageBuffer& message_buffer)
{
  // reference:
  // https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::Handshake

  int ret = OMS_OK;

  MessageBufferReader buffer_reader(message_buffer);
  ret = buffer_reader.forward(4);  // packet length
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip the packet length while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // read protocol version
  ret = buffer_reader.read_uint8(_protocol_version);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read protocol version while decoding mysql InitialHandShakePacket";
    return ret;
  }

  if (_protocol_version != 0x0a) {
    OMS_ERROR << "Unsupported packet version. version=" << _protocol_version;
  }

  // skip the string version
  char c = 0xFF;
  do {
    ret = buffer_reader.read(&c, 1);
  } while (ret == OMS_OK && c != 0);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read buffer while decoding mysql InitialHandShakePacket. buffer read "
              << buffer_reader.read_size();
  }

  int32_t connection_id;
  ret = buffer_reader.read_int<int32_t, Endian::LITTLE>(connection_id);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read connection id while decoding mysql InitialHandShakePacket";
    return ret;
  }

  _scramble_buffer_valid = false;
  _scramble_buffer.reserve(20);

  // scramble part 1.
  // string[8]      auth-plugin-data-part-1
  _scramble_buffer.resize(8);
  ret = buffer_reader.read(_scramble_buffer.data(), 8);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read auth-plugin-data-part-1 while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // [00] filler
  ret = buffer_reader.forward(1);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip 'filler' while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // capability flags (lower 2 bytes)
  ret = buffer_reader.read((char*)&_capabilities_flag, 2);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read auth-plugin-data-part-1 while decoding mysql InitialHandShakePacket";
    return ret;
  }

  if (!buffer_reader.has_more()) {
    OMS_DEBUG << "Decode done. field end with capabilities flag lower byte_size";
    _scramble_buffer_valid = true;
    return OMS_OK;
  }

  // skip the 'character set' and 'status flags'
  ret = buffer_reader.forward(1 /*character set */ + 2 /*status flags*/);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip the 'character set' and 'status flags' while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // capability flags (upper 2 bytes)
  ret = buffer_reader.read((char*)&_capabilities_flag + 2, 2);
  if (ret != OMS_OK) {
    OMS_ERROR
        << "Failed to read the 'capability flags (upper 2 byte_size)' while decoding mysql InitialHandShakePacket";
    return ret;
  }

  _capabilities_flag = le_to_cpu<uint32_t>(_capabilities_flag);

  // if capabilities & CLIENT_PLUGIN_AUTH then length of auth-plugin-data
  uint8_t auth_plugin_data_len = 0;
  ret = buffer_reader.read_uint8(auth_plugin_data_len);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read the 'length of auth-plugin-data' while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // reserved (all [00])
  ret = buffer_reader.forward(10);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip the 'reserved (all [00])' while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // if capabilities & CLIENT_SECURE_CONNECTION
  // then auth-plugin-data-part-2 ($len=MAX(13, length of auth-plugin-data - 8))
  if (_capabilities_flag & CLIENT_SECURE_CONNECTION) {
    const int auth_plugin_data_part_2_len = std::max(13, auth_plugin_data_len - 8);
    _scramble_buffer.resize(8 + auth_plugin_data_part_2_len - 1);
    ret = buffer_reader.read(_scramble_buffer.data() + 8, auth_plugin_data_part_2_len - 1);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to read the part 2 of 'scramble buffer' while decoding mysql InitialHandShakePacket";
      return ret;
    }

    _scramble_buffer_valid = true;
  }

  return ret;
}

bool MysqlInitialHandShakePacket::scramble_buffer_valid() const
{
  return _scramble_buffer_valid;
}

const std::vector<char>& MysqlInitialHandShakePacket::scramble_buffer() const
{
  return _scramble_buffer;
}

uint8_t MysqlInitialHandShakePacket::sequence() const
{
  return _sequence;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 在buffer中写入一个\0结尾的字符串
 * @return 返回非负数，表示写入的数据大小，否则失败
 */
int write_null_terminate_string(char* buf, int capacity, const char* s)
{
  int str_len = strlen(s);
  if (str_len + 1 > capacity) {
    return OMS_FAILED;
  }
  memcpy(buf, s, str_len + 1);
  return str_len + 1;
}

int write_string(char* buf, int capacity, const char* s, int str_len)
{
  if (str_len > capacity) {
    return OMS_FAILED;
  }
  memcpy(buf, s, str_len);
  return str_len;
}

int write_len_enc_integer(char* buf, int capacity, uint64_t integer)
{
  // https://dev.mysql.com/doc/internals/en/integer.html#packet-Protocol::LengthEncodedInteger

  if (integer < 251) {
    if (capacity >= 1) {
      buf[0] = (uint8_t)integer;
      return 1;
    } else {
      return OMS_FAILED;
    }
  } else if (integer < (1 << 16UL)) {
    if (capacity >= 3) {
      buf[0] = 0xFC;
      uint16_t num = cpu_to_le<uint16_t>((uint16_t)integer);
      memcpy(buf + 1, &num, sizeof(num));
      return 3;
    } else {
      return OMS_FAILED;
    }
  } else if (integer < (1 << 24UL)) {
    if (capacity >= 4) {
      buf[0] = 0xFD;
      uint32_t num = cpu_to_le<uint32_t>((uint32_t)integer);
      memcpy(buf + 1, (char*)&num + 1, 3);
      return 4;
    } else {
      return OMS_FAILED;
    }
  } else if (integer < ULLONG_MAX) {
    if (capacity >= 9) {
      buf[0] = 0xFE;
      uint64_t num = cpu_to_le<uint64_t>((uint64_t)integer);
      memcpy(buf + 1, &num, sizeof(num));
      return 9;
    } else {
      return OMS_FAILED;
    }
  } else if (integer == ULLONG_MAX) {
    if (capacity >= 1) {
      buf[0] = 0xFB;
      return 1;
    } else {
      return OMS_FAILED;
    }
  }
  return OMS_FAILED;
}

MysqlHandShakeResponsePacket::MysqlHandShakeResponsePacket(
    const std::string& username, const std::string& database, const std::vector<char>& auth_response, int8_t sequence)
    : _username(username), _database(database), _auth_response(auth_response), _sequence(sequence)
{}

uint32_t MysqlHandShakeResponsePacket::calc_capabilities_flag()
{
  uint32_t capabilities_flag = CLIENT_MYSQL | CLIENT_LONG_FLAG | CLIENT_LOCAL_FILES |
                               CLIENT_PROTOCOL_41
                               // | CLIENT_INTERACTIVE
                               | CLIENT_TRANSACTIONS | CLIENT_SECURE_CONNECTION | CLIENT_MULTI_STATEMENTS |
                               CLIENT_MULTI_RESULTS | CLIENT_PS_MULTI_RESULTS |
                               CLIENT_PLUGIN_AUTH
                               // | CLIENT_CONNECT_ATTRS
                               | CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA | CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS |
                               CLIENT_SUPPORT_ORACLE_MODE;  // MySQL 鉴权加上这个flag不影响

  if (!_database.empty()) {
    capabilities_flag |= CLIENT_CONNECT_WITH_DB;
  }
  return capabilities_flag;
}
int MysqlHandShakeResponsePacket::encode(MessageBuffer& message_buffer)
{
  // reference:
  // https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::HandshakeResponse
  // Protocol::HandshakeResponse41

  const int capacity = 200;
  char* buffer = (char*)malloc(capacity);
  if (nullptr == buffer) {
    OMS_ERROR << "Failed to alloc memory. count=" << capacity;
    return OMS_FAILED;
  }

  int offset = 4;  // 3 bytes: packet size, 1 byte: sequence

  // capability flags, CLIENT_PROTOCOL_41 always set
  uint32_t src_capabilities_flag = calc_capabilities_flag();
  uint32_t dst_capabilities_flag = cpu_to_le<uint32_t>(src_capabilities_flag);
  memcpy(buffer + offset, &dst_capabilities_flag, sizeof(dst_capabilities_flag));
  offset += sizeof(dst_capabilities_flag);

  // max-packet size
  // max size of a command packet that the client wants to send to the server.
  int32_t max_packet_size = 16 * 1024 * 1024;
  max_packet_size = cpu_to_le<int32_t>(max_packet_size);
  memcpy(buffer + offset, &max_packet_size, sizeof(max_packet_size));
  offset += sizeof(max_packet_size);

  // character set
  const int8_t character_set = 33;  // // utf8 COLLATE utf8_general_ci
  buffer[offset] = character_set;
  offset += 1;

  // string[23] reserved (all [0])
  offset += 23;

  // string[NUL]    username
  int ret = write_null_terminate_string(buffer + offset, capacity - offset, _username.c_str());
  if (ret < 0) {
    OMS_ERROR << "Failed to encode user name";
    free(buffer);
    buffer = nullptr;
    return OMS_FAILED;
  }
  offset += ret;

  // if capabilities & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
  //   then lenenc-int     length of auth-response
  //    and string[n]      auth-response
  // https://dev.mysql.com/doc/internals/en/string.html#packet-Protocol::LengthEncodedString
  if (src_capabilities_flag & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
    ret = write_len_enc_integer(buffer + offset, capacity - offset, _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response length. length=" << _auth_response.size() << ", capacity last "
                << capacity - offset;
      free(buffer);
      buffer = nullptr;
      return OMS_FAILED;
    }

    offset += ret;

    ret = write_string(buffer + offset, capacity - offset, _auth_response.data(), _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response data. length=" << _auth_response.size() << ", capacity last %d"
                << capacity - offset;
      free(buffer);
      buffer = nullptr;
      return OMS_FAILED;
    }
    offset += ret;
  }
  // else if capabilities & CLIENT_SECURE_CONNECTION
  //   then length of auth-response
  //   and auth-response
  else if (src_capabilities_flag & CLIENT_SECURE_CONNECTION) {
    buffer[offset] = (int8_t)_auth_response.size();
    ret = write_string(buffer + offset, capacity - offset, _auth_response.data(), _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response data. length=" << _auth_response.size() << ", capacity last "
                << capacity - offset;
      free(buffer);
      buffer = nullptr;
      return OMS_FAILED;
    }
    offset += ret;
  }
  // else
  //  then auth-response
  else {
    ret = write_null_terminate_string(buffer + offset, capacity - offset, _auth_response.data());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response data. length=" << _auth_response.size() + 1 << ", capacity last "
                << capacity - offset;
      free(buffer);
      buffer = nullptr;
      return OMS_FAILED;
    }
    offset += ret;
  }

  // if capabilities & CLIENT_CONNECT_WITH_DB
  //   then string[NUL]    database
  if (!_database.empty() && src_capabilities_flag & CLIENT_CONNECT_WITH_DB) {
    ret = write_null_terminate_string(buffer + offset, capacity - offset, _database.c_str());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode database. length=" << _database.size() + 1 << ", capacity last "
                << capacity - offset;
      free(buffer);
      buffer = nullptr;
      return OMS_FAILED;
    }
    offset += ret;
  }

  // if capabilities & CLIENT_PLUGIN_AUTH
  //   then string[NUL]    auth plugin name
  const char* plugin_auth = "mysql_native_password";
  if (src_capabilities_flag & CLIENT_PLUGIN_AUTH) {
    ret = write_null_terminate_string(buffer + offset, capacity - offset, plugin_auth);
    if (ret < 0) {
      OMS_ERROR << "Failed to encode database. length=" << strlen(plugin_auth) + 1 << ", capacity last "
                << capacity - offset;
      free(buffer);
      buffer = nullptr;
      return OMS_FAILED;
    }
    offset += ret;
  }

  // if capabilities & CLIENT_CONNECT_ATTRS
  //  lenenc-int     length of all key-values
  //  and lenenc-str     key
  //  and lenenc-str     value
  // do nothing now
  // end

  uint32_t packet_size = cpu_to_le<uint32_t>((uint32_t)offset - 4);
  memcpy(buffer, (char*)&packet_size, 3);
  buffer[3] = _sequence;
  OMS_DEBUG << "mysql hand shake response packet count=" << offset;

  message_buffer.push_back(buffer, offset);
  return OMS_OK;
}

bool MysqlOkPacket::result_ok(const MessageBuffer& message_buffer) const
{
  // https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html

  MessageBufferReader buffer_reader(message_buffer);
  int ret = buffer_reader.forward(4);  // length
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to decode_payload mysql ok packet. step: skip 4 byte_size";
    return false;
  }

  int buffer_bytes = buffer_reader.byte_size();
  uint8_t packet_type;
  ret = buffer_reader.read_uint8(packet_type);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read packet type.";
    return false;
  }

  if (packet_type == 0x00) {  // OK packet
    // https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
    return buffer_bytes > 7;
  }

  if (packet_type == 0xFE) {  // EOF packet
    // https://dev.mysql.com/doc/internals/en/packet-EOF_Packet.html
    return buffer_bytes < 9;
  }

  if (packet_type == 0xFF) {  // ERR packet
    // https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
    uint16_t error_code = 0;
    ret = buffer_reader.read_int<uint16_t>(error_code);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to read error code";
      return false;
    }

    // 5 bytes SQL state
    ret = buffer_reader.forward(5);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to skip SQL state";
      return false;
    }

    // skip SQL State Marker '#'
    ret = buffer_reader.forward(1);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to skip # state marker";
      return false;
    }

    int last_bytes = buffer_bytes - buffer_reader.read_size();
    std::string message;
    message.resize(last_bytes);
    ret = buffer_reader.read((char*)message.data(), last_bytes);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to read error message";
      return false;
    }

    OMS_DEBUG << "mysql ok packet error code=" << error_code << ", error message=" << message;
  }
  return false;
}
}  // namespace logproxy
}  // namespace oceanbase
