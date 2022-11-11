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

#include <string.h>
#include <vector>

#include "obaccess/mysql_protocol.h"
#include "obaccess/ob_mysql_packet.h"
#include "obaccess/ob_sha1.h"
#include "common/log.h"
#include "common/common.h"
#include "communication/io.h"
#include "codec/codec_endian.h"
#include "codec/msg_buf.h"

namespace oceanbase {
namespace logproxy {

MysqlProtocol::~MysqlProtocol()
{
  close();
}

void MysqlProtocol::close()
{
  if (_sockfd >= 0) {
    ::close(_sockfd);
    _sockfd = -1;
  }
  _hostname = "";
  _port = -1;
  _username = "";
  _passwd_sha1 = "";
}

int MysqlProtocol::connect_to_server()
{
  int ret = connect(_hostname.c_str(), _port, false, _detect_timeout, _sockfd);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to connect to server: " << _hostname << ':' << _port << ", user: " << _username;
  } else {
    OMS_INFO << "Connect to server success: " << _hostname << ':' << _port << ", user: " << _username;
  }
  return ret;
}

int MysqlProtocol::login(const std::string& host, int port, const std::string& username, const std::string& passwd_sha1,
    const std::string& database)
{
  _hostname = host;
  _port = port;
  _username = username;
  _passwd_sha1 = passwd_sha1;

  // https://dev.mysql.com/doc/internals/en/secure-password-authentication.html
  // 1 connect to the server
  int ret = connect_to_server();
  if (ret < 0) {
    OMS_ERROR << "Failed to connect to server " << _hostname << ':' << _port;
    return OMS_CONNECT_FAILED;
  }

  // 2 receive initial handshake
  MsgBuf msgbuf;
  uint8_t sequence = 0;
  uint32_t packet_length = 0;
  ret = recv_mysql_packet(_sockfd, _detect_timeout, packet_length, sequence, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to receive handshake packet from: " << _hostname << ':' << _port
              << ", error: " << strerror(errno);
    return ret;
  }
  OMS_DEBUG << "Receive handshake packet from server: " << _hostname << ':' << _port << ", user: " << _username;

  MySQLInitialHandShakePacket handshake_packet;
  ret = handshake_packet.decode(msgbuf);
  if (ret != OMS_OK || !handshake_packet.scramble_valid()) {
    OMS_ERROR << "Failed to decode_payload initial handshake packet or does not has a valid scramble:"
              << handshake_packet.scramble_valid() << ". length=" << packet_length << ", server: " << _hostname << ':'
              << _port;
    return ret;
  }

  const std::vector<char>& scramble = handshake_packet.scramble();

  // 3 calculate the password combined with scramble buffer -> auth information
  std::vector<char> auth;
  ret = calc_mysql_auth_info(scramble, auth);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to calc login info from: " << _hostname << ':' << _port << ", user: " << _username;
    return ret;
  }

  // 4 send the handshake response with auth information
  ret = send_auth(auth, database);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send handshake response message to " << _hostname << ':' << _port << ", user=" << _username;
    return OMS_FAILED;
  }

  // 5 receive response from server
  msgbuf.reset();
  ret = recv_mysql_packet(_sockfd, _detect_timeout, packet_length, sequence, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to recv handshake auth response from: " << _hostname << ':' << _port
              << ", user: " << _username;
    return OMS_FAILED;
  }

  MySQLOkPacket ok_packet;
  ret = ok_packet.decode(msgbuf);
  if (ret == OMS_OK) {
    OMS_INFO << "Auth user success of server: " << _hostname << ':' << _port << ", user: " << _username;
  } else {
    MySQLErrorPacket error_packet;
    error_packet.decode(msgbuf);
    OMS_ERROR << "Auth user failed of server: " << _hostname << ':' << _port << ", user: " << _username;
  }
  return ret;
}

static inline void my_xor(const unsigned char* s1, const unsigned char* s2, uint32_t len, unsigned char* to)
{
  const unsigned char* s1_end = s1 + len;
  while (s1 < s1_end) {
    *to++ = *s1++ ^ *s2++;
  }
}

int MysqlProtocol::calc_mysql_auth_info(const std::vector<char>& scramble, std::vector<char>& auth)
{
  // SHA1( password ) XOR SHA1( "20-bytes random data from server" <concat> SHA1( SHA1( password ) ) )
  // SHA1(password) -> stage1
  // SHA1(SHA1(password)) -> stage2
  SHA1 sha1;
  SHA1::ResultCode result_code = sha1.input((const unsigned char*)_passwd_sha1.data(), _passwd_sha1.size());
  if (result_code != SHA1::SHA_SUCCESS) {
    OMS_ERROR << "Failed to calc sha1(step input) of passwd stage1 to stage2. result code=" << result_code;
    return OMS_FAILED;
  }

  std::vector<char> passwd_stage2;
  passwd_stage2.resize(SHA1::SHA1_HASH_SIZE);
  result_code = sha1.get_result((unsigned char*)passwd_stage2.data());
  if (result_code != SHA1::SHA_SUCCESS) {
    OMS_ERROR << "Failed to calc sha1(step get_result) of passwd stage1 to stage2. result code=" << result_code;
    return OMS_FAILED;
  }

  std::vector<char> scramble_combined;
  scramble_combined.reserve(scramble.size() + passwd_stage2.size());
  scramble_combined.assign(scramble.begin(), scramble.end());
  scramble_combined.insert(scramble_combined.end(), passwd_stage2.begin(), passwd_stage2.end());

  sha1.reset();
  result_code = sha1.input((const unsigned char*)scramble_combined.data(), scramble_combined.size());
  if (result_code != SHA1::SHA_SUCCESS) {
    OMS_ERROR << "Failed to calc sha1(step input) of combined. result code=" << result_code;
    return OMS_FAILED;
  }

  std::vector<char> sha_combined;
  sha_combined.resize(SHA1::SHA1_HASH_SIZE);
  result_code = sha1.get_result((unsigned char*)sha_combined.data());
  if (result_code != SHA1::SHA_SUCCESS) {
    OMS_ERROR << "Failed to calc sha1(step get_result) of combined. result code=" << result_code;
    return OMS_FAILED;
  }

  if (_passwd_sha1.size() != sha_combined.size()) {
    OMS_ERROR << "the length of password stage1(sha1)(" << _passwd_sha1.size() << ") != the length of sha_combined("
              << sha_combined.size() << ")";
    return OMS_FAILED;
  }

  auth.resize(sha_combined.size());
  my_xor((const unsigned char*)_passwd_sha1.data(),
      (const unsigned char*)sha_combined.data(),
      auth.size(),
      (unsigned char*)auth.data());
  return OMS_OK;
}

int MysqlProtocol::send_auth(const std::vector<char>& auth_info, const std::string& database)
{
  MySQLHandShakeResponsePacket handshake_response_packet(_username, database, auth_info);
  MsgBuf msgbuf;
  int ret = handshake_response_packet.encode(msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode handshake response packet";
    return -1;
  }

  ret = send_mysql_packet(_sockfd, msgbuf, 0);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send handshake response to server: " << _hostname << ":" << _port;
    return ret;
  }
  return OMS_OK;
}

int MysqlProtocol::query(const std::string& sql, MySQLResultSet& rs)
{

  OMS_INFO << "Query obmysql SQL:" << sql;
  rs.reset();

  MsgBuf msgbuf;
  MySQLQueryPacket packet(sql);
  int ret = packet.encode_inplace(msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode observer sql packet, ret:" << ret;
    return ret;
  }
  ret = send_mysql_packet(_sockfd, msgbuf, 0);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send query packet to server:" << _hostname << ':' << _port;
    return OMS_CONNECT_FAILED;
  }

  ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to recv query packet from server:" << _hostname << ':' << _port;
    return OMS_CONNECT_FAILED;
  }

  MySQLQueryResponsePacket query_resp;
  ret = query_resp.decode(msgbuf);
  if (ret != OMS_OK || query_resp.col_count() == 0) {
    OMS_ERROR << "Failed to query observer:" << query_resp._err._message
              << (query_resp.col_count() == 0 ? ", unexpected column count: 0" : "");
    rs.code = query_resp._err._code;
    rs.message = query_resp._err._message;
    return OMS_FAILED;
  }

  // column definitions
  rs.col_count = query_resp.col_count();
  rs.cols.resize(query_resp.col_count());
  for (uint64_t i = 0; i < query_resp.col_count(); ++i) {
    ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to recv column defines packet from server:" << _hostname << ':' << _port;
      return OMS_CONNECT_FAILED;
    }

    MySQLCol column;
    column.decode(msgbuf);
    rs.cols[i] = column;
  }

  // eof
  ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to recv eof packet from server:" << _hostname << ':' << _port;
    return OMS_CONNECT_FAILED;
  }
  MySQLEofPacket eof;
  ret = eof.decode(msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to decode eof packet from server:" << _hostname << ':' << _port;
    return OMS_OK;
  }

  // rows
  while (true) {
    ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
    if (ret != OMS_OK || msgbuf.begin()->buffer() == nullptr) {
      OMS_ERROR << "Failed to recv row packet from server:" << _hostname << ':' << _port;
      return OMS_CONNECT_FAILED;
    }
    uint8_t eof_code = msgbuf.begin()->buffer()[0];
    if (eof_code == 0xfe) {
      break;
    }

    MySQLRow row(query_resp.col_count());
    row.decode(msgbuf);
    rs.rows.emplace_back(row);
  }

  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
