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

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <poll.h>
#include <vector>

#include "obaccess/ob_mysql_auth.h"
#include "obaccess/ob_mysql_packet.h"
#include "obaccess/ob_sha1.h"
#include "common/log.h"
#include "common/common.h"
#include "communication/io.h"
#include "codec/codec_endian.h"
#include "codec/message_buffer.h"

namespace oceanbase {
namespace logproxy {

void my_xor(const unsigned char* s1, const unsigned char* s2, uint32_t len, unsigned char* to)
{
  const unsigned char* s1_end = s1 + len;
  while (s1 < s1_end) {
    *to++ = *s1++ ^ *s2++;
  }
}

ObMysqlAuth::ObMysqlAuth(const std::string& host, int port, const std::string& username,
    const std::vector<char>& passwd_sha1, const std::string& database)
    : _username(username), _passwd_sha1(passwd_sha1), _database(database), _hostname(host), _port(port)
{}

ObMysqlAuth::~ObMysqlAuth()
{
  if (_sock >= 0) {
    close(_sock);
    _sock = -1;
  }
}

void ObMysqlAuth::set_detect_timeout(int t)
{
  _detect_timeout = t;
}

int ObMysqlAuth::connect_to_server()
{
  int ret = connect(_hostname.c_str(), _port, false, _detect_timeout, _sock);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to connect to server. server=" << _hostname << ':' << _port;
  } else {
    OMS_INFO << "Connect to server success. server=" << _hostname << ':' << _port;
  }
  return ret;
}

int ObMysqlAuth::calc_mysql_auth_info(const std::vector<char>& scramble_buffer, std::vector<char>& auth_info)
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

int ObMysqlAuth::send_auth_info(const std::vector<char>& auth_info, uint8_t sequence)
{
  MysqlHandShakeResponsePacket hand_shake_response_packet(_username, _database, auth_info, sequence);
  MessageBuffer message_buffer;
  int ret = hand_shake_response_packet.encode(message_buffer);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to encode hand shake response packet";
    return -1;
  }

  for (auto iter = message_buffer.begin(), end = message_buffer.end(); iter != end; ++iter) {
    ret = writen(_sock, iter->buffer(), iter->size());
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to send hand shake response message. error=" << strerror(errno) << ". peer=" << _hostname
                << ":" << _port;
      return ret;
    }
  }
  return OMS_OK;
}

int ObMysqlAuth::is_mysql_response_ok()
{
  uint32_t packet_length = 0;
  uint8_t sequence = 0;
  MessageBuffer message_buffer;
  int ret = recv_mysql_packet(_sock, _detect_timeout, packet_length, sequence, message_buffer);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to receive ok packet from server " << _hostname << ':' << _port
              << ", error=" << strerror(errno);
  } else {
    MysqlOkPacket ok_packet;
    if (ok_packet.result_ok(message_buffer)) {
      ret = OMS_OK;
    } else {
      ret = OMS_FAILED;
    }
  }
  return ret;
}

int ObMysqlAuth::auth()
{
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
  MessageBuffer message_buffer;
  ret = recv_mysql_packet(_sock, _detect_timeout, packet_length, sequence, message_buffer);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to receive hand shake packet. error=" << strerror(errno) << ". server=" << _hostname << ':'
              << _port;
    return ret;
  }
  OMS_DEBUG << "receive hand shake packet from server=" << _hostname << ':' << _port;

  MysqlInitialHandShakePacket hand_shake_packet;
  ret = hand_shake_packet.decode(message_buffer);
  if (ret != OMS_OK || !hand_shake_packet.scramble_buffer_valid()) {
    OMS_ERROR << "Failed to decode_payload initial hand shake packet or does not has a valid scramble:"
              << hand_shake_packet.scramble_buffer_valid() << ". length=" << packet_length << ", server=" << _hostname
              << ':' << _port;
    return ret;
  }

  const std::vector<char>& scramble_buffer = hand_shake_packet.scramble_buffer();

  // 3 calculate the password combined with scramble buffer -> auth information
  std::vector<char> auth_info;
  ret = calc_mysql_auth_info(scramble_buffer, auth_info);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to calc auth info. server=" << _hostname << ':' << _port << ", user=" << _username;
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

  if (OMS_OK == ret) {
    OMS_INFO << "Auth user success. server=" << _hostname << ':' << _port << ", user=" << _username << ", database='"
             << _database << '\'';
  } else {
    OMS_DEBUG << "Auth user failed. server=" << _hostname << ':' << _port << ", user=" << _username << ", database='"
              << _database << '\'';
  }
  return ret;
}
}  // namespace logproxy
}  // namespace oceanbase
