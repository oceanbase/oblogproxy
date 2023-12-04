/**
 * Copyright (c) 2023 OceanBase
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
#include <map>
#include <cstdint>
#include "oblogevent_type.h"
#include "codec/byte_decoder.h"
#include "fs_util.h"
#include "log.h"
#include "msg_buf.h"

namespace oceanbase {
namespace logproxy {
#define SERVER_VERSION "5.7.38"
#define BINLOG_VERSION 4
// temp
#define SERVER_ID 1147473732
#define XID_LEN 8
#define COMMON_HEADER_LENGTH 19
#define ST_SERVER_VER_LEN 50
#define EVENT_TYPE_HEADER_LEN 40
#define COMMON_CHECKSUM_LENGTH 4
#define ROWS_HEADER_LEN 8
#define VAR_HEADER_LEN 2
#define SERVER_UUID_LEN 16
#define MAX_PACKET_INTEGER_LEN 9

// status common _header
#define TYPE_CODE_OFFSET 4
#define SERVER_ID_OFFSET 5
#define EVENT_LEN_OFFSET 9
#define LOG_POS_OFFSET 13
#define FLAGS_OFFSET 17
#define FLAGS_LEN 2

#define BINLOG_MAGIC_SIZE 4
#define BINLOG_INDEX_NAME "mysql-bin.index"
#define BINLOG_DATA_DIR "/data/"

#define BEGIN_VAR "BEGIN"
#define BEGIN_VAR_LEN 5
#define LOG_EVENT_BINLOG_IN_USE_F 0x1
#define BINLOG_START_POS 190

constexpr uint8_t binlog_magic[4] = {254, 98, 105, 110};

static const int LOG_EVENT_TYPES = (ENUM_END_EVENT - 1);

enum enum_post_header_length {
  QUERY_HEADER_MINIMAL_LEN = (4 + 4 + 1 + 2),
  QUERY_HEADER_LEN = (QUERY_HEADER_MINIMAL_LEN + 2),
  STOP_HEADER_LEN = 0,
  LOAD_HEADER_LEN = (4 + 4 + 4 + 1 + 1 + 4),
  START_V3_HEADER_LEN = (2 + ST_SERVER_VER_LEN + 4),
  // this is FROZEN (the Rotate post-_header is frozen)
  ROTATE_HEADER_LEN = 8,
  INTVAR_HEADER_LEN = 0,
  CREATE_FILE_HEADER_LEN = 4,
  APPEND_BLOCK_HEADER_LEN = 4,
  EXEC_LOAD_HEADER_LEN = 4,
  DELETE_FILE_HEADER_LEN = 4,
  NEW_LOAD_HEADER_LEN = LOAD_HEADER_LEN,
  RAND_HEADER_LEN = 0,
  USER_VAR_HEADER_LEN = 0,
  FORMAT_DESCRIPTION_HEADER_LEN = (START_V3_HEADER_LEN + 1 + LOG_EVENT_TYPES),
  XID_HEADER_LEN = 0,
  BEGIN_LOAD_QUERY_HEADER_LEN = APPEND_BLOCK_HEADER_LEN,
  ROWS_HEADER_LEN_V1 = 8,
  TABLE_MAP_HEADER_LEN = 8,
  EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN = (4 + 4 + 4 + 1),
  EXECUTE_LOAD_QUERY_HEADER_LEN = (QUERY_HEADER_LEN + EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN),
  INCIDENT_HEADER_LEN = 2,
  HEARTBEAT_HEADER_LEN = 0,
  IGNORABLE_HEADER_LEN = 0,
  ROWS_HEADER_LEN_V2 = 10,
  TRANSACTION_CONTEXT_HEADER_LEN = 18,
  VIEW_CHANGE_HEADER_LEN = 52,
  XA_PREPARE_HEADER_LEN = 0,
  GTID_HEADER_LEN = 42
};  // end enum_post_header_length

enum enum_checksum_flag { OFF = 0, CRC32 = 1, UNDEF = 255 };

enum enum_flag {
  /** Last event of a statement */
  STMT_END_F = (1U << 0),
  /** Value of the OPTION_NO_FOREIGN_KEY_CHECKS flag in thd->options */
  NO_FOREIGN_KEY_CHECKS_F = (1U << 1),
  /** Value of the OPTION_RELAXED_UNIQUE_CHECKS flag in thd->options */
  RELAXED_UNIQUE_CHECKS_F = (1U << 2),
  /**
    Indicates that rows in this event are complete, that is contain
    values for all columns of the table.
  */
  COMPLETE_ROWS_F = (1U << 3)
};

/*!
 * @brief
 * https://dev.mysql.com/doc/dev/mysql-server/latest/classbinary__log_1_1Query__event.html#afc5c7b5942e775bd74f1f6416814eb49
 */
enum QUERY_EVENT_STATUS_VARS {
  /**
   The flags in thd->options, binary AND-ed with OPTIONS_WRITTEN_TO_BIN_LOG. The thd->options bitfield contains options
for "SELECT". OPTIONS_WRITTEN identifies those options that need to be written to the binlog (not all do). Specifically,
OPTIONS_WRITTEN_TO_BIN_LOG equals (OPTION_AUTO_IS_NULL | OPTION_NO_FOREIGN_KEY_CHECKS | OPTION_RELAXED_UNIQUE_CHECKS |
OPTION_NOT_AUTOCOMMIT), or 0x0c084000 in hex. These flags correspond to the SQL variables SQL_AUTO_IS_NULL,
FOREIGN_KEY_CHECKS, UNIQUE_CHECKS, and AUTOCOMMIT, documented in the "SET Syntax" section of the MySQL Manual.

   This field is always written to the binlog in version >= 5.0, and never written in version < 5.0.
   */
  Q_FLAGS2_CODE = 0,
  Q_SQL_MODE_CODE,
  /**
   Stores the client's current catalog. Every database belongs to a catalog, the same way that every table belongs to a
database. Currently, there is only one catalog, "std". This field is written if the length of the catalog is > 0;
otherwise it is not written.
   */
  Q_CATALOG_CODE,
  Q_AUTO_INCREMENT,
  /**
   The three variables character_set_client, collation_connection, and collation_server, in that order.
character_set_client is a code identifying the character set and collation used by the client to encode the query.
collation_connection identifies the character set and collation that the master converts the query to when it receives
it; this is useful when comparing literal strings. collation_server is the default character set and collation used when
a new database is created.See also "Connection Character Sets and Collations" in the MySQL 5.1 manual.

All three variables are codes identifying a (character set, collation) pair. To see which codes map to which pairs, run
the query "SELECT id, character_set_name, collation_name FROM COLLATIONS".

   Cf. Q_CHARSET_DATABASE_CODE below.

   This field is always written.
 */
  Q_CHARSET_CODE,
  Q_TIME_ZONE_CODE,
  Q_CATALOG_NZ_CODE,
  Q_LC_TIME_NAMES_CODE,
  Q_CHARSET_DATABASE_CODE,
  Q_TABLE_MAP_FOR_UPDATE_CODE,
  Q_MASTER_DATA_WRITTEN_CODE,
  Q_INVOKER,
  Q_UPDATED_DB_NAMES,
  Q_MICROSECONDS,
  Q_COMMIT_TS,
  Q_COMMIT_TS2,
  Q_EXPLICIT_DEFAULTS_FOR_TIMESTAMP,
  Q_DDL_LOGGED_WITH_XID,
  Q_DEFAULT_COLLATION_FOR_UTF8MB4,
  Q_SQL_REQUIRE_PRIMARY_KEY,
  Q_DEFAULT_TABLE_ENCRYPTION,
  QUERY_EVENT_STATUS_VARS_END
};

static const uint8_t status_vars_bitfield[QUERY_EVENT_STATUS_VARS_END] = {
    4, 8, 255, 4, 6, 255, 2, 2, 8, 4, 255, 255, 255, 255, 255, 1, 8, 2, 2, 2, 2};

/*
 +=====================================+
| event  | timestamp         0 : 4    |
| _header +----------------------------+
|        | type_code         4 : 1    |
|        +----------------------------+
|        | server_id         5 : 4    |
|        +----------------------------+
|        | event_length      9 : 4    |
|        +----------------------------+
|        | next_position    13 : 4    |
|        +----------------------------+
|        | flags            17 : 2    |
|        +----------------------------+
|        | extra_headers    19 : x-19 |
+=====================================+
 */
class OblogEventHeader {
public:
  uint64_t get_timestamp() const;

  void set_timestamp(uint64_t timestamp);

  EventType get_type_code() const;

  void set_type_code(EventType type_code);

  uint32_t get_server_id() const;

  void set_server_id(uint32_t server_id);

  uint32_t get_event_length();

  void set_event_length(uint32_t event_length);

  uint32_t get_next_position() const;

  void set_next_position(uint32_t next_position);

  uint16_t get_flags() const;

  void set_flags(uint16_t flags);

  explicit OblogEventHeader(EventType type_code, uint64_t timestamp, uint32_t event_length, uint32_t next_position);

  explicit OblogEventHeader(EventType type_code);

  OblogEventHeader() = default;

  ~OblogEventHeader() = default;

  size_t flush_to_buff(unsigned char* header);

  void deserialize(unsigned char* buff);

  // Format binlog event, print out binlog event offset information
  std::string str_format();

private:
  uint64_t _timestamp;
  EventType _type_code;
  uint32_t _server_id;
  uint32_t _event_length;
  uint32_t _next_position;
  uint16_t _flags;
};

class ObLogEvent {
private:
  OblogEventHeader* _header = nullptr;
  std::string _ob_txn;

  uint64_t _checkpoint = 0;
  uint8_t _checksum_flag = OFF;

public:
  const std::string& get_ob_txn() const;

  void set_ob_txn(const std::string& ob_txn);

  uint64_t get_checkpoint() const;

  void set_checkpoint(uint64_t checkpoint);

  uint8_t get_checksum_flag() const;

  void set_checksum_flag(uint8_t checksum_flag);

  uint8_t get_checksum_len() const;

  OblogEventHeader* get_header();

  void set_header(OblogEventHeader* header);

  ObLogEvent();

  virtual ~ObLogEvent();

  // decode event
  virtual size_t flush_to_buff(unsigned char* data) = 0;

  virtual void deserialize(unsigned char* buff) = 0;

  // Format binlog event, print out binlog event offset information
  std::string str_format();

  size_t write_checksum(unsigned char* buff, size_t& pos) const;

  virtual std::string print_event_info() = 0;
};

/*
+=================================================+
| fixed  | version                      0 : 2     |
| part   +----------------------------------------+
|        | server_version               2 : 50    |
|        +----------------------------------------+
|        | timestamp                    52 : 4    |
|        +----------------------------------------+
|        | header_length                56 : 1    |
|        +----------------------------------------+
|        | event_type_header_lengths    57 : x    |
|        | _checksum_flag    57+x : 1               |
|        | checksum        58+x : 4               |
+=================================================+
|variable|                                        |
| part   +                                        |
|        |                                        |
+=================================================+
 * */
class FormatDescriptionEvent : public ObLogEvent {
public:
  FormatDescriptionEvent(std::string server_version, uint64_t timestamp, uint8_t header_length,
      std::vector<uint8_t> event_type_header_lengths);
  FormatDescriptionEvent(uint64_t timestamp, uint32_t server_id);
  FormatDescriptionEvent() = default;
  ~FormatDescriptionEvent() override = default;
  size_t flush_to_buff(unsigned char* data) override;
  uint16_t get_version() const;
  void set_version(uint16_t version);
  const std::string& get_server_version() const;
  void set_server_version(std::string server_version);
  uint64_t get_timestamp() const;
  void set_timestamp(uint64_t timestamp);
  uint8_t get_header_len() const;
  void set_header_len(uint8_t header_len);
  std::vector<uint8_t> get_event_type_header_len() const;
  void set_event_type_header_len(const std::vector<uint8_t>& event_type_header_len);
  void deserialize(unsigned char* buff) override;
  std::string print_event_info() override;

private:
  uint16_t _version;
  std::string _server_version;
  uint64_t _timestamp;
  uint8_t _header_len;
  std::vector<uint8_t> _event_type_header_len;
};

typedef std::pair<std::uint64_t, std::uint64_t> txn_range;
class GtidMessage {
private:
  unsigned char* _gtid_uuid = nullptr;
  uint64_t _gtid_txn_id_intervals = 0;
  /*  uint32_t _txn_start;
    uint32_t _txn_end;*/
  std::vector<txn_range> _txn_range;

public:
  virtual ~GtidMessage();

  std::vector<txn_range>& get_txn_range();

  void set_txn_range(std::vector<txn_range> txn_range);

  GtidMessage() = default;

  GtidMessage(unsigned char* gtid_uuid, uint64_t gtid_txn_id_intervals);

  unsigned char* get_gtid_uuid();

  std::string get_gtid_uuid_str();

  void set_gtid_uuid(unsigned char* gtid_uuid);

  int set_gtid_uuid(const std::string& gtid_uuid);

  /*
   * @params gtid_set
   * @returns
   * @description   Serialize a transaction set corresponding to gtid from string
   * @date 2023/6/15 12:16
   */
  int deserialize_gtid_set(const std::string& gtid_set);

  int merge_txn_range(GtidMessage* message, GtidMessage* target);

  uint64_t get_gtid_txn_id_intervals();

  void set_gtid_txn_id_intervals(uint64_t gtid_txn_id_intervals);

  std::string format_string();

  GtidMessage(const GtidMessage& gtid_message)
  {
    if (this != &gtid_message) {
      if (_gtid_uuid != nullptr) {
        free(_gtid_uuid);
      }
      _gtid_uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
      memcpy(_gtid_uuid, gtid_message._gtid_uuid, SERVER_UUID_LEN);
      _gtid_txn_id_intervals = gtid_message._gtid_txn_id_intervals;
      _txn_range.assign(gtid_message._txn_range.begin(), gtid_message._txn_range.end());
    }
  }

  GtidMessage& operator=(const GtidMessage& gtid_message)
  {
    if (this != &gtid_message) {
      if (_gtid_uuid != nullptr) {
        free(_gtid_uuid);
      }
      _gtid_uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
      memcpy(_gtid_uuid, gtid_message._gtid_uuid, SERVER_UUID_LEN);
      _gtid_txn_id_intervals = gtid_message._gtid_txn_id_intervals;
      _txn_range.assign(gtid_message._txn_range.begin(), gtid_message._txn_range.end());
    }
    return *this;
  }
};

/*
+===========================================+
| fixed   | gtid_uuid_num           0 : 8    | 表示该binlog文件中后续由多少个GTID的uuid
| part    +---------------------------------+
|         | gtid_uuid              8 : 16   | GTID中的uuid,2个uuid字符占一个字节（不包含'-'）
|         +---------------------------------+
|         | gtid_txn_id_intervals  24 : 8   | 表示有几个事务号存在间隔
|         +---------------------------------+
|         | txn_start              32 : 8   | 间隔事务号的起始值
|         +---------------------------------+
|         | txn_end                40 : 8   | 间隔事务号的结束值+1
+===========================================+
| variable| check_sum                       | 校验和，4字节
| part    |                                 |
+======================================+
 */
class PreviousGtidsLogEvent : public ObLogEvent {
public:
  PreviousGtidsLogEvent() = default;

  PreviousGtidsLogEvent(uint32_t gtid_uuid_num, const std::vector<GtidMessage*>& gtid_messages, uint64_t timestamp);

  ~PreviousGtidsLogEvent();

  size_t flush_to_buff(unsigned char* data) override;

  uint64_t get_gtid_uuid_num() const;

  void set_gtid_uuid_num(uint64_t gtid_uuid_num);

  std::map<std::string, GtidMessage*>& get_gtid_messages();

  void set_gtid_messages(const std::vector<GtidMessage*>& gtid_messages);

  void deserialize(unsigned char* buff) override;

  std::string print_event_info() override;

private:
  uint64_t _gtid_uuid_num = 0;
  std::map<std::string, GtidMessage*> _gtid_messages;
};

/*
 +======================================+
| fixed   | commit_flag       0 : 1    | 事务是否提交
| part    +----------------------------+
|         | gtid_uuid         1 : 16   | GTID中的uuid,2个uuid字符占一个字节（不包含'-'）
|         +----------------------------+
|         | gtid_txn_id       17 : 8   | GTID中的事务号
|         | ts_type           25 : 1   | 逻辑时间类型
|         | last_committed    26 : 8   | 逻辑时间s
|         | sequence_number   34 : 8   | 以微秒为逻辑时间的sequence number
+======================================+
| variable| check_sum                  | 校验和，4字节
| part    |                            |
+======================================+
 */
class GtidLogEvent : public ObLogEvent {
public:
  ~GtidLogEvent() override;
  unsigned char get_commit_flag() const;
  void set_commit_flag(unsigned char commit_flag);
  const unsigned char* get_gtid_uuid() const;
  void set_gtid_uuid(unsigned char* gtid_uuid);
  void set_gtid_uuid(const std::string& gtid_uuid);
  uint64_t get_gtid_txn_id() const;
  void set_gtid_txn_id(uint64_t gtid_txn_id);
  uint64_t get_last_committed() const;
  void set_last_committed(uint64_t last_committed);
  uint64_t get_sequence_number() const;
  void set_sequence_number(uint64_t sequence_number);
  int get_ts_type() const;
  void set_ts_type(int ts_type);
  size_t flush_to_buff(unsigned char* data) override;
  /**
   * Deserialize the corresponding event from the buff
   * @param buff
   */
  void deserialize(unsigned char* buff) override;
  std::string print_event_info() override;

  static const int LOGICAL_TIMESTAMP_TYPECODE_LEN = 1;
  static const int LOGICAL_TIMESTAMP_TYPECODE = 2;

private:
  unsigned char _commit_flag = 1;
  unsigned char* _gtid_uuid;
  uint64_t _gtid_txn_id;
  uint64_t _last_committed;
  uint64_t _sequence_number;
  int _ts_type = LOGICAL_TIMESTAMP_TYPECODE;
};

/*
 +==========================================+
| fixed   | next_binlog_position  0 : 8    | 下一个binlog文件起始的偏移地址
| part    |                                |
+==========================================+
| variable| next_binlog_file_name          | 下一个binlog的文件名
| part    |
+==========================================+
 */

class RotateEvent : public ObLogEvent {
public:
  enum OP { INIT, ROTATE };
  RotateEvent(
      uint64_t next_binlog_position, std::string next_binlog_file_name, uint64_t timestamp, uint32_t begin_position);
  ~RotateEvent() override = default;
  size_t flush_to_buff(unsigned char* data) override;
  uint64_t get_next_binlog_position() const;
  void set_next_binlog_position(uint64_t next_binlog_position);
  const std::string& get_next_binlog_file_name() const;
  void set_next_binlog_file_name(std::string next_binlog_file_name);
  OP get_op() const;
  void set_op(OP op);
  uint64_t get_index() const;
  void set_index(uint64_t index);
  bool is_existed() const;
  void set_existed(bool existed);
  void deserialize(unsigned char* buff) override;
  std::string print_event_info() override;

private:
  uint64_t _next_binlog_position;
  std::string _next_binlog_file_name;
  uint64_t _index;
  OP _op;
  bool existed = false;
};

/*
 +======================================+
| fixed   | thread_id         0 : 4    | 触发该语句的线程id
| part    +----------------------------+
|         | query_exec_time   4 : 4    | 语句执行的时间
|         +----------------------------+
|         | db_len            8 : 1    | 数据库名长度
|         +----------------------------+
|         | error_code        9 : 2    | 错误码
|         +----------------------------+
|         | status_vars_len   11 : 2   | 状态值的长度x
+======================================+
|         | status_vars                | 记录状态值，长度为status_vars_len
|         +----------------------------+
| variable| db_name                    | 数据库名
| part    +----------------------------+
|         | sql_statment               | 执行的sql语句
|         +----------------------------+
|         | check_sum                  | 校验码4字节
+======================================+
 */

class QueryEvent : public ObLogEvent {
public:
  uint32_t get_thread_id() const;

  void set_thread_id(uint32_t thread_id);

  uint32_t get_query_exec_time() const;

  void set_query_exec_time(uint32_t query_exec_time);

  size_t get_db_len() const;

  void set_db_len(size_t db_len);

  uint16_t get_error_code() const;

  void set_error_code(uint16_t error_code);

  uint16_t get_status_var_len() const;

  void set_status_var_len(uint16_t status_var_len);

  const std::string& get_status_vars() const;

  void set_status_vars(std::string status_vars);

  const std::string& get_dbname() const;

  void set_dbname(std::string dbname);

  const std::string& get_sql_statment() const;

  void set_sql_statment(std::string sql_statment);

  size_t get_sql_statment_len() const;

  void set_sql_statment_len(size_t sql_statment_len);

  ~QueryEvent() override;

  QueryEvent(std::string dbname, std::string sql_statment);

  QueryEvent();

  size_t flush_to_buff(unsigned char* data) override;

  void deserialize(unsigned char* buff) override;

  std::string print_event_info() override;

private:
  uint32_t _thread_id;
  uint32_t _query_exec_time;
  size_t _db_len;
  size_t _sql_statment_len;
  uint16_t _error_code;
  uint16_t _status_var_len;

  std::string _status_vars;
  std::string _dbname;
  std::string _sql_statment;
};

/*
+=========================================================+
| fixed   |                                               |
| part    |                                               |
+=========================================================+
| variable| xid                 0 : 8                     | xid
| part    | column_count        8 : x                     | 表中字段数量
+=========================================================+
 */

class XidEvent : public ObLogEvent {
public:
  XidEvent() = default;
  ~XidEvent() = default;
  size_t flush_to_buff(unsigned char* data) override;
  uint64_t get_xid() const;
  void set_xid(uint64_t xid);
  size_t get_column_count() const;
  void set_column_count(size_t column_count);
  void deserialize(unsigned char* buff) override;
  std::string print_event_info() override;

private:
  uint64_t _xid;
  size_t _column_count;
};

/*

 +======================================+
| fixed   | table_id          0 : 6    | 表id
| part    +----------------------------+
|         | flags             6 : 2    |
+======================================+
|         | db_name                    | 数据库名,1个字节表示数据库名长度+数据库名字符串
|         +----------------------------+
| variable| tb_name                    | 表名，1字节表示表名长度+表名字符串
| part    +----------------------------+
|         | column_count               | 表中字段数量
|         +----------------------------+
|         | column_type                | 字段类型，一个字段占用1个字节
|         +----------------------------+
|         | metadata_len               | 对应字段的元数据信息长度
|         +----------------------------+
|         | metadata                   | 每个字段的元数据信息
|         +----------------------------+
|         | null_bits                  | 标识字段是否可为NULL，一个bit表示1个字段。低位->高位
|         +----------------------------+
|         | check_sum                  | 校验和，4字节
+======================================+

 */

class TableMapEvent : public ObLogEvent {
public:
  TableMapEvent() = default;
  ~TableMapEvent();
  size_t flush_to_buff(unsigned char* data) override;
  uint64_t get_table_id() const;
  void set_table_id(uint64_t table_id);
  uint16_t get_flags() const;
  void set_flags(uint16_t flags);
  std::string get_db_name();
  void set_db_name(std::string db_name);
  std::string get_tb_name();
  void set_tb_name(std::string tb_name);
  size_t get_column_count() const;
  void set_column_count(size_t column_count);
  const unsigned char* get_column_type() const;
  void set_column_type(unsigned char* column_type);
  size_t get_metadata_len() const;
  void set_metadata_len(size_t metadata_len);
  const unsigned char* get_metadata() const;
  void set_metadata(unsigned char* metadata);
  const unsigned char* get_null_bits() const;
  void set_null_bits(unsigned char* null_bits);
  size_t get_db_len() const;
  void set_db_len(size_t db_len);
  size_t get_tb_len() const;
  void set_tb_len(size_t tb_len);
  void deserialize(unsigned char* buff) override;
  std::string print_event_info() override;

private:
  uint64_t _table_id;
  uint16_t _flags;

  std::string _db_name;
  // len(_db_name)
  size_t _db_len;
  std::string _tb_name;
  // len(_tb_name)
  size_t _tb_len;
  size_t _column_count;
  unsigned char* _column_type;
  size_t _metadata_len;
  unsigned char* _metadata;
  unsigned char* _null_bits;
};

enum RowsEventType { INSERT, DELETE, UPDATE };
/*
 +======================================+
| fixed   | table_id          0 : 6    | 表id
| part    +----------------------------+
|         | flags             6 : 2    |
+======================================+
|         | var_header_len             | 2字节
|         +----------------------------+
| variable| width                      | 表中字段数量
| part    +----------------------------+
|         | before_image_cols          | before image中字段数量
|         +----------------------------+
|         | after_image_cols           | after image中字段数量
|         +----------------------------+
|         | columns_before_bitmaps     | 表示列是否在变更的images中，一个bit表示一个字段
|         +----------------------------+
|         | before_row                 | 变更前字段值，按照顺序输出字段值
|         +----------------------------+
|         | columns_after_bitmaps      | 表示列是否在变更的images中，一个bit表示一个字段
|         +----------------------------+
|         | after_row                  | 变更后字段值，按照顺序输出字段值
|         +----------------------------+
|         | check_sum                  | 校验和，4字节
+======================================+
 */
class RowsEvent : public ObLogEvent {
public:
  uint64_t get_table_id() const;
  void set_table_id(uint64_t table_id);
  uint16_t get_flags() const;
  void set_flags(uint16_t flags);
  uint16_t get_var_header_len() const;
  void set_var_header_len(uint16_t var_header_len);
  size_t get_width() const;
  void set_width(size_t width);
  size_t get_before_image_cols() const;
  void set_before_image_cols(size_t before_image_cols);
  size_t get_after_image_cols() const;
  void set_after_image_cols(size_t after_image_cols);
  const unsigned char* get_columns_before_bitmaps() const;
  void set_columns_before_bitmaps(unsigned char* columns_before_bitmaps);
  const unsigned char* get_columns_after_bitmaps() const;
  void set_columns_after_bitmaps(unsigned char* columns_after_bitmaps);
  MsgBuf& get_before_row();
  void set_before_row(MsgBuf& before_row);
  MsgBuf& get_after_row();
  void set_after_row(MsgBuf& after_row);
  RowsEvent(uint64_t table_id, uint16_t flags);
  RowsEvent(uint64_t table_id, uint16_t flags, RowsEventType rows_event_type);
  size_t flush_to_buff(unsigned char* data) override;
  RowsEventType get_rows_event_type() const;
  void set_rows_event_type(RowsEventType rows_event_type);
  size_t get_before_pos() const;
  void set_before_pos(size_t before_pos);
  size_t get_after_pos() const;
  void set_after_pos(size_t after_pos);
  virtual void deserialize(unsigned char* buff) = 0;
  std::string print_event_info() override;
  virtual ~RowsEvent();

private:
  uint64_t _table_id = 0;
  // STMT_END_F = (1U << 0),
  uint16_t _flags = (1U << 0);
  uint16_t _var_header_len = 0;
  size_t _width = 0;
  // (col_count + 7) / 8 bytes
  size_t _before_image_cols = 0;
  size_t _after_image_cols = 0;
  unsigned char* _columns_before_bitmaps = nullptr;
  unsigned char* _columns_after_bitmaps = nullptr;
  MsgBuf _before_row;
  MsgBuf _after_row;
  size_t _before_pos = 0;
  size_t _after_pos = 0;
  RowsEventType _rows_event_type;
};

/*

 +======================================+
| fixed   | table_id          0 : 6    | 表id
| part    +----------------------------+
|         | flags             6 : 2    |
+======================================+
|         | var_header_len             | 2字节
|         +----------------------------+
|         | width                      | 表中字段数量
|         +----------------------------+
| variable| after_image_cols          | 字段数量
| part    +----------------------------+
|         | columns_after_bitmaps      | 表示列是否在变更的images中，一个bit表示一个字段
|         +----------------------------+
|         | after_row                  | 字段值，按照顺序输出字段值
|         +----------------------------+
|         | check_sum                  | 校验和，4字节
+======================================+

 */
class WriteRowsEvent : public RowsEvent {
public:
  virtual ~WriteRowsEvent();
  WriteRowsEvent(uint64_t table_id, uint16_t flags);
  //  size_t flush_to_buff(unsigned char* data) override;
  void deserialize(unsigned char* buff) override;
};

/*

 +======================================+
| fixed   | table_id          0 : 6    | 表id
| part    +----------------------------+
|         | flags             6 : 2    |
+======================================+
|         | var_header_len             | 表中字段长度
|         +----------------------------+
| variable| width                      | 表中字段数量
| part    +----------------------------+
|         | before_image_cols          | before image中字段数量
|         +----------------------------+
|         | after_image_cols           | after image中字段数量
|         +----------------------------+
|         | columns_before_bitmaps     | 表示列是否在变更的images中，一个bit表示一个字段
|         +----------------------------+
|         | before_row                 | 变更前字段值，按照顺序输出字段值
|         +----------------------------+
|         | columns_after_bitmaps      | 表示列是否在变更的images中，一个bit表示一个字段
|         +----------------------------+
|         | after_row                  | 变更后字段值，按照顺序输出字段值
|         +----------------------------+
|         | check_sum                  | 校验和，4字节
+======================================+

 */
class UpdateRowsEvent : public RowsEvent {
public:
  virtual ~UpdateRowsEvent();
  UpdateRowsEvent(uint64_t table_id, uint16_t flags);
  //  size_t flush_to_buff(unsigned char* data) override;
  void deserialize(unsigned char* buff) override;
};

/*
 +======================================+
| fixed   | table_id          0 : 6    | 表id
| part    +----------------------------+
|         | flags             6 : 2    |
+======================================+
|         | var_header_len             | 表中字段长度
|         +----------------------------+
| variable| width                      | 表中字段数量
| part    +----------------------------+
|         | before_image_cols          | before image中字段数量
|         +----------------------------+
|         | columns_before_bitmaps     | 表示列是否在变更的images中，一个bit表示一个字段
|         +----------------------------+
|         | before_row                 | 变更前字段值，按照顺序输出字段值
|         +----------------------------+
|         | check_sum                  | 校验和，4字节
+======================================+
 */
class DeleteRowsEvent : public RowsEvent {
public:
  virtual ~DeleteRowsEvent();
  DeleteRowsEvent(uint64_t table_id, uint16_t flags);
  //  size_t flush_to_buff(unsigned char* data) override;
  void deserialize(unsigned char* buff) override;
};

/*
+==========================================+
| variable| binlog_file_name          | 当前binlog的文件名
| part    |
+==========================================+
 */
class HeartbeatEvent : public ObLogEvent {
public:
  HeartbeatEvent(std::string binlog_file_name, uint64_t pos);
  const std::string& get_binlog_file_name() const;
  void set_binlog_file_name(const std::string& binlog_file_name);
  size_t flush_to_buff(unsigned char* data) override;
  void deserialize(unsigned char* buff) override;
  std::string print_event_info() override;

private:
  std::string _binlog_file_name;
};

size_t write_rows(unsigned char* buff, size_t pos, MsgBuf& rows, size_t len);

int64_t seek_gtid_event(const std::string& binlog, int64_t& record_num, std::vector<GtidLogEvent*>& gtid_log_events,
    bool& existed, int64_t& last_txn_record_num, uint8_t& checksum_flag);

int get_the_last_complete_txn(const std::string& binlog, uint64_t& complete_transaction_pos, uint64_t& last_complete_txn_id,
    uint64_t& start_complete_txn_id, bool& rotate_existed);

/*!
   * Verify whether the event is complete based on the checksum
   * @param event
   * @param checksum_flag
   * @return
 */
int verify_event_crc32(unsigned char* event, uint64_t len, uint8_t checksum_flag);

int seek_events(const std::string& binlog, std::vector<ObLogEvent*>& log_events, EventType event_type = ENUM_END_EVENT,
    bool single = false);

}  // namespace logproxy
}  // namespace oceanbase
