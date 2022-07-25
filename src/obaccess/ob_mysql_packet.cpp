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
#include "codec/msg_buf.h"
#include "communication/io.h"
#include "common/log.h"
#include "common/common.h"
#include "common/guard.hpp"

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

int recv_mysql_packet(int fd, int timeout, uint32_t& packet_length, uint8_t& sequence, MsgBuf& msgbuf)
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
    OMS_ERROR << "Failed to read packet length, errno: " << errno << ", error: " << strerror(errno);
    return ret;
  }
  sequence = (packet_header & 0xFF000000) >> 3;
  packet_length = le_to_cpu(packet_header & 0x00FFFFFF);
  if (packet_length >= UINT32_MAX - sizeof(packet_header)) {
    OMS_ERROR << "Got invalid packet length, too length: " << packet_length;
    return OMS_FAILED;
  }

  char* buffer = (char*)malloc(packet_length);
  if (nullptr == buffer) {
    OMS_ERROR << "Failed to malloc memory for mysql handshake packet length: " << packet_length;
    return OMS_FAILED;
  }

  ret = readn(fd, buffer, packet_length);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read mysql handshake packet. length: " << packet_length << ", error: " << strerror(errno);
    free(buffer);
    return ret;
  }

  msgbuf.push_back(buffer, packet_length);
  return OMS_OK;
}

int recv_mysql_packet(int fd, int timeout, MsgBuf& msgbuf)
{
  uint32_t packet_length = 0;
  uint8_t sequence = 0;
  msgbuf.reset();
  return recv_mysql_packet(fd, timeout, packet_length, sequence, msgbuf);
}

int send_mysql_packet(int fd, MsgBuf& msgbuf, uint8_t sequence)
{
  // [24bit] packet_size
  // [8bit] sequence
  uint32_t packet_length = msgbuf.byte_size();
  packet_length = cpu_to_le(packet_length & 0x00FFFFFF);
  packet_length = packet_length | (sequence << 3);

  ////// DEBUG ONLY ///////
  std::string hexstr;
  dumphex((char*)&packet_length, 4, hexstr);
  OMS_DEBUG << "MySQL packet header: " << hexstr << ", value: " << packet_length;
  ////// DEBUG ONLY ///////

  int ret = writen(fd, &packet_length, 4);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send packet, error:" << strerror(errno);
    return ret;
  }
  for (const auto& iter : msgbuf) {
    ret = writen(fd, iter.buffer(), iter.size());
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to send packet, errno:" << errno << ", error:" << strerror(errno);
      return ret;
    }
  }
  return OMS_OK;
}

// https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
int MySQLOkPacket::decode(const MsgBuf& msgbuf)
{
  MsgBufReader reader(msgbuf);
  uint8_t packet_type;
  int ret = reader.read_uint8(packet_type);
  return (ret == OMS_OK && packet_type == 0x00) ? OMS_OK : OMS_FAILED;
}

const uint8_t MySQLEofPacket::_s_packet_type = 0xfe;

int MySQLEofPacket::decode(const MsgBuf& msgbuf)
{
  size_t len = msgbuf.byte_size();
  if (len >= 9) {
    OMS_ERROR << "Not an EOF packet as expected, length(" << len << ") >= 9";
    return OMS_FAILED;
  }

  MsgBufReader reader(msgbuf);
  uint8_t code = 0;
  reader.read_uint8(code);
  if (code != _s_packet_type) {
    OMS_ERROR << "Not an EOF packet as expected";
    return OMS_FAILED;
  }
  reader.read_uint16(_warnings_count);
  reader.read_uint16(_status_flags);
  return OMS_OK;
}

/**
 * https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
 */
int MySQLErrorPacket::decode(const MsgBuf& msgbuf)
{
  MsgBufReader reader(msgbuf);

  int ret = reader.read_int<uint16_t>(_code);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read error code";
    return OMS_FAILED;
  }

  // skip SQL State Marker '#'
  _sql_state_marker.resize(1);
  ret = reader.read((char*)_sql_state_marker.data(), 1);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip # state marker";
    return OMS_FAILED;
  }

  // 5 bytes SQL state
  _sql_state.resize(6);
  ret = reader.read((char*)_sql_state.data(), 6);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip SQL state";
    return OMS_FAILED;
  }

  size_t remain_size = reader.remain_size();
  _message.resize(remain_size);
  ret = reader.read((char*)_message.data(), remain_size);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read error message";
    return OMS_FAILED;
  }

  OMS_DEBUG << "Error packet: [" << _code << "][" << _sql_state_marker << _sql_state << "] " << _message;
  return OMS_OK;
}

// reference:
// https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::Handshake
int MySQLInitialHandShakePacket::decode(const MsgBuf& msgbuf)
{

  MsgBufReader buffer_reader(msgbuf);

  // read protocol version
  int ret = buffer_reader.read_uint8(_protocol_version);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read protocol version while decoding mysql InitialHandShakePacket";
    return ret;
  }
  if (_protocol_version != 0x0a) {
    OMS_ERROR << "Unsupported packet version: " << _protocol_version;
    return OMS_FAILED;
  }

  // server version
  std::string server_version;
  while (ret == OMS_OK && buffer_reader.has_more()) {
    char c = 0;
    ret = buffer_reader.read(&c, 1);
    if (c == '\0') {
      break;
    }
    server_version.append(1, c);
  }
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read buffer while decoding mysql InitialHandShakePacket. buffer read "
              << buffer_reader.read_size();
  }
  OMS_DEBUG << "Observer version: " << server_version;

  int32_t connection_id;
  ret = buffer_reader.read_int<int32_t>(connection_id);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to read connection id while decoding mysql InitialHandShakePacket";
    return ret;
  }
  OMS_DEBUG << "Connection id: " << connection_id;

  _scramble.reserve(20);

  // scramble part 1.
  // string[8]      auth-plugin-data-part-1
  _scramble.resize(8);
  ret = buffer_reader.read(_scramble.data(), 8);
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

  _scramble_valid = false;
  if (!buffer_reader.has_more()) {
    OMS_DEBUG << "Decode done. field end with capabilities flag lower byte_size";
    _scramble_valid = true;
    return OMS_OK;
  }

  // skip the 'character set' and 'status flags'
  ret = buffer_reader.forward(1 /*character set */ + 2 /*status flags*/);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to skip the 'character set' and 'status flags' while decoding mysql InitialHandShakePacket";
    return ret;
  }

  // capability flags (upper 2 bytes)
  ret = buffer_reader.read(((char*)&_capabilities_flag) + 2, 2);
  if (ret != OMS_OK) {
    OMS_ERROR
        << "Failed to read the 'capability flags (upper 2 byte_size)' while decoding mysql InitialHandShakePacket";
    return ret;
  }
  _capabilities_flag = le_to_cpu(_capabilities_flag);

  // if capabilities & CLIENT_PLUGIN_AUTH then length of auth-plugin-data
  uint8_t auth_plugin_data_len = 0;
  if (_capabilities_flag & CLIENT_PLUGIN_AUTH) {
    ret = buffer_reader.read_uint8(auth_plugin_data_len);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to read the 'length of auth-plugin-data' while decoding mysql InitialHandShakePacket";
      return ret;
    }
  } else {
    buffer_reader.forward(1);
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
    _scramble.resize(8 + auth_plugin_data_part_2_len);
    ret = buffer_reader.read(_scramble.data() + 8, auth_plugin_data_part_2_len);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to read the part 2 of 'scramble buffer' while decoding mysql InitialHandShakePacket";
      return ret;
    }
    _scramble_valid = true;
  }
  if (_capabilities_flag & CLIENT_PLUGIN_AUTH) {
    _auth_plugin_name.reserve(buffer_reader.remain_size());
    while (ret == OMS_OK && buffer_reader.has_more()) {
      char c = 0;
      ret = buffer_reader.read(&c, 1);
      if (c == '\0') {
        break;
      }
      _auth_plugin_name.append(1, c);
    }
    OMS_DEBUG << "auth plugin name: " << _auth_plugin_name;
  }

  if (_auth_plugin_name != "mysql_native_password") {
    OMS_ERROR << "Unsupport auth plugin name: " << _auth_plugin_name;
    return OMS_FAILED;
  }
  // fix ob response length 21 scramble
  if (_scramble.size() > 20) {
    _scramble.resize(20);
  }
  return ret;
}

bool MySQLInitialHandShakePacket::scramble_valid() const
{
  return _scramble_valid;
}

const std::vector<char>& MySQLInitialHandShakePacket::scramble() const
{
  return _scramble;
}

uint8_t MySQLInitialHandShakePacket::sequence() const
{
  return _sequence;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 在buffer中写入一个\0结尾的字符串
 * @return 返回非负数，表示写入的数据大小，否则失败
 */

static inline int write_null_terminate_string(char* buf, size_t capacity, const char* s, size_t len)
{
  if (len + 1 > capacity) {
    return OMS_FAILED;
  }
  memcpy(buf, s, len + 1);
  buf[len] = '\0';
  return len + 1;
}

static inline int write_null_terminate_string(char* buf, size_t capacity, const std::string& str)
{
  return write_null_terminate_string(buf, capacity, str.data(), str.size());
}

static inline int write_string(char* buf, size_t capacity, const char* s, size_t str_len)
{
  if (str_len > capacity) {
    return OMS_FAILED;
  }
  memcpy(buf, s, str_len);
  return str_len;
}

static inline int write_lenenc_uint(char* buf, size_t capacity, uint64_t integer)
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
      uint16_t num = cpu_to_le((uint16_t)integer);
      memcpy(buf + 1, &num, sizeof(num));
      return 3;
    } else {
      return OMS_FAILED;
    }
  } else if (integer < (1 << 24UL)) {
    if (capacity >= 4) {
      buf[0] = 0xFD;
      uint32_t num = cpu_to_le((uint32_t)integer);
      memcpy(buf + 1, (char*)&num + 1, 3);
      return 4;
    } else {
      return OMS_FAILED;
    }
  } else if (integer < ULLONG_MAX) {
    if (capacity >= 9) {
      buf[0] = 0xFE;
      uint64_t num = cpu_to_le((uint64_t)integer);
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

MySQLHandShakeResponsePacket::MySQLHandShakeResponsePacket(
    const std::string& username, const std::string& database, const std::vector<char>& auth_response)
    : _username(username), _database(database), _auth_response(auth_response)
{}

int MySQLHandShakeResponsePacket::encode(MsgBuf& msgbuf)
{
  // reference:
  // https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::HandshakeResponse
  // Protocol::HandshakeResponse41

  const int capacity = 200;
  char* buffer = (char*)malloc(capacity);
  if (nullptr == buffer) {
    OMS_ERROR << "Failed to alloc memory size: " << capacity;
    return OMS_FAILED;
  }
  FreeGuard<char*> fg(buffer);

  // capability flags, CLIENT_PROTOCOL_41 always set
  uint32_t capabilities_flag = CLIENT_MYSQL | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS |
                               CLIENT_SECURE_CONNECTION | CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS |
                               CLIENT_PS_MULTI_RESULTS | CLIENT_PLUGIN_AUTH | CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
                               CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS |
                               CLIENT_SUPPORT_ORACLE_MODE;  // MySQL 鉴权加上这个flag不影响
  if (!_database.empty()) {
    capabilities_flag |= CLIENT_CONNECT_WITH_DB;
  }

  uint32_t offset = 0;
  uint32_t dst_capabilities_flag = cpu_to_le(capabilities_flag);
  memcpy(buffer + offset, &dst_capabilities_flag, 4);
  offset += 4;

  // max-packet size
  // max size of a command packet that the client wants to send to the server.
  int32_t max_packet_size = cpu_to_le(16 * 1024 * 1024);
  memcpy(buffer + offset, &max_packet_size, 4);
  offset += 4;

  // character set utf8 COLLATE utf8_general_ci
  buffer[offset] = 33;
  offset += 1;

  // string[23] reserved (all [0])
  offset += 23;

  // string[NUL]    username
  int ret = write_null_terminate_string(buffer + offset, capacity - offset, _username);
  if (ret < 0) {
    OMS_ERROR << "Failed to encode user name";
    return OMS_FAILED;
  }
  offset += ret;

  // if capabilities & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
  //   then lenenc-int     length of auth-response
  //    and string[n]      auth-response
  // https://dev.mysql.com/doc/internals/en/string.html#packet-Protocol::LengthEncodedString
  if (capabilities_flag & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
    ret = write_lenenc_uint(buffer + offset, capacity - offset, _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response length: " << _auth_response.size() << ", capacity last "
                << capacity - offset;
      return OMS_FAILED;
    }
    offset += ret;

    ret = write_string(buffer + offset, capacity - offset, _auth_response.data(), _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response data length: " << _auth_response.size() << ", capacity last %d"
                << capacity - offset;
      return OMS_FAILED;
    }
    offset += ret;
  }
  // else if capabilities & CLIENT_SECURE_CONNECTION
  //   then length of auth-response
  //   and auth-response
  else if (capabilities_flag & CLIENT_SECURE_CONNECTION) {
    buffer[offset] = (int8_t)_auth_response.size();
    ret = write_string(buffer + offset, capacity - offset, _auth_response.data(), _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response data length: " << _auth_response.size() << ", capacity last "
                << capacity - offset;
      return OMS_FAILED;
    }
    offset += ret;
  } else {
    //  then auth-response with null terminate string
    ret = write_null_terminate_string(buffer + offset, capacity - offset, _auth_response.data(), _auth_response.size());
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth response data length: " << _auth_response.size() + 1 << ", capacity last "
                << capacity - offset;
      return OMS_FAILED;
    }
    offset += ret;
  }

  // if capabilities & CLIENT_CONNECT_WITH_DB
  //   then string[NUL]    database
  if (capabilities_flag & CLIENT_CONNECT_WITH_DB) {
    ret = write_null_terminate_string(buffer + offset, capacity - offset, _database);
    if (ret < 0) {
      OMS_ERROR << "Failed to encode database length: " << _database.size() + 1 << ", capacity last "
                << capacity - offset;
      return OMS_FAILED;
    }
    offset += ret;
  }

  // if capabilities & CLIENT_PLUGIN_AUTH
  //   then string[NUL]    auth plugin name
  if (capabilities_flag & CLIENT_PLUGIN_AUTH) {
    const std::string auth_plugin = "mysql_native_password";
    ret = write_null_terminate_string(buffer + offset, capacity - offset, auth_plugin);
    if (ret < 0) {
      OMS_ERROR << "Failed to encode auth plugin: " << auth_plugin << ", capacity last " << capacity - offset;
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

  OMS_DEBUG << "Handshake response packet len: " << offset;

  fg.release();
  msgbuf.push_back(buffer, offset);
  return OMS_OK;
}

int MySQLCol::decode(const MsgBuf& msgbuf)
{
  MysqlBufReader reader(msgbuf);
  reader.read_lenenc_str(_catalog);
  reader.read_lenenc_str(_schema);
  reader.read_lenenc_str(_table);
  reader.read_lenenc_str(_org_table);
  reader.read_lenenc_str(_name);
  reader.read_lenenc_str(_org_name);
  reader.read_uint8(_len_of_fixed_len);  // always 0x0c
  reader.read_uint16(_charset);
  reader.read_uint32(_column_len);
  reader.read_uint8(_type);
  reader.read_uint16(_flags);
  reader.read_uint8(_decimals);
  reader.read_uint16(_filler);
  return OMS_OK;
}

MySQLRow::MySQLRow(uint64_t col_count) : _col_count(col_count)
{}

int logproxy::MySQLRow::decode(const MsgBuf& msgbuf)
{
  MysqlBufReader reader(msgbuf);
  for (uint64_t i = 0; i < _col_count; ++i) {
    std::string data;
    reader.read_lenenc_str(data);
    _fields.emplace_back(data);
  }
  return OMS_OK;
}

MySQLQueryPacket::MySQLQueryPacket(const std::string& sql) : _sql(sql)
{}

/**
 * https://dev.mysql.com/doc/internals/en/com-query.html
 */
int MySQLQueryPacket::encode_inplace(MsgBuf& msgbuf)
{
  msgbuf.push_back((char*)&_cmd_id, 1, false);
  msgbuf.push_back((char*)_sql.c_str(), _sql.size(), false);
  return OMS_OK;
}

/**
 * https://dev.mysql.com/doc/internals/en/com-query-response.html
 */
int MySQLQueryResponsePacket::decode(const MsgBuf& msgbuf)
{
  MysqlBufReader reader(msgbuf);
  uint8_t packet_ret = 0xff;
  reader.read_uint8(packet_ret);
  if (packet_ret == 0xff) {
    _err.decode(msgbuf);
    return OMS_FAILED;
  } else if (packet_ret == 0xfb) {
    // LOCAL INFILE
    return OMS_FAILED;
  } else if (packet_ret == 0x00) {
    // OK Packet
    return OMS_OK;
  }

  reader.backward(1);
  reader.read_lenenc_uint(_col_count);
  return OMS_OK;
}

void MySQLResultSet::reset()
{
  col_count = 0;
  cols.clear();
  rows.clear();
}

}  // namespace logproxy
}  // namespace oceanbase
