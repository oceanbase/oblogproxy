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
 * rece a mysql protocol packet
 * @param fd
 * @param timout 等待有消息的超时时间(一旦判断有消息到达，就不再关注timeout). 单位 毫秒
 * @param[out] packet_length 消息包长度
 * @param[out] sequence 消息sequence，参考mysql说明
 * @param[out] msgbuf 接收到的消息包
 * @return 成功返回0
 */
int recv_mysql_packet(int fd, int timeout, uint32_t& packet_length, uint8_t& sequence, MsgBuf& msgbuf);

int recv_mysql_packet(int fd, int timeout, MsgBuf& msgbuf);

int send_mysql_packet(int fd, MsgBuf& msgbuf, uint8_t sequence);

class MySQLResponse {
public:
  friend class MysqlProtocol;

protected:
  virtual int decode(const MsgBuf& msgbuf) = 0;
};

class MySQLOkPacket : public MySQLResponse {
public:
  int decode(const MsgBuf& msgbuf) override;
};

class MySQLEofPacket : public MySQLResponse {
public:
  int decode(const MsgBuf& msgbuf) override;

private:
  static const uint8_t _s_packet_type;

  uint16_t _warnings_count;
  uint16_t _status_flags;
};

class MySQLErrorPacket : public MySQLResponse {
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

class MySQLInitialHandShakePacket {
public:
  int decode(const MsgBuf& msgbuf);

  bool scramble_valid() const;

  const std::vector<char>& scramble() const;

  uint8_t sequence() const;

private:
  uint8_t _sequence = 0;
  uint8_t _protocol_version = 0;
  uint32_t _capabilities_flag = 0;
  std::string _auth_plugin_name;

  bool _scramble_valid = false;
  std::vector<char> _scramble;
};

class MySQLHandShakeResponsePacket {
public:
  MySQLHandShakeResponsePacket(
      const std::string& username, const std::string& database, const std::vector<char>& auth_response);

  int encode(MsgBuf& msgbuf);

private:
  std::string _username;
  std::string _database;
  std::vector<char> _auth_response;
};

class MySQLQueryPacket {
public:
  explicit MySQLQueryPacket(const std::string& sql);

  // use memory in-stack, none any heap memory would be alloc
  int encode_inplace(MsgBuf& msgbuf);

private:
  const uint8_t _cmd_id = 0x03;  // COM_QUERY
  const std::string& _sql;
};

class MySQLCol : public MySQLResponse {
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

class MySQLRow : public MySQLResponse {
public:
  friend class MysqlProtocol;

  explicit MySQLRow(uint64_t col_count);

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

class MySQLQueryResponsePacket : public MySQLResponse {
public:
  friend class MysqlProtocol;

  int decode(const MsgBuf& msgbuf) override;

  inline uint64_t col_count() const
  {
    return _col_count;
  }

private:
  uint64_t _col_count = 0;

  MySQLErrorPacket _err;
};

struct MySQLResultSet {
  void reset();

  uint64_t col_count = 0;
  std::vector<MySQLCol> cols;
  std::vector<MySQLRow> rows;
};

}  // namespace logproxy
}  // namespace oceanbase
