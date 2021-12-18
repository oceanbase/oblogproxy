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

class MsgBuf;

/**
 * 接收一个mysql消息包
 * 参考: https://dev.mysql.com/doc/internals/en/mysql-packet.html
 * @param fd 接收消息的描述符
 * @param timout 等待有消息的超时时间(一旦判断有消息到达，就不再关注timeout). 单位 毫秒
 * @param[out] packet_length 消息包长度
 * @param[out] sequence 消息sequence，参考mysql说明
 * @param[out] msgbuf 接收到的消息包
 * @return 成功返回0
 */
int recv_mysql_packet(int fd, int timeout, uint32_t& packet_length, uint8_t& sequence, MsgBuf& msgbuf);

int recv_mysql_packet(int fd, int timeout, MsgBuf& msgbuf);

int send_mysql_packet(int fd, MsgBuf& msgbuf);

class MysqlResponse {
public:
  friend class MysqlProtocol;

protected:
  virtual int decode(const MsgBuf& msgbuf) = 0;
};

class MysqlOkPacket : public MysqlResponse {
public:
  int decode(const MsgBuf& msgbuf) override;
};

class MysqlEofPacket : public MysqlResponse {
public:
  int decode(const MsgBuf& msgbuf) override;

private:
  static const uint8_t _s_packet_type;

  uint16_t _warnings_count;
  uint16_t _status_flags;
};

class MysqlErrorPacket : public MysqlResponse {
public:
  friend class MysqlProtocol;

  int decode(const MsgBuf& msgbuf) override;

private:
  const uint8_t _packet_type = 0xff;
  uint16_t _code;
  std::string _sql_state_marker;
  std::string _sql_state;
  std::string _message;
};

class MysqlInitialHandShakePacket {
public:
  /**
   * 连接上mysql之后，mysql就向客户端发送一个握手数据包。握手数据包中包含了一个随机生成的字符串(20字节）。
   * 这个随机字符串称为scramble，与密码一起做运算后，发送给mysql server做鉴权
   */
  int decode(const MsgBuf& msgbuf);

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
  int encode(MsgBuf& msgbuf);

private:
  uint32_t calc_capabilities_flag();

private:
  std::string _username;
  std::string _database;
  std::vector<char> _auth_response;
  int8_t _sequence = 0;
};

class MysqlQueryPacket {
public:
  explicit MysqlQueryPacket(const std::string& sql);

  // use memory in-stack, none any heap memory would be alloc
  int encode_inplace(MsgBuf& msgbuf);

private:
  const uint8_t _cmd_id = 0x03;  // COM_QUERY
  const std::string& _sql;
};

class MysqlCol : public MysqlResponse {
public:
  int decode(const MsgBuf& msgbuf) override;

private:
  std::string _catalog;
  std::string _schema;
  std::string _table;
  std::string _org_table;
  std::string _name;
  std::string _org_name;
  uint8_t _len_of_fixed_len;
  uint16_t _charset;
  uint32_t _column_len;
  uint8_t _type;
  uint16_t _flags;
  uint8_t _decimals;
  uint16_t _filler;
};

class MysqlRow : public MysqlResponse {
public:
  friend class MysqlProtocol;

  explicit MysqlRow(uint64_t col_count);

  int decode(const MsgBuf& msgbuf) override;

  inline uint64_t col_count() const
  {
    return _col_count;
  }

  inline const std::vector<std::string>& fields() const
  {
    return _fields;
  }

private:
  uint64_t _col_count;
  std::vector<std::string> _fields;
};

class MysqlQueryResponsePacket : public MysqlResponse {
public:
  friend class MysqlProtocol;

  int decode(const MsgBuf& msgbuf) override;

  inline uint64_t col_count() const
  {
    return _col_count;
  }

private:
  uint64_t _col_count = 0;

  MysqlErrorPacket _err;
};

struct MysqlResultSet {
  void reset();

  uint64_t col_count = 0;
  std::vector<MysqlCol> cols;
  std::vector<MysqlRow> rows;
};

}  // namespace logproxy
}  // namespace oceanbase
