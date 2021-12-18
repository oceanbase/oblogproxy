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
    OMS_ERROR << "Failed to connect to server. server=" << _hostname << ':' << _port;
  } else {
    OMS_INFO << "Connect to server success. server=" << _hostname << ':' << _port;
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

int MysqlProtocol::calc_mysql_auth_info(const std::vector<char>& scramble_buffer, std::vector<char>& auth_info)
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
  scramble_combined.reserve(scramble_buffer.size() + passwd_stage2.size());
  scramble_combined.assign(scramble_buffer.begin(), scramble_buffer.end());
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

  auth_info.resize(sha_combined.size());
  my_xor((const unsigned char*)_passwd_sha1.data(),
      (const unsigned char*)sha_combined.data(),
      auth_info.size(),
      (unsigned char*)auth_info.data());
  return OMS_OK;
}

int MysqlProtocol::send_auth_info(const std::vector<char>& auth_info, uint8_t sequence)
{
  MysqlHandShakeResponsePacket hand_shake_response_packet(_username, "", auth_info, sequence);
  MsgBuf msgbuf;
  int ret = hand_shake_response_packet.encode(msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode hand shake response packet";
    return -1;
  }

  for (const auto& iter : msgbuf) {
    ret = writen(_sockfd, iter.buffer(), iter.size());
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to send hand shake response message. error=" << strerror(errno) << ". peer=" << _hostname
                << ":" << _port;
      return ret;
    }
  }
  return OMS_OK;
}

int MysqlProtocol::is_mysql_response_ok()
{
  MsgBuf msgbuf;
  int ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to receive ok packet from server " << _hostname << ':' << _port
              << ", error=" << strerror(errno);
    return OMS_FAILED;
  }
  MysqlOkPacket ok_packet;
  return ok_packet.decode(msgbuf);
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

  // 2 receive initial hand shake
  uint32_t packet_length = 0;
  uint8_t sequence = 0;
  MsgBuf msgbuf;
  ret = recv_mysql_packet(_sockfd, _detect_timeout, packet_length, sequence, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to receive hand shake packet. error=" << strerror(errno) << ". server=" << _hostname << ':'
              << _port;
    return ret;
  }
  OMS_DEBUG << "receive hand shake packet from server=" << _hostname << ':' << _port;

  MysqlInitialHandShakePacket handshake_packet;
  ret = handshake_packet.decode(msgbuf);
  if (ret != OMS_OK || !handshake_packet.scramble_buffer_valid()) {
    OMS_ERROR << "Failed to decode_payload initial hand shake packet or does not has a valid scramble:"
              << handshake_packet.scramble_buffer_valid() << ". length=" << packet_length << ", server=" << _hostname
              << ':' << _port;
    return ret;
  }

  const std::vector<char>& scramble_buffer = handshake_packet.scramble_buffer();

  // 3 calculate the password combined with scramble buffer -> auth information
  std::vector<char> auth_info;
  ret = calc_mysql_auth_info(scramble_buffer, auth_info);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to calc login info. server=" << _hostname << ':' << _port << ", user=" << _username;
    return ret;
  }

  // 4 send the hand shake response with auth information
  ret = send_auth_info(auth_info, sequence + 1);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send hand shake response message to " << _hostname << ':' << _port
              << ", user=" << _username;
    return OMS_FAILED;
  }

  // 5 receive response from server
  ret = is_mysql_response_ok();

  if (ret == OMS_OK) {
    OMS_INFO << "Auth user success. server=" << _hostname << ':' << _port << ", user=" << _username;
  } else {
    OMS_DEBUG << "Auth user failed. server=" << _hostname << ':' << _port << ", user=" << _username;
  }
  return ret;
}

int MysqlProtocol::query(const std::string& sql, MysqlResultSet& rs)
{
  MysqlQueryPacket packet(sql);
  MsgBuf msgbuf;
  int ret = packet.encode_inplace(msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode mysql sql packet ,ret:" << ret;
    return ret;
  }
  ret = send_mysql_packet(_sockfd, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send query packet to server:" << _hostname << ':' << _port;
    return OMS_FAILED;
  }

  ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to recv query packet from server:" << _hostname << ':' << _port;
    return OMS_FAILED;
  }

  MysqlQueryResponsePacket query_resp;
  ret = query_resp.decode(msgbuf);
  if (ret != OMS_OK || query_resp.col_count() == 0) {
    OMS_ERROR << "Failed to query mysql:" << query_resp._err._message
              << (query_resp.col_count() == 0 ? ", unexpected column count 0" : "");
    return OMS_FAILED;
  }

  // column definitions
  rs.col_count = query_resp.col_count();
  rs.cols.resize(query_resp.col_count());
  for (uint64_t i = 0; i < query_resp.col_count(); ++i) {
    ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to recv column defines packet from server:" << _hostname << ':' << _port;
      return OMS_FAILED;
    }

    MysqlCol column;
    column.decode(msgbuf);
    rs.cols[i] = column;
  }

  // eof
  ret = recv_mysql_packet(_sockfd, _detect_timeout, msgbuf);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to recv eof packet from server:" << _hostname << ':' << _port;
    return OMS_FAILED;
  }
  MysqlEofPacket eof;
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
      return OMS_FAILED;
    }
    uint8_t eof_code = msgbuf.begin()->buffer()[0];
    if (eof_code == 0xfe) {
      break;
    }

    MysqlRow row(query_resp.col_count());
    row.decode(msgbuf);
    rs.rows.emplace_back(row);
  }

  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
