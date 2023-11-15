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
#define MAX_PACKET_SIZE 16777215

class MsgBuf;

int recv_mysql_packet(int fd, int timeout, uint32_t& packet_length, uint8_t& sequence, MsgBuf& msgbuf);

int recv_mysql_packet(int fd, int timeout, MsgBuf& msgbuf);

int send_mysql_packet(int fd, MsgBuf& msgbuf, uint8_t sequence);

uint32_t binlog_server_capability_flags();

/*
 * @params packet_size
 * @returns
 * @description
 * @date 2022/9/21 17:20
 */
int send_mysql_packets(int fd, MsgBuf& msgbuf, uint8_t& sequence, size_t packet_size = MAX_PACKET_SIZE);

class MySQLResponse {
public:
  friend class MysqlProtocol;

protected:
  virtual int decode(const MsgBuf& msgbuf) = 0;
  virtual int serialize(MsgBuf& msg_buf, int64_t len) = 0;
};

class MySQLOkPacket : public MySQLResponse {
public:
  int decode(const MsgBuf& msgbuf) override;

protected:
  int serialize(MsgBuf& msg_buf, int64_t len) override;

public:
  uint64_t get_affected_rows() const;

private:
  uint64_t _affected_rows;
  uint64_t _last_insert_id;
  std::string _info;
  std::string _session_state_changes;
};

class MySQLEofPacket : public MySQLResponse {
public:
  int decode(const MsgBuf& msgbuf) override;
  int serialize(MsgBuf& msg_buf, int64_t len) override;
  uint16_t get_warnings_count() const;
  void set_warnings_count(uint16_t warnings_count);
  uint16_t get_status_flags() const;
  void set_status_flags(uint16_t status_flags);

private:
  static const uint8_t _s_packet_type;

  uint16_t _warnings_count;
  uint16_t _status_flags;
};

class MySQLErrorPacket : public MySQLResponse {
public:
  friend class MysqlProtocol;

  int decode(const MsgBuf& msgbuf) override;
  int serialize(MsgBuf& msg_buf, int64_t len) override;

  int64_t get_serialize_size();
  const uint8_t get_packet_type() const;
  uint16_t get_code() const;
  void set_code(uint16_t code);
  const std::string& get_sql_state_marker() const;
  void set_sql_state_marker(const std::string& sql_state_marker);
  const std::string& get_sql_state() const;
  void set_sql_state(const std::string& sql_state);
  const std::string& get_message() const;
  void set_message(const std::string& message);

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

protected:
  int serialize(MsgBuf& msg_buf, int64_t len) override;

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

protected:
  int serialize(MsgBuf& msg_buf, int64_t len) override;

public:
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

protected:
  int serialize(MsgBuf& msg_buf, int64_t len) override;

public:
  inline uint64_t col_count() const
  {
    return _col_count;
  }

private:
  uint64_t _col_count = 0;
  MySQLOkPacket _ok;
  MySQLErrorPacket _err;
};

struct MySQLResultSet {
  void reset();

  uint64_t col_count = 0;
  std::vector<MySQLCol> cols;
  std::vector<MySQLRow> rows;
  uint64_t affect_rows;

  uint16_t code;
  std::string message;
};

int write_lenenc_uint(char* buf, size_t capacity, uint64_t integer);

int write_null_terminate_string(char* buf, size_t capacity, const std::string& str);

int write_string(char* buf, size_t capacity, const char* s, size_t str_len);
/*
 * @params
 * @returns lenenc_uint
 * @description
 * @date 2022/9/28 15:17
 */
uint64_t get_lenenc_uint(unsigned char* buf, uint64_t& pos);

}  // namespace logproxy
}  // namespace oceanbase
