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

namespace oceanbase {
namespace logproxy {

class MessageBuffer;

/**
 * 接收一个mysql消息包
 * 参考
 * https://dev.mysql.com/doc/internals/en/mysql-packet.html
 * @param fd 接收消息的描述符
 * @param timout 等待有消息的超时时间(一旦判断有消息到达，就不再关注timeout). 单位 毫秒
 * @param[out] packet_length 消息包长度
 * @param[out] sequence 消息sequence，参考mysql说明
 * @param[out] message_buffer 接收到的消息包
 * @return 成功返回0
 */
int recv_mysql_packet(int fd, int timeout, uint32_t& packet_length, uint8_t& sequence, MessageBuffer& message_buffer);

class MysqlInitialHandShakePacket {
public:
  /**
   * 连接上mysql之后，mysql就向客户端发送一个握手数据包。握手数据包中包含了一个随机生成的字符串(20字节）。
   * 这个随机字符串称为scramble，与密码一起做运算后，发送给mysql server做鉴权
   */
  int decode(const MessageBuffer& message_buffer);

  bool scramble_buffer_valid() const;
  const std::vector<char>& scramble_buffer() const;

  uint8_t sequence() const;

private:
  uint8_t _sequence = 0;
  uint8_t _protocol_version = 0;
  uint32_t _capabilities_flag = 0;
  std::vector<char> _scramble_buffer;
  bool _scramble_buffer_valid = false;
};

class MysqlHandShakeResponsePacket {
public:
  MysqlHandShakeResponsePacket(const std::string& username, const std::string& database,
      const std::vector<char>& auth_response, int8_t sequence);

  /**
   * 客户端创建socket连接成功mysql server后，MySQL会发一个握手包，之后客户端向MySQL server回复一个消息。
   * 这里就负责这条消息的编码。
   */
  int encode(MessageBuffer& message_buffer);

private:
  uint32_t calc_capabilities_flag();

private:
  std::string _username;
  std::string _database;
  std::vector<char> _auth_response;
  int8_t _sequence = 0;
};

class MysqlOkPacket {
public:
  /**
   * MySQL server向客户端返回一个结果消息包，这里根据消息包来判断回复的结果是成功，还是失败
   * 比如鉴权请求，回复成功，表示鉴权成功。否则表示鉴权失败，用户没有权限。
   */
  bool result_ok(const MessageBuffer& message_buffer) const;
};
}  // namespace logproxy
}  // namespace oceanbase
