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

#include "ob_log_event.h"
#include "obaccess/ob_mysql_packet.h"
#include <utility>
#include <cstring>
#include <cassert>
#include <zlib.h>
#include "str.h"
#include "common_util.h"
#include "guard.hpp"
#include "binlog_func.h"
namespace oceanbase {
namespace logproxy {
OblogEventHeader::OblogEventHeader(
    EventType type_code, uint64_t timestamp, uint32_t event_length, uint32_t next_position)
    : _flags(1), _server_id(SERVER_ID)
{
  this->_type_code = type_code;
  this->_timestamp = timestamp;
  this->_event_length = event_length;
  this->_next_position = next_position;
  OMS_DEBUG(
      "type:{}, timestamp:{}, event_length:{},next_position:{}", type_code, timestamp, event_length, next_position);
}

OblogEventHeader::OblogEventHeader(EventType type_code) : _type_code(type_code)
{}
size_t OblogEventHeader::flush_to_buff(unsigned char* header)
{
  // decode _header
  int4store(header, this->get_timestamp());

  int1store(header + TYPE_CODE_OFFSET, this->get_type_code());

  int4store(header + SERVER_ID_OFFSET, this->get_server_id());

  int4store(header + EVENT_LEN_OFFSET, this->get_event_length());

  int4store(header + LOG_POS_OFFSET, this->get_next_position());

  int2store(header + FLAGS_OFFSET, this->get_flags());

  return COMMON_HEADER_LENGTH;
}
uint64_t OblogEventHeader::get_timestamp() const
{
  return _timestamp;
}
void OblogEventHeader::set_timestamp(uint64_t timestamp)
{
  _timestamp = timestamp;
}
EventType OblogEventHeader::get_type_code() const
{
  return _type_code;
}
void OblogEventHeader::set_type_code(EventType type_code)
{
  _type_code = type_code;
}
uint32_t OblogEventHeader::get_server_id() const
{
  return _server_id;
}
void OblogEventHeader::set_server_id(uint32_t server_id)
{
  _server_id = server_id;
}
uint32_t OblogEventHeader::get_event_length()
{
  return _event_length;
}
void OblogEventHeader::set_event_length(uint32_t event_length)
{
  _event_length = event_length;
}
uint32_t OblogEventHeader::get_next_position() const
{
  return _next_position;
}
void OblogEventHeader::set_next_position(uint32_t next_position)
{
  _next_position = next_position;
}
uint16_t OblogEventHeader::get_flags() const
{
  return _flags;
}
void OblogEventHeader::set_flags(uint16_t flags)
{
  _flags = flags;
}
void OblogEventHeader::deserialize(unsigned char* buff)
{
  this->set_timestamp(int4load(buff));

  this->set_type_code(static_cast<EventType>(int1load(buff + TYPE_CODE_OFFSET)));

  this->set_server_id(int4load(buff + SERVER_ID_OFFSET));

  this->set_event_length(int4load(buff + EVENT_LEN_OFFSET));

  this->set_next_position(int4load(buff + LOG_POS_OFFSET));

  this->set_flags(int2load(buff + FLAGS_OFFSET));
}

std::string OblogEventHeader::str_format()
{
  std::stringstream format;
  format << "[event type:" << this->get_type_code() << "]";
  format << "[offset pos:" << this->get_next_position() - this->get_event_length() << "]";
  format << "[end pos:" << this->get_next_position() << "]";
  format << "[len:" << this->get_event_length() << "]";
  return format.str();
}

OblogEventHeader* ObLogEvent::get_header()
{
  return _header;
}
void ObLogEvent::set_header(OblogEventHeader* header)
{
  ObLogEvent::_header = header;
}

ObLogEvent::~ObLogEvent()
{
  delete (this->get_header());
}

const std::string& ObLogEvent::get_ob_txn() const
{
  return _ob_txn;
}

void ObLogEvent::set_ob_txn(const std::string& ob_txn)
{
  _ob_txn = ob_txn;
}

uint64_t ObLogEvent::get_checkpoint() const
{
  return _checkpoint;
}

void ObLogEvent::set_checkpoint(uint64_t checkpoint)
{
  _checkpoint = checkpoint;
}

uint8_t ObLogEvent::get_checksum_flag() const
{
  return _checksum_flag;
}

void ObLogEvent::set_checksum_flag(uint8_t checksum_flag)
{
  this->_checksum_flag = checksum_flag;
}

/**
 *
 * @param buff
 * @param pos
 * @param checksum
 * @return
 *
 * TODO
 */
size_t ObLogEvent::write_checksum(unsigned char* buff, size_t& pos) const
{
  if (_checksum_flag == CRC32) {
    uint64_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, buff, pos);
    int4store(buff + pos, crc);
    pos += 4;
  }
  return pos;
}

uint8_t ObLogEvent::get_checksum_len() const
{
  if (_checksum_flag != OFF) {
    return COMMON_CHECKSUM_LENGTH;
  }
  return 0;
}

std::string ObLogEvent::str_format()
{
  if (_header == nullptr) {
    return "";
  }
  std::stringstream format;
  format << "[event type:" << _header->get_type_code() << "]";
  format << "[offset pos:" << _header->get_next_position() - _header->get_event_length() << "]";
  format << "[end pos:" << _header->get_next_position() << "]";
  format << "[len:" << _header->get_event_length() << "]";
  return format.str();
}

ObLogEvent::ObLogEvent()
{
  // If the crc32 check is enabled, the checksum must be set for each event
  if (Config::instance().binlog_checksum.val()) {
    this->set_checksum_flag(CRC32);
  }
}

FormatDescriptionEvent::FormatDescriptionEvent(std::string server_version, uint64_t timestamp, uint8_t header_length,
    std::vector<uint8_t> event_type_header_lengths)
{
  this->_event_type_header_len = std::move(event_type_header_lengths);
  this->_header_len = header_length;
  this->_server_version = std::move(server_version);
  auto* common_header = new OblogEventHeader();
  common_header->set_type_code(FORMAT_DESCRIPTION_EVENT);
  common_header->set_flags(LOG_EVENT_BINLOG_IN_USE_F);
  this->set_header(common_header);
}

FormatDescriptionEvent::FormatDescriptionEvent(uint64_t timestamp, uint32_t server_id)
    : _header_len(COMMON_HEADER_LENGTH), _server_version(SERVER_VERSION), _version(BINLOG_VERSION)
{
  // common _header part
  auto* common_header = new OblogEventHeader(FORMAT_DESCRIPTION_EVENT);
  common_header->set_server_id(server_id);
  common_header->set_timestamp(timestamp);
  common_header->set_flags(LOG_EVENT_BINLOG_IN_USE_F);
  // fixed part
  this->_timestamp = timestamp;
  static uint8_t server_event_header_length[] = {START_V3_HEADER_LEN,
      QUERY_HEADER_LEN,
      STOP_HEADER_LEN,
      ROTATE_HEADER_LEN,
      INTVAR_HEADER_LEN,
      LOAD_HEADER_LEN,
      0,
      CREATE_FILE_HEADER_LEN,
      APPEND_BLOCK_HEADER_LEN,
      EXEC_LOAD_HEADER_LEN,
      DELETE_FILE_HEADER_LEN,
      NEW_LOAD_HEADER_LEN,
      RAND_HEADER_LEN,
      USER_VAR_HEADER_LEN,
      FORMAT_DESCRIPTION_HEADER_LEN,
      XID_HEADER_LEN,
      BEGIN_LOAD_QUERY_HEADER_LEN,
      EXECUTE_LOAD_QUERY_HEADER_LEN,
      TABLE_MAP_HEADER_LEN,
      0,                  /* PRE_GA_WRITE_ROWS_EVENT */
      0,                  /* PRE_GA_UPDATE_ROWS_EVENT*/
      0,                  /* PRE_GA_DELETE_ROWS_EVENT*/
      ROWS_HEADER_LEN_V1, /* WRITE_ROWS_EVENT_V1*/
      ROWS_HEADER_LEN_V1, /* UPDATE_ROWS_EVENT_V1*/
      ROWS_HEADER_LEN_V1, /* DELETE_ROWS_EVENT_V1*/
      INCIDENT_HEADER_LEN,
      0, /* HEARTBEAT_LOG_EVENT*/
      IGNORABLE_HEADER_LEN,
      IGNORABLE_HEADER_LEN,
      ROWS_HEADER_LEN_V2,
      ROWS_HEADER_LEN_V2,
      ROWS_HEADER_LEN_V2,
      GTID_HEADER_LEN, /*GTID_EVENT*/
      GTID_HEADER_LEN, /*ANONYMOUS_GTID_EVENT*/
      IGNORABLE_HEADER_LEN,
      TRANSACTION_CONTEXT_HEADER_LEN,
      VIEW_CHANGE_HEADER_LEN,
      XA_PREPARE_HEADER_LEN};
  this->_event_type_header_len.insert(
      _event_type_header_len.begin(), server_event_header_length, server_event_header_length + ENUM_END_EVENT - 1);

  // variable part,only _checksum
  //  this->set_checksum(new ObLogEventChecksumCrc(0));

  // len of current event,len(Event Header)+len(Event Data)+len(check sum)
  // this->set_checksum_flag(CRC32);
  uint32_t event_len =
      get_checksum_len() + 1 + this->_header_len + this->_event_type_header_len[FORMAT_DESCRIPTION_EVENT - 1];

  common_header->set_event_length(event_len);
  common_header->set_next_position(BINLOG_MAGIC_SIZE + event_len);
  this->set_header(common_header);
}

size_t FormatDescriptionEvent::flush_to_buff(unsigned char* buff)
{
  // 1.decode version
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;

  int2store(buff + pos, this->get_version());
  pos += 2;

  memset(buff + pos, '\0', ST_SERVER_VER_LEN);
  strncpy(reinterpret_cast<char*>(buff + pos), this->get_server_version().c_str(), ST_SERVER_VER_LEN - 1);
  pos += ST_SERVER_VER_LEN;

  int4store(buff + pos, this->get_timestamp());
  pos += 4;

  int1store(buff + pos, this->get_header_len());
  pos += 1;

  std::vector<uint8_t> event_type_header = this->get_event_type_header_len();
  std::vector<uint8_t>::iterator iterator;
  for (iterator = event_type_header.begin(); iterator != event_type_header.end(); ++iterator) {
    int1store(buff + pos, *iterator);
    pos += 1;
  }
  //  pos += EVENT_TYPE_HEADER_LEN;
  int1store(buff + pos, this->get_checksum_flag());
  pos += 1;

  /*!
   * @brief This is the FormatDescriptionEvent, we need to clear the
   * LOG_EVENT_BINLOG_IN_USE_F flag before computing the checksum,
   * Because the flag will be cleared when the binlog is closed. The flag is discarded before the checksum is calculated
   * also.
   */
  if ((this->get_header()->get_flags() & LOG_EVENT_BINLOG_IN_USE_F) != 0) {
    int2store(buff + FLAGS_OFFSET, 0);
  }

  // Regardless of whether the crc32 check is enabled or not, the FormatDescriptionEvent needs to fill in the checksum
  write_checksum(buff, pos);

  /*!
   * @brief After doing the checksum, we need to restore the flag parameter
   */
  if ((this->get_header()->get_flags() & LOG_EVENT_BINLOG_IN_USE_F) != 0) {
    int2store(buff + FLAGS_OFFSET, this->get_header()->get_flags());
  }
  return pos;
}
uint16_t FormatDescriptionEvent::get_version() const
{
  return _version;
}
void FormatDescriptionEvent::set_version(uint16_t version)
{
  _version = version;
}
const std::string& FormatDescriptionEvent::get_server_version() const
{
  return _server_version;
}

void FormatDescriptionEvent::set_server_version(std::string server_version)
{
  _server_version = std::move(server_version);
}

uint64_t FormatDescriptionEvent::get_timestamp() const
{
  return _timestamp;
}
void FormatDescriptionEvent::set_timestamp(uint64_t timestamp)
{
  _timestamp = timestamp;
}
uint8_t FormatDescriptionEvent::get_header_len() const
{
  return _header_len;
}
void FormatDescriptionEvent::set_header_len(uint8_t header_len)
{
  _header_len = header_len;
}
std::vector<uint8_t> FormatDescriptionEvent::get_event_type_header_len() const
{
  return _event_type_header_len;
}

void FormatDescriptionEvent::set_event_type_header_len(const std::vector<uint8_t>& event_type_header_len)
{
  _event_type_header_len = event_type_header_len;
}

void FormatDescriptionEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_version(int2load(buff + pos));
  pos += 2;

  // TODO
  char* server_version = static_cast<char*>(malloc(ST_SERVER_VER_LEN));
  memcpy(server_version, buff + pos, ST_SERVER_VER_LEN);
  this->set_server_version(server_version);
  pos += ST_SERVER_VER_LEN;
  free(server_version);

  this->set_timestamp(int4load(buff + pos));
  pos += 4;

  this->set_header_len(int1load(buff + pos));
  pos += 1;

  std::vector<uint8_t> event_type_header_len;
  assert(pos + LOG_EVENT_TYPES <= header->get_event_length());

  for (int i = 0; i < LOG_EVENT_TYPES; ++i) {
    event_type_header_len.emplace_back(int1load(buff + pos));
    pos += 1;
  }
  this->set_event_type_header_len(event_type_header_len);

  this->set_checksum_flag(int1load(buff + pos));
  pos += 5;
}

std::string FormatDescriptionEvent::print_event_info()
{
  std::stringstream info;
  info << "Server ver: " << this->get_server_version();
  info << ", Binlog ver: " << this->get_version();
  return info.str();
}

PreviousGtidsLogEvent::PreviousGtidsLogEvent(
    uint32_t gtid_uuid_num, const std::vector<GtidMessage*>& gtid_messages, uint64_t timestamp)
{
  this->set_gtid_messages(gtid_messages);
  this->set_gtid_uuid_num(gtid_uuid_num);

  size_t header_len = COMMON_HEADER_LENGTH;

  // default val 1
  header_len += 8;

  std::map<std::string, GtidMessage*> gtid_msg = this->get_gtid_messages();
  assert(this->get_gtid_uuid_num() == gtid_msg.size());
  for (auto const& gtid_it : gtid_msg) {
    GtidMessage* msg = gtid_it.second;
    header_len += SERVER_UUID_LEN;
    header_len += 8;

    for (auto const& txn_range : msg->get_txn_range()) {
      OMS_STREAM_INFO << "gtid:[" << txn_range.first << "," << txn_range.second << ")";
      header_len += 16;
    }
  }

  header_len += get_checksum_len();
  this->set_header(new OblogEventHeader(PREVIOUS_GTIDS_LOG_EVENT, timestamp, header_len, 0));
}
size_t PreviousGtidsLogEvent::flush_to_buff(unsigned char* buff)
{
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;

  // default val 1
  int8store(buff + pos, this->get_gtid_uuid_num());
  pos += 8;

  std::map<std::string, GtidMessage*> gtid_msg = this->get_gtid_messages();
  assert(this->get_gtid_uuid_num() == gtid_msg.size());

  for (auto const& gtid_it : gtid_msg) {
    GtidMessage* msg = gtid_it.second;
    //    assert(msg.get_gtid_uuid().size() == SERVER_UUID_LEN);
    memcpy(buff + pos, msg->get_gtid_uuid(), SERVER_UUID_LEN);
    pos += SERVER_UUID_LEN;

    int8store(buff + pos, msg->get_gtid_txn_id_intervals());
    pos += 8;

    for (auto const& txn_range : msg->get_txn_range()) {
      int8store(buff + pos, txn_range.first);
      pos += 8;

      int8store(buff + pos, txn_range.second);
      pos += 8;
    }
  }
  return write_checksum(buff, pos);
}

uint64_t PreviousGtidsLogEvent::get_gtid_uuid_num() const
{
  return _gtid_uuid_num;
}

void PreviousGtidsLogEvent::set_gtid_uuid_num(uint64_t gtid_uuid_num)
{
  _gtid_uuid_num = gtid_uuid_num;
}

std::map<std::string, GtidMessage*>& PreviousGtidsLogEvent::get_gtid_messages()
{
  return _gtid_messages;
}

void PreviousGtidsLogEvent::set_gtid_messages(const std::vector<GtidMessage*>& gtid_messages)
{
  for (GtidMessage* gtid_message : gtid_messages) {
    std::string uuid_str;
    logproxy::dumphex(reinterpret_cast<const char*>(gtid_message->get_gtid_uuid()), SERVER_UUID_LEN, uuid_str);
    // Assemble GTID information
    OMS_STREAM_INFO << "assemble GTID information " << uuid_str;
    _gtid_messages.emplace(uuid_str, gtid_message);
  }
}

void PreviousGtidsLogEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);

  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_gtid_uuid_num(int8load(buff + pos));
  pos += 8;

  auto* uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
  memcpy(uuid, buff + pos, SERVER_UUID_LEN);
  pos += SERVER_UUID_LEN;

  std::vector<GtidMessage*> gtid_messages;
  auto* gtid_message = new GtidMessage();
  gtid_message->set_gtid_uuid(uuid);
  gtid_message->set_gtid_txn_id_intervals(int8load(buff + pos));
  pos += 8;

  std::vector<txn_range> txn_vector;

  for (uint64_t i = 0; i < gtid_message->get_gtid_txn_id_intervals(); ++i) {
    uint64_t txn_start = int8load(buff + pos);
    pos += 8;
    uint64_t txn_end = int8load(buff + pos);
    pos += 8;
    OMS_STREAM_INFO << "txn_start:" << txn_start << " txn_end:" << txn_end;
    txn_range range;
    range.first = txn_start;
    range.second = txn_end;
    txn_vector.emplace_back(range);
  }

  gtid_message->set_txn_range(txn_vector);
  OMS_STREAM_INFO << "gtid uuid" << gtid_message->format_string();

  gtid_messages.emplace_back(gtid_message);

  this->set_gtid_messages(gtid_messages);
  pos += get_checksum_len();
}

PreviousGtidsLogEvent::~PreviousGtidsLogEvent()
{
  for (auto gtid_message : _gtid_messages) {
    delete gtid_message.second;
    gtid_message.second = nullptr;
  }
}

std::string PreviousGtidsLogEvent::print_event_info()
{
  std::stringstream info;
  int count = 0;
  for (const auto& gtid_message : this->get_gtid_messages()) {
    if (count > 0) {
      info << std::endl;
    }

    /*
     * For Previous Gtids Log Event, it is an interval that is closed on the left and open on the right, so it needs to
     * be revised first when serializing the display.
     */
    auto temp = *gtid_message.second;
    for (auto& i : temp.get_txn_range()) {
      if (i.second != i.first) {
        i.second -= 1;
      }
    }
    info << temp.format_string();
    count++;
  }
  return info.str();
}

RotateEvent::RotateEvent(
    uint64_t next_binlog_position, std::string next_binlog_file_name, uint64_t timestamp, uint32_t begin_position)
    : _next_binlog_position(next_binlog_position), _next_binlog_file_name(std::move(next_binlog_file_name))
{
  uint32_t event_len = COMMON_HEADER_LENGTH + ROTATE_HEADER_LEN + _next_binlog_file_name.length() + get_checksum_len();
  uint32_t end_position = begin_position + event_len;
  this->set_header(new OblogEventHeader(ROTATE_EVENT, timestamp, event_len, end_position));
}

size_t RotateEvent::flush_to_buff(unsigned char* buff)
{
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;

  int8store(buff + pos, this->get_next_binlog_position());
  pos += 8;

  memcpy(buff + pos, this->get_next_binlog_file_name().c_str(), this->get_next_binlog_file_name().size());
  pos += this->get_next_binlog_file_name().size();
  return write_checksum(buff, pos);
}
uint64_t RotateEvent::get_next_binlog_position() const
{
  return _next_binlog_position;
}
void RotateEvent::set_next_binlog_position(uint64_t next_binlog_position)
{
  _next_binlog_position = next_binlog_position;
}
const std::string& RotateEvent::get_next_binlog_file_name() const
{
  return _next_binlog_file_name;
}
void RotateEvent::set_next_binlog_file_name(std::string next_binlog_file_name)
{
  _next_binlog_file_name = std::move(next_binlog_file_name);
}

RotateEvent::OP RotateEvent::get_op() const
{
  return _op;
}

void RotateEvent::set_op(RotateEvent::OP op)
{
  _op = op;
}
uint64_t RotateEvent::get_index() const
{
  return _index;
}
void RotateEvent::set_index(uint64_t index)
{
  _index = index;
}

void RotateEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_next_binlog_position(int8load(buff + pos));
  pos += 8;

  uint64_t binlog_file_name_len = header->get_event_length() - get_checksum_len() - pos;

  std::string binlog_file_name(reinterpret_cast<char*>(buff) + pos, binlog_file_name_len);

  this->set_next_binlog_file_name(binlog_file_name);
  this->set_op(ROTATE);
  pos += binlog_file_name_len;

  pos += get_checksum_len();
  assert(pos == header->get_event_length());
}

bool RotateEvent::is_existed() const
{
  return existed;
}

void RotateEvent::set_existed(bool existed)
{
  RotateEvent::existed = existed;
}

std::string RotateEvent::print_event_info()
{
  std::stringstream info;
  info << "Binlog Position: " << this->get_next_binlog_position();
  info << ", Log name: " << this->get_next_binlog_file_name();
  return info.str();
}

QueryEvent::QueryEvent(std::string dbname, std::string sql_statment)
    : _dbname(dbname), _sql_statment(std::move(sql_statment))
{
  this->_db_len = dbname.size();
  this->_error_code = 0;
}
size_t QueryEvent::flush_to_buff(unsigned char* buff)
{
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;
  int4store(buff + pos, this->get_thread_id());
  pos += 4;

  int4store(buff + pos, this->get_query_exec_time());
  pos += 4;

  int1store(buff + pos, this->get_db_len());
  pos += 1;

  int2store(buff + pos, this->get_error_code());
  pos += 2;

  int2store(buff + pos, this->get_status_var_len());
  pos += 2;

  // body
  memcpy(buff + pos, this->get_status_vars().c_str(), this->get_status_var_len());
  pos += this->get_status_var_len();

  // db name
  char* dbname_nts = static_cast<char*>(malloc(this->get_db_len() + 1));
  write_null_terminate_string(dbname_nts, this->get_db_len() + 1, this->get_dbname());
  memcpy(buff + pos, dbname_nts, this->get_db_len() + 1);
  pos += this->get_db_len() + 1;
  //  OMS_STREAM_DEBUG << "dbname:" << dbname_nts << "len:" << this->get_db_len() + 1;
  free(dbname_nts);
  // sql statment
  memcpy(buff + pos, this->get_sql_statment().c_str(), this->get_sql_statment_len());
  pos += this->get_sql_statment_len();
  //  OMS_STREAM_DEBUG << "sql statment:" << this->get_sql_statment() << "len:" << this->get_sql_statment_len();

  // _checksum
  return write_checksum(buff, pos);
}
uint32_t QueryEvent::get_thread_id() const
{
  return _thread_id;
}
void QueryEvent::set_thread_id(uint32_t thread_id)
{
  _thread_id = thread_id;
}
uint32_t QueryEvent::get_query_exec_time() const
{
  return _query_exec_time;
}
void QueryEvent::set_query_exec_time(uint32_t query_exec_time)
{
  _query_exec_time = query_exec_time;
}
size_t QueryEvent::get_db_len() const
{
  return _db_len;
}
void QueryEvent::set_db_len(size_t db_len)
{
  _db_len = db_len;
}
uint16_t QueryEvent::get_error_code() const
{
  return _error_code;
}
void QueryEvent::set_error_code(uint16_t error_code)
{
  _error_code = error_code;
}
uint16_t QueryEvent::get_status_var_len() const
{
  return _status_var_len;
}
void QueryEvent::set_status_var_len(uint16_t status_var_len)
{
  _status_var_len = status_var_len;
}
size_t QueryEvent::get_sql_statment_len() const
{
  return _sql_statment_len;
}
void QueryEvent::set_sql_statment_len(size_t sql_statment_len)
{
  _sql_statment_len = sql_statment_len;
}
QueryEvent::~QueryEvent()
{}

void QueryEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_thread_id(int4load(buff + pos));
  pos += 4;

  this->set_query_exec_time(int4load(buff + pos));
  pos += 4;

  this->set_db_len(int1load(buff + pos));
  pos += 1;

  this->set_error_code(int2load(buff + pos));
  pos += 2;

  this->set_status_var_len(int2load(buff + pos));
  pos += 2;

  char* status_var = static_cast<char*>(malloc(this->get_status_var_len()));
  memcpy(status_var, buff + pos, this->get_status_var_len());
  pos += this->get_status_var_len();
  this->set_status_vars(status_var);
  free(status_var);

  char* db_name = static_cast<char*>(malloc(this->get_db_len() + 1));
  memcpy(db_name, buff + pos, this->get_db_len() + 1);
  this->set_dbname(db_name);
  pos += this->get_db_len() + 1;
  free(db_name);

  uint64_t sql_statment_len = header->get_event_length() - get_checksum_len() - pos;

  char* sql_statment = static_cast<char*>(malloc(sql_statment_len));
  memcpy(sql_statment, buff + pos, sql_statment_len);
  pos += sql_statment_len;
  this->set_sql_statment_len(sql_statment_len);
  this->set_sql_statment(std::string(sql_statment, sql_statment_len));
  free(sql_statment);

  pos += get_checksum_len();
  assert(pos == header->get_event_length());
}
const std::string& QueryEvent::get_status_vars() const
{
  return _status_vars;
}
void QueryEvent::set_status_vars(std::string status_vars)
{
  _status_vars = std::move(status_vars);
}
const std::string& QueryEvent::get_dbname() const
{
  return _dbname;
}
void QueryEvent::set_dbname(std::string dbname)
{
  _dbname = std::move(dbname);
}
const std::string& QueryEvent::get_sql_statment() const
{
  return _sql_statment;
}
void QueryEvent::set_sql_statment(std::string sql_statment)
{
  _sql_statment = std::move(sql_statment);
}

std::string QueryEvent::print_event_info()
{
  std::stringstream info;
  if (this->get_sql_statment() == "BEGIN") {
    info << BEGIN_VAR;
  } else {
    info << "use " << this->get_dbname() << ";" << this->get_sql_statment();
  }
  return info.str();
}

QueryEvent::QueryEvent()
{}

GtidMessage::GtidMessage(unsigned char* gtid_uuid, uint64_t gtid_txn_id_intervals)
    : _gtid_uuid(gtid_uuid), _gtid_txn_id_intervals(gtid_txn_id_intervals)
{}

unsigned char* GtidMessage::get_gtid_uuid()
{
  return _gtid_uuid;
}

std::string GtidMessage::get_gtid_uuid_str()
{
  return std::string{reinterpret_cast<char*>(_gtid_uuid), SERVER_UUID_LEN};
}

void GtidMessage::set_gtid_uuid(unsigned char* gtid_uuid)
{
  _gtid_uuid = gtid_uuid;
}

uint64_t GtidMessage::get_gtid_txn_id_intervals()
{
  return _gtid_txn_id_intervals;
}

void GtidMessage::set_gtid_txn_id_intervals(uint64_t gtid_txn_id_intervals)
{
  _gtid_txn_id_intervals = gtid_txn_id_intervals;
}

std::vector<txn_range>& GtidMessage::get_txn_range()
{
  return _txn_range;
}

void GtidMessage::set_txn_range(std::vector<txn_range> txn_range)
{
  _txn_range = std::move(txn_range);
}

std::string GtidMessage::format_string()
{
  /*
   * 89fbcea2-db65-11e7-a851-fa163e618bac:1-5:999:1050-1052
    +-----+-----+-----+-----+-----+
    |4 bit|2 bit|2 bit|2 bit|6 bit|
    +-----+-----+-----+-----+-----+
   */
  //  unsigned char uuid_str[20];
  std::string uuid_str;
  std::stringstream stream;
  // uuid
  //  stream << this->_gtid_uuid.substr(0,4)<<"-"<< this->_gtid_uuid.substr(4,2);

  logproxy::dumphex(reinterpret_cast<const char*>(this->_gtid_uuid), SERVER_UUID_LEN, uuid_str);
  int pos = 0;
  std::string item_first = uuid_str.substr(pos, 8);
  transform(item_first.begin(), item_first.end(), item_first.begin(), ::tolower);
  stream << item_first << "-";
  pos += 8;

  std::string item_second = uuid_str.substr(pos, 4);
  transform(item_second.begin(), item_second.end(), item_second.begin(), ::tolower);
  stream << item_second << "-";
  pos += 4;

  std::string item_third = uuid_str.substr(pos, 4);
  transform(item_third.begin(), item_third.end(), item_third.begin(), ::tolower);
  stream << item_third << "-";
  pos += 4;

  std::string item_fourth = uuid_str.substr(pos, 4);
  transform(item_fourth.begin(), item_fourth.end(), item_fourth.begin(), ::tolower);
  stream << item_fourth << "-";
  pos += 4;

  std::string item_fifth = uuid_str.substr(pos, 12);
  transform(item_fifth.begin(), item_fifth.end(), item_fifth.begin(), ::tolower);
  stream << item_fifth;

  for (uint64_t i = 0; i < this->_txn_range.size(); ++i) {
    if (this->_txn_range.at(i).second != this->_txn_range.at(i).first) {
      stream << ":" << this->_txn_range.at(i).first << "-" << this->_txn_range.at(i).second;
    } else {
      stream << ":" << this->_txn_range.at(i).first;
    }
  }
  OMS_INFO("uuid string {}", stream.str());
  return stream.str();
}

int GtidMessage::set_gtid_uuid(const std::string& gtid_uuid)
{
  std::vector<std::string> parts;
  logproxy::split(gtid_uuid, '-', parts);
  if (parts.size() == 5) {
    this->_gtid_uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
    int pos = 0;
    binlog::CommonUtils::hex_to_bin(parts[0], this->_gtid_uuid + pos);
    pos += 4;
    binlog::CommonUtils::hex_to_bin(parts[1], this->_gtid_uuid + pos);
    pos += 2;
    binlog::CommonUtils::hex_to_bin(parts[2], this->_gtid_uuid + pos);
    pos += 2;
    binlog::CommonUtils::hex_to_bin(parts[3], this->_gtid_uuid + pos);
    pos += 2;
    binlog::CommonUtils::hex_to_bin(parts[4], this->_gtid_uuid + pos);
  } else {
    OMS_STREAM_ERROR << "Invalid server uuid:" << gtid_uuid;
    return OMS_FAILED;
  }
  return OMS_OK;
}

int GtidMessage::deserialize_gtid_set(const std::string& gtid_set)
{
  std::vector<std::string> parts;
  logproxy::split(gtid_set, ':', parts);
  if (parts.size() <= 1) {
    return OMS_FAILED;
  }

  if (set_gtid_uuid(parts[0]) != OMS_OK) {
    return OMS_FAILED;
  }

  for (size_t i = 1; i < parts.size(); ++i) {
    std::vector<std::string> txn_ranges;
    logproxy::split(parts[i], '-', txn_ranges);
    switch (txn_ranges.size()) {
      case 1: {
        txn_range range;
        range.first = atoll(txn_ranges.at(0).c_str());
        range.second = atoll(txn_ranges.at(0).c_str());
        _txn_range.emplace_back(range);
        break;
      }
      case 2: {
        txn_range range;
        range.first = atoll(txn_ranges.at(0).c_str());
        range.second = atoll(txn_ranges.at(1).c_str());
        _txn_range.emplace_back(range);
        break;
      }
      default:
        return OMS_FAILED;
    }
  }

  merge_txn_range(this, this);

  return OMS_OK;
}

GtidMessage::~GtidMessage()
{
  if (_gtid_uuid != nullptr) {
    free(_gtid_uuid);
    _gtid_uuid = nullptr;
  }
}

int GtidMessage::merge_txn_range(GtidMessage* message, GtidMessage* target)
{
  uint64_t start = 0, end = 0;
  std::vector<txn_range> merge;
  std::vector<txn_range> full_range;
  full_range.insert(full_range.end(), message->get_txn_range().begin(), message->get_txn_range().end());
  full_range.insert(full_range.end(), target->get_txn_range().begin(), target->get_txn_range().end());
  for (auto const& range : full_range) {
    if (start == 0) {
      start = range.first;
      end = range.second;
    } else {
      if (range.first <= end + 1) {
        end = std::max(end, range.second);
      } else {
        merge.emplace_back(start, end);
        start = range.first;
        end = range.second;
      }
    }
  }
  if (start != 0) {
    merge.emplace_back(start, end);
  }
  target->set_txn_range(merge);
  return OMS_OK;
}

size_t XidEvent::flush_to_buff(unsigned char* buff)
{
  this->get_header()->flush_to_buff(buff);

  size_t pos = COMMON_HEADER_LENGTH;
  // body
  int8store(buff + pos, this->get_xid());
  pos += 8;
  // no column_count
  return write_checksum(buff, pos);
}
uint64_t XidEvent::get_xid() const
{
  return _xid;
}
void XidEvent::set_xid(uint64_t xid)
{
  _xid = xid;
}
size_t XidEvent::get_column_count() const
{
  return _column_count;
}
void XidEvent::set_column_count(size_t column_count)
{
  _column_count = column_count;
}

void XidEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_xid(int8load(buff + pos));
  pos += 8;

  pos += get_checksum_len();
  assert(pos == header->get_event_length());
}

std::string XidEvent::print_event_info()
{
  std::stringstream info;
  info << "Xid ID=" << this->get_xid();
  return info.str();
}

uint64_t RowsEvent::get_table_id() const
{
  return _table_id;
}
void RowsEvent::set_table_id(uint64_t table_id)
{
  _table_id = table_id;
}
uint16_t RowsEvent::get_flags() const
{
  return _flags;
}
void RowsEvent::set_flags(uint16_t flags)
{
  _flags = flags;
}
uint16_t RowsEvent::get_var_header_len() const
{
  return _var_header_len;
}
void RowsEvent::set_var_header_len(uint16_t var_header_len)
{
  _var_header_len = var_header_len;
}
size_t RowsEvent::get_width() const
{
  return _width;
}
void RowsEvent::set_width(size_t width)
{
  _width = width;
}
size_t RowsEvent::get_before_image_cols() const
{
  return _before_image_cols;
}
void RowsEvent::set_before_image_cols(size_t before_image_cols)
{
  _before_image_cols = before_image_cols;
}
size_t RowsEvent::get_after_image_cols() const
{
  return _after_image_cols;
}
void RowsEvent::set_after_image_cols(size_t after_image_cols)
{
  _after_image_cols = after_image_cols;
}
const unsigned char* RowsEvent::get_columns_before_bitmaps() const
{
  return _columns_before_bitmaps;
}
void RowsEvent::set_columns_before_bitmaps(unsigned char* columns_before_bitmaps)
{
  _columns_before_bitmaps = columns_before_bitmaps;
}
const unsigned char* RowsEvent::get_columns_after_bitmaps() const
{
  return _columns_after_bitmaps;
}
void RowsEvent::set_columns_after_bitmaps(unsigned char* columns_after_bitmaps)
{
  _columns_after_bitmaps = columns_after_bitmaps;
}
MsgBuf& RowsEvent::get_before_row()
{
  return _before_row;
}
void RowsEvent::set_before_row(MsgBuf& before_row)
{
  _before_row = before_row;
}

MsgBuf& RowsEvent::get_after_row()
{
  return _after_row;
}
void RowsEvent::set_after_row(MsgBuf& after_row)
{
  _after_row = after_row;
}
RowsEvent::RowsEvent(uint64_t table_id, uint16_t flags) : _table_id(table_id), _flags(flags)
{}
size_t RowsEvent::flush_to_buff(unsigned char* buff)
{
  // common _header
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;
  // body
  int6store(buff + pos, this->get_table_id());
  pos += 6;
  int2store(buff + pos, this->get_flags());
  pos += 2;
  int2store(buff + pos, this->get_var_header_len());
  pos += 2;
  int ret = write_lenenc_uint(reinterpret_cast<char*>(buff + pos), MAX_PACKET_INTEGER_LEN, this->get_width());
  pos += ret;
  if (get_rows_event_type() != INSERT) {
    // before_image_cols
    auto* bit_map = fill_bit_map(this->get_before_image_cols());
    memcpy(buff + pos, bit_map, this->get_before_image_cols());
    pos += this->get_before_image_cols();
    free(bit_map);
  }
  if (get_rows_event_type() != DELETE) {
    // after_image_cols
    auto* bit_map = fill_bit_map(this->get_after_image_cols());
    memcpy(buff + pos, bit_map, this->get_after_image_cols());
    pos += this->get_after_image_cols();
    free(bit_map);
  }

  if (get_rows_event_type() != INSERT) {
    // columns_before_bitmaps
    assert(this->get_columns_before_bitmaps() != nullptr);
    memcpy(buff + pos, this->get_columns_before_bitmaps(), (this->get_width() + 7) / 8);
    pos += (this->get_width() + 7) / 8;

    auto before_row = this->get_before_row();
    pos = write_rows(buff, pos, before_row, this->get_before_pos());
  }

  if (get_rows_event_type() != DELETE) {
    // column_after_bitmaps
    assert(this->get_columns_after_bitmaps() != nullptr);
    //    OMS_STREAM_DEBUG << "row event type:" << get_rows_event_type() << " bytes:" << (this->get_width() + 7) / 8
    //              << " even len :" << this->get_header()->get_event_length();
    memcpy(buff + pos, this->get_columns_after_bitmaps(), (this->get_width() + 7) / 8);
    pos += (this->get_width() + 7) / 8;
    auto after_row = this->get_after_row();
    pos = write_rows(buff, pos, after_row, this->get_after_pos());
  }

  // add _checksum
  return write_checksum(buff, pos);
}
RowsEventType RowsEvent::get_rows_event_type() const
{
  return _rows_event_type;
}
void RowsEvent::set_rows_event_type(RowsEventType rows_event_type)
{
  _rows_event_type = rows_event_type;
}
RowsEvent::RowsEvent(uint64_t table_id, uint16_t flags, RowsEventType rows_event_type)
    : _table_id(table_id), _flags(flags), _rows_event_type(rows_event_type)
{}
size_t RowsEvent::get_before_pos() const
{
  return _before_pos;
}
void RowsEvent::set_before_pos(size_t before_pos)
{
  _before_pos = before_pos;
}
size_t RowsEvent::get_after_pos() const
{
  return _after_pos;
}
void RowsEvent::set_after_pos(size_t after_pos)
{
  _after_pos = after_pos;
}

RowsEvent::~RowsEvent()
{
  if (_columns_before_bitmaps != nullptr) {
    free(_columns_before_bitmaps);
    _columns_before_bitmaps = nullptr;
  }

  if (_columns_after_bitmaps != nullptr) {
    free(_columns_after_bitmaps);
    _columns_after_bitmaps = nullptr;
  }

  _before_row.reset();
  _after_row.reset();
}

std::string RowsEvent::print_event_info()
{
  std::stringstream info;
  info << "table_id: " << this->get_table_id() << ((this->get_flags() == 1) ? " flags: STMT_END_F" : "");
  return info.str();
}

WriteRowsEvent::WriteRowsEvent(uint64_t table_id, uint16_t flags) : RowsEvent(table_id, flags)
{
  this->set_rows_event_type(INSERT);
}
WriteRowsEvent::~WriteRowsEvent()
{}

void WriteRowsEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_table_id(int6load(buff + pos));
  pos += 6;

  this->set_flags(int2load(buff + pos));
  pos += 2;

  this->set_var_header_len(int2load(buff + pos));
  pos += 2;

  this->set_width(get_lenenc_uint(buff, pos));

  this->set_after_image_cols(this->get_width());
  pos += (this->get_width() + 7) / 8;

  auto* bitmaps = static_cast<unsigned char*>(malloc((this->get_width() + 7) / 8));
  memcpy(bitmaps, buff + pos, (this->get_width() + 7) / 8);
  this->set_columns_after_bitmaps(bitmaps);
  pos += (this->get_width() + 7) / 8;

  uint64_t after_row_bytes = header->get_event_length() - pos - get_checksum_len();
  auto* after_row = static_cast<unsigned char*>(malloc(after_row_bytes));
  memcpy(after_row, buff + pos, after_row_bytes);
  // table map event
  this->get_after_row().push_back(reinterpret_cast<char*>(after_row), after_row_bytes);
  pos += after_row_bytes;

  pos += get_checksum_len();
  assert(pos == header->get_event_length());
}

DeleteRowsEvent::DeleteRowsEvent(uint64_t table_id, uint16_t flags) : RowsEvent(table_id, flags)
{
  this->set_rows_event_type(DELETE);
}
DeleteRowsEvent::~DeleteRowsEvent()
{}

void DeleteRowsEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_table_id(int6load(buff + pos));
  pos += 6;

  this->set_flags(int2load(buff + pos));
  pos += 2;

  this->set_var_header_len(int2load(buff + pos));
  pos += 2;

  this->set_width(get_lenenc_uint(buff, pos));

  this->set_before_image_cols(this->get_width());
  pos += (this->get_width() + 7) / 8;

  auto* bitmaps = static_cast<unsigned char*>(malloc((this->get_width() + 7) / 8));
  memcpy(bitmaps, buff + pos, (this->get_width() + 7) / 8);
  this->set_columns_before_bitmaps(bitmaps);
  pos += (this->get_width() + 7) / 8;

  uint64_t before_row_bytes = header->get_event_length() - pos - get_checksum_len();
  auto* before_row = static_cast<unsigned char*>(malloc(before_row_bytes));
  memcpy(before_row, buff + pos, before_row_bytes);
  // table map event
  this->get_before_row().push_back(reinterpret_cast<char*>(before_row), before_row_bytes);
  pos += before_row_bytes;

  pos += get_checksum_len();
  assert(pos == header->get_event_length());
}

UpdateRowsEvent::UpdateRowsEvent(uint64_t table_id, uint16_t flags) : RowsEvent(table_id, flags)
{
  this->set_rows_event_type(UPDATE);
}
UpdateRowsEvent::~UpdateRowsEvent()
{}

void UpdateRowsEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_table_id(int6load(buff + pos));
  pos += 6;

  this->set_flags(int2load(buff + pos));
  pos += 2;

  this->set_var_header_len(int2load(buff + pos));
  pos += 2;

  this->set_width(get_lenenc_uint(buff, pos));

  this->set_after_image_cols(this->get_width());
  pos += (this->get_width() + 7) / 8;

  auto* bitmaps = static_cast<unsigned char*>(malloc((this->get_width() + 7) / 8));
  memcpy(bitmaps, buff + pos, (this->get_width() + 7) / 8);
  this->set_columns_after_bitmaps(bitmaps);
  pos += (this->get_width() + 7) / 8;

  // FIXME deserialize by table map event
  uint64_t before_row_bytes = header->get_event_length() - pos - get_checksum_len();
  auto* before_row = static_cast<unsigned char*>(malloc(before_row_bytes));
  memcpy(before_row, buff + pos, before_row_bytes);
  // table map event
  this->get_before_row().push_back(reinterpret_cast<char*>(before_row), before_row_bytes);
  pos += before_row_bytes;
}

size_t GtidLogEvent::flush_to_buff(unsigned char* buff)
{
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;

  int1store(buff + pos, this->get_commit_flag());
  pos += 1;

  memcpy(buff + pos, this->get_gtid_uuid(), SERVER_UUID_LEN);
  pos += SERVER_UUID_LEN;

  int8store(buff + pos, this->get_gtid_txn_id());
  pos += 8;

  int1store(buff + pos, this->get_ts_type());
  pos += 1;

  int8store(buff + pos, this->get_last_committed());
  pos += 8;

  int8store(buff + pos, this->get_sequence_number());
  pos += 8;

  return write_checksum(buff, pos);
}
unsigned char GtidLogEvent::get_commit_flag() const
{
  return _commit_flag;
}
const unsigned char* GtidLogEvent::get_gtid_uuid() const
{
  return _gtid_uuid;
}
void GtidLogEvent::set_gtid_uuid(unsigned char* gtid_uuid)
{
  _gtid_uuid = gtid_uuid;
}
uint64_t GtidLogEvent::get_gtid_txn_id() const
{
  return _gtid_txn_id;
}
void GtidLogEvent::set_gtid_txn_id(uint64_t gtid_txn_id)
{
  _gtid_txn_id = gtid_txn_id;
}
void GtidLogEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  size_t pos = COMMON_HEADER_LENGTH;
  this->set_commit_flag(int1load(buff + pos));
  pos += 1;

  auto* gtid_uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
  memcpy(gtid_uuid, buff + pos, SERVER_UUID_LEN);
  this->set_gtid_uuid(gtid_uuid);
  pos += SERVER_UUID_LEN;

  this->set_gtid_txn_id(int8load(buff + pos));
  pos += 8;

  this->set_ts_type(int1load(buff + pos));
  pos += 1;

  this->set_last_committed(int8load(buff + pos));
  pos += 8;

  this->set_sequence_number(int8load(buff + pos));
  pos += 8;
}
void GtidLogEvent::set_commit_flag(const unsigned char commit_flag)
{
  _commit_flag = commit_flag;
}
uint64_t GtidLogEvent::get_last_committed() const
{
  return _last_committed;
}
void GtidLogEvent::set_last_committed(uint64_t last_committed)
{
  _last_committed = last_committed;
}
uint64_t GtidLogEvent::get_sequence_number() const
{
  return _sequence_number;
}
void GtidLogEvent::set_sequence_number(uint64_t sequence_number)
{
  _sequence_number = sequence_number;
}
int GtidLogEvent::get_ts_type() const
{
  return _ts_type;
}
void GtidLogEvent::set_ts_type(int ts_type)
{
  _ts_type = ts_type;
}

GtidLogEvent::~GtidLogEvent()
{
  if (_gtid_uuid != nullptr) {
    free(_gtid_uuid);
  }
}

void GtidLogEvent::set_gtid_uuid(const std::string& gtid_uuid)
{
  std::vector<std::string> parts;
  logproxy::split(gtid_uuid, '-', parts);
  if (parts.size() == 5) {
    this->_gtid_uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
    int pos = 0;
    binlog::CommonUtils::hex_to_bin(parts[0], this->_gtid_uuid + pos);
    pos += 4;
    binlog::CommonUtils::hex_to_bin(parts[1], this->_gtid_uuid + pos);
    pos += 2;
    binlog::CommonUtils::hex_to_bin(parts[2], this->_gtid_uuid + pos);
    pos += 2;
    binlog::CommonUtils::hex_to_bin(parts[3], this->_gtid_uuid + pos);
    pos += 2;
    binlog::CommonUtils::hex_to_bin(parts[4], this->_gtid_uuid + pos);
  } else {
    OMS_STREAM_ERROR << "Invalid server uuid:" << gtid_uuid;
  }
}

std::string GtidLogEvent::print_event_info()
{
  std::stringstream info;
  info << "SET @@SESSION.GTID_NEXT= "
       << "'" << binlog::CommonUtils::gtid_format(this->get_gtid_uuid(), this->get_gtid_txn_id()) << "'";
  return info.str();
}

size_t TableMapEvent::flush_to_buff(unsigned char* buff)
{
  this->get_header()->flush_to_buff(buff);
  size_t pos = COMMON_HEADER_LENGTH;

  int6store(buff + pos, this->get_table_id());
  pos += 6;

  int2store(buff + pos, this->get_flags());
  pos += 2;

  int1store(buff + pos, this->get_db_len());
  pos += 1;

  write_null_terminate_string(reinterpret_cast<char*>(buff + pos), this->get_db_len() + 1, this->get_db_name());
  pos += this->get_db_len() + 1;
  // table name
  int1store(buff + pos, this->get_tb_len());
  pos += 1;

  write_null_terminate_string(reinterpret_cast<char*>(buff + pos), this->get_tb_len() + 1, this->get_tb_name());
  pos += this->get_tb_len() + 1;
  //  int2store(buff + pos, this->get_column_count());
  pos += write_lenenc_uint(reinterpret_cast<char*>(buff + pos), INT32_MAX, this->get_column_count());

  // column type
  memcpy(buff + pos, this->get_column_type(), this->get_column_count());
  pos += this->get_column_count();

  pos += write_lenenc_uint(reinterpret_cast<char*>(buff + pos), INT32_MAX, this->get_metadata_len());

  memcpy(buff + pos, this->get_metadata(), this->get_metadata_len());
  pos += this->get_metadata_len();

  memcpy(buff + pos, this->get_null_bits(), (this->get_column_count() + 7) / 8);
  pos += (this->get_column_count() + 7) / 8;

  return write_checksum(buff, pos);
}
uint64_t TableMapEvent::get_table_id() const
{
  return _table_id;
}
void TableMapEvent::set_table_id(uint64_t table_id)
{
  _table_id = table_id;
}
uint16_t TableMapEvent::get_flags() const
{
  return _flags;
}
void TableMapEvent::set_flags(uint16_t flags)
{
  _flags = flags;
}
std::string TableMapEvent::get_db_name()
{
  return _db_name;
}
void TableMapEvent::set_db_name(std::string db_name)
{
  _db_name = std::move(db_name);
}
std::string TableMapEvent::get_tb_name()
{
  return _tb_name;
}
void TableMapEvent::set_tb_name(std::string tb_name)
{
  _tb_name = std::move(tb_name);
}
size_t TableMapEvent::get_column_count() const
{
  return _column_count;
}
void TableMapEvent::set_column_count(size_t column_count)
{
  _column_count = column_count;
}
const unsigned char* TableMapEvent::get_column_type() const
{
  return _column_type;
}
void TableMapEvent::set_column_type(unsigned char* column_type)
{
  _column_type = column_type;
}
size_t TableMapEvent::get_metadata_len() const
{
  return _metadata_len;
}
void TableMapEvent::set_metadata_len(size_t metadata_len)
{
  _metadata_len = metadata_len;
}
const unsigned char* TableMapEvent::get_metadata() const
{
  return _metadata;
}
void TableMapEvent::set_metadata(unsigned char* metadata)
{
  _metadata = metadata;
}
const unsigned char* TableMapEvent::get_null_bits() const
{
  return _null_bits;
}
void TableMapEvent::set_null_bits(unsigned char* null_bits)
{
  _null_bits = null_bits;
}
size_t TableMapEvent::get_db_len() const
{
  return _db_len;
}
void TableMapEvent::set_db_len(size_t db_len)
{
  _db_len = db_len;
}
size_t TableMapEvent::get_tb_len() const
{
  return _tb_len;
}
void TableMapEvent::set_tb_len(size_t tb_len)
{
  _tb_len = tb_len;
}
TableMapEvent::~TableMapEvent()
{
  if (_column_type != nullptr) {
    free(_column_type);
  }

  if (_metadata != nullptr) {
    free(_metadata);
  }

  if (_null_bits != nullptr) {
    free(_null_bits);
  }
}
void TableMapEvent::deserialize(unsigned char* buff)
{
  auto* header = new OblogEventHeader();
  header->deserialize(buff);
  this->set_header(header);
  uint64_t pos = COMMON_HEADER_LENGTH;

  this->set_table_id(int6load(buff + pos));
  pos += 6;

  this->set_flags(int2load(buff + pos));
  pos += 2;

  this->set_db_len(int1load(buff + pos));
  pos += 1;

  std::string db_name(reinterpret_cast<char*>(buff) + pos, this->get_db_len());
  this->set_db_name(db_name);
  pos += this->get_db_len() + 1;

  this->set_tb_len(int1load(buff + pos));
  pos += 1;

  std::string tb_name(reinterpret_cast<char*>(buff) + pos, this->get_tb_len());
  this->set_tb_name(tb_name);
  pos += this->get_tb_len() + 1;

  this->set_column_count(get_lenenc_uint(buff, pos));

  auto* col_type = static_cast<unsigned char*>(malloc(this->get_column_count()));
  this->set_column_type(col_type);
  pos += this->get_column_count();

  this->set_metadata_len(get_lenenc_uint(buff, pos));

  auto* metadata = static_cast<unsigned char*>(malloc(this->get_metadata_len()));
  this->set_column_type(metadata);
  pos += this->get_metadata_len();

  auto* null_bits = static_cast<unsigned char*>(malloc((this->get_column_count() + 7) / 8));
  this->set_null_bits(null_bits);
  pos += (this->get_column_count() + 7) / 8;
}

std::string TableMapEvent::print_event_info()
{
  std::stringstream info;
  info << "table id: " << this->get_table_id() << " (" << this->get_db_name() << "." << this->get_tb_name() << ")";
  return info.str();
}

size_t write_rows(unsigned char* buff, size_t pos, MsgBuf& rows, size_t len)
{
  char* temp = static_cast<char*>(malloc(rows.byte_size()));
  rows.bytes(temp);
  memcpy(buff + pos, temp, len);
  free(temp);
  return pos + len;
}

/**
 *
 * @param binlog
 * @param txn_id
 * @return
 */
int64_t seek_gtid_event(const std::string& binlog, int64_t& record_num, std::vector<GtidLogEvent*>& gtid_log_events,
    bool& existed, int64_t& last_txn_record_num, uint8_t& checksum_flag)
{
  unsigned char magic[BINLOG_MAGIC_SIZE];
  unsigned char buff[COMMON_HEADER_LENGTH];
  int64_t pos = 0;
  FILE* fp = logproxy::FsUtil::fopen_binary(binlog);
  if (fp == nullptr) {
    OMS_ERROR("Failed to open file:{},reason:{}", binlog, logproxy::system_err(errno));
    return pos;
  }
  // read magic number
  FsUtil::read_file(fp, magic, pos, BINLOG_MAGIC_SIZE);

  if (memcmp(magic, binlog_magic, BINLOG_MAGIC_SIZE) != 0) {
    OMS_STREAM_ERROR << "The file format is invalid.";
    FsUtil::fclose_binary(fp);
    return OMS_FAILED;
  }

  pos += BINLOG_MAGIC_SIZE;

  int64_t file_size = FsUtil::file_size(binlog);

  while (pos < file_size) {
    // read common header
    size_t ret = FsUtil::read_file(fp, buff, pos, COMMON_HEADER_LENGTH);
    if (ret != OMS_OK) {
      OMS_STREAM_ERROR << "Failed to read common header";
      break;
    }
    OblogEventHeader header = OblogEventHeader();
    header.deserialize(buff);
    switch (header.get_type_code()) {
      case XID_EVENT:
        last_txn_record_num = record_num;
        record_num = 0;
        break;
      case QUERY_EVENT:
      case TABLE_MAP_EVENT:
      case WRITE_ROWS_EVENT:
      case UPDATE_ROWS_EVENT:
      case DELETE_ROWS_EVENT:
        record_num++;
        break;
      case GTID_LOG_EVENT: {
        auto* event = static_cast<unsigned char*>(malloc(header.get_event_length()));
        FreeGuard<unsigned char*> free_guard(event);
        ret = FsUtil::read_file(fp, event, pos, header.get_event_length());
        if (ret != OMS_OK) {
          OMS_STREAM_ERROR << "Failed to read GTID LOG EVENT";
          break;
        }
        auto* gtid_log_event = new GtidLogEvent();
        gtid_log_event->deserialize(event);
        gtid_log_events.emplace_back(gtid_log_event);
        record_num++;
        break;
      }
      case ROTATE_EVENT:
        existed = true;
        break;
      case FORMAT_DESCRIPTION_EVENT: {
        auto* event = static_cast<unsigned char*>(malloc(header.get_event_length()));
        FreeGuard<unsigned char*> free_guard(event);
        ret = FsUtil::read_file(fp, event, pos, header.get_event_length());
        if (ret != OMS_OK) {
          OMS_STREAM_ERROR << "Failed to read GTID LOG EVENT";
          break;
        }
        auto format_description_event = logproxy::FormatDescriptionEvent();
        format_description_event.deserialize(event);
        checksum_flag = format_description_event.get_checksum_flag();
        OMS_INFO("The checksum configuration of the current binlog file is:{}", checksum_flag);
        break;
      }
      default:
        break;
    }
    pos = header.get_next_position();
  }
  FsUtil::fclose_binary(fp);
  return pos;
}

int get_the_last_complete_txn(const std::string& binlog, uint64_t& complete_transaction_pos,
    uint64_t& last_complete_txn_id, uint64_t& start_complete_txn_id, bool& rotate_existed)
{
  complete_transaction_pos = BINLOG_MAGIC_SIZE;
  unsigned char magic[BINLOG_MAGIC_SIZE];
  unsigned char buff[COMMON_HEADER_LENGTH];
  int64_t pos = 0;
  FILE* fp = FsUtil::fopen_binary(binlog);
  defer(FsUtil::fclose_binary(fp));

  if (fp == nullptr) {
    OMS_ERROR("Failed to open file:{},reason:{}", binlog, system_err(errno));
    return OMS_FAILED;
  }
  // read magic number
  FsUtil::read_file(fp, magic, pos, BINLOG_MAGIC_SIZE);

  if (memcmp(magic, binlog_magic, BINLOG_MAGIC_SIZE) != 0) {
    OMS_STREAM_ERROR << "The file format is invalid.";
    return OMS_FAILED;
  }

  pos += BINLOG_MAGIC_SIZE;

  uint64_t file_size = FsUtil::file_size(binlog);

  bool within_transaction = false;
  uint64_t current_gtid = 0;
  uint8_t checksum_flag = OFF;
  while (pos < file_size) {
    // read common header
    size_t ret = FsUtil::read_file(fp, buff, pos, COMMON_HEADER_LENGTH);
    if (ret != OMS_OK) {
      OMS_STREAM_ERROR << "Failed to read common header";
      return OMS_FAILED;
    }
    OblogEventHeader header = OblogEventHeader();
    header.deserialize(buff);
    auto* event = static_cast<unsigned char*>(malloc(header.get_event_length()));
    FreeGuard<unsigned char*> free_guard(event);
    ret = FsUtil::read_file(fp, event, pos, header.get_event_length());
    if (ret != OMS_OK) {
      OMS_ERROR("Failed to read event");
      return OMS_FAILED;
    }

    ret = verify_event_crc32(event, header.get_event_length(), checksum_flag);
    if (ret != OMS_OK) {
      return OMS_FAILED;
    }

    if (header.get_type_code() == XID_EVENT) {
      // At this time, it is expected that within_transaction must be equal to true, otherwise it does not meet
      // expectations and exits abnormally.
      if (!within_transaction) {
        OMS_ERROR(
            "At this time, it is expected that within_transaction must be equal to true, otherwise it does not meet"
            "expectations and exits abnormally.");
        return OMS_FAILED;
      } else {
        within_transaction = false;
        last_complete_txn_id = current_gtid;

        /*!
         * Retrieve the first complete transaction in the current binlog file
         */
        if (start_complete_txn_id == 0) {
          start_complete_txn_id = last_complete_txn_id;
        }
      }
    }

    if (header.get_type_code() == QUERY_EVENT) {
      auto query_event = QueryEvent();
      query_event.deserialize(event);
      if (strcmp(query_event.get_sql_statment().c_str(), BEGIN_VAR) == 0) {
        within_transaction = true;
      } else {
        // When it is non-BEGIN, we all think it is an ordinary DDL change (note: COMMIT will not be recorded in the
        // query event in the implementation)
        last_complete_txn_id = current_gtid;
        /*!
         * Retrieve the first complete transaction in the current binlog file
         */
        if (start_complete_txn_id == 0) {
          start_complete_txn_id = last_complete_txn_id;
        }
      }
    }

    if (!within_transaction && header.get_type_code() != GTID_LOG_EVENT) {
      complete_transaction_pos = pos + header.get_event_length();
    }

    switch (header.get_type_code()) {
      case GTID_LOG_EVENT: {
        ret = FsUtil::read_file(fp, event, pos, header.get_event_length());
        if (ret != OMS_OK) {
          OMS_STREAM_ERROR << "Failed to read GTID LOG EVENT";
          return OMS_FAILED;
        }
        auto gtid_log_event = GtidLogEvent();
        gtid_log_event.deserialize(event);
        current_gtid = gtid_log_event.get_gtid_txn_id();
        break;
      }
      case ROTATE_EVENT: {
        rotate_existed = true;
        break;
      }
      case FORMAT_DESCRIPTION_EVENT: {
        ret = FsUtil::read_file(fp, event, pos, header.get_event_length());
        if (ret != OMS_OK) {
          OMS_ERROR("Failed to deserialize query event");
          return OMS_FAILED;
        }
        auto format_description_event = FormatDescriptionEvent();
        format_description_event.deserialize(event);
        checksum_flag = format_description_event.get_checksum_flag();
        OMS_INFO("The checksum configuration of the current binlog file is:{}", checksum_flag);
        break;
      }
      default:
        break;
    }
    pos += header.get_event_length();
  }
  return OMS_OK;
}

int verify_event_crc32(unsigned char* event, uint64_t len, uint8_t checksum_flag)
{
  if (checksum_flag != CRC32) {
    return OMS_OK;
  }
  uint32_t check_sum_pos = len - 4;
  uint32_t check_sum_read = int4load(event + check_sum_pos);
  uint64_t crc = crc32(0L, Z_NULL, 0);
  uint32_t check_sum_compute = crc32(crc, event, check_sum_pos);
  if (check_sum_read != check_sum_compute) {
    OMS_ERROR("The checksum [{}] carried by the last binlog event is different from the calculated checksum: {}",
        check_sum_read,
        check_sum_compute);
    return OMS_FAILED;
  }
  OMS_INFO("The checksum carried by the last binlog event is equal to the calculated checksum: {} = {}",
      check_sum_read,
      check_sum_compute);
  return OMS_OK;
}

int seek_events(const std::string& binlog, std::vector<ObLogEvent*>& log_events, EventType event_type, bool single)
{
  unsigned char magic[BINLOG_MAGIC_SIZE];
  unsigned char buff[COMMON_HEADER_LENGTH];
  size_t pos = 0;
  std::ifstream ifstream(binlog, std::ifstream ::in | std::ifstream::out);
  // read magic number
  FsUtil::read_file(ifstream, magic, pos, sizeof(magic));

  if (memcmp(magic, binlog_magic, sizeof(magic)) != 0) {
    OMS_STREAM_ERROR << "The file format is invalid.";
    ifstream.close();
    return OMS_FAILED;
  }

  pos += BINLOG_MAGIC_SIZE;

  for (;;) {
    // read common header
    size_t ret = FsUtil::read_file(ifstream, buff, pos, sizeof buff);
    if (ret != OMS_OK) {
      ifstream.close();
      break;
    }
    OblogEventHeader header = OblogEventHeader();
    header.deserialize(buff);
    auto* event = static_cast<unsigned char*>(malloc(header.get_event_length()));
    ret = FsUtil::read_file(ifstream, event, pos, header.get_event_length());
    if (ret != OMS_OK) {
      ifstream.close();
      break;
    }

    if (event_type == ENUM_END_EVENT || header.get_type_code() == event_type) {
      switch (header.get_type_code()) {
        case QUERY_EVENT: {
          auto* query_event = new QueryEvent("", "");
          query_event->deserialize(event);
          log_events.emplace_back(query_event);
          break;
        }
        case ROTATE_EVENT: {
          auto* rotate_event = new RotateEvent(0, "", 0, 0);
          rotate_event->deserialize(event);
          log_events.emplace_back(rotate_event);
          break;
        }
        case FORMAT_DESCRIPTION_EVENT: {
          auto* format_description_event = new FormatDescriptionEvent();
          format_description_event->deserialize(event);
          log_events.emplace_back(format_description_event);
          break;
        }
        case XID_EVENT: {
          auto* xid_event = new XidEvent();
          xid_event->deserialize(event);
          log_events.emplace_back(xid_event);
          break;
        }
        case TABLE_MAP_EVENT: {
          auto* table_map_event = new TableMapEvent();
          table_map_event->deserialize(event);
          log_events.emplace_back(table_map_event);
          break;
        }
        case WRITE_ROWS_EVENT: {
          auto* write_rows_event = new WriteRowsEvent(0, 0);
          write_rows_event->deserialize(event);
          log_events.emplace_back(write_rows_event);
          break;
        }
        case UPDATE_ROWS_EVENT: {
          auto* update_rows_event = new UpdateRowsEvent(0, 0);
          update_rows_event->deserialize(event);
          log_events.emplace_back(update_rows_event);
          break;
        }
        case DELETE_ROWS_EVENT: {
          auto* delete_row_event = new DeleteRowsEvent(0, 0);
          delete_row_event->deserialize(event);
          log_events.emplace_back(delete_row_event);
          break;
        }
        case GTID_LOG_EVENT: {
          auto* gtid_log_event = new GtidLogEvent();
          gtid_log_event->deserialize(event);
          log_events.emplace_back(gtid_log_event);
          break;
        }
        case PREVIOUS_GTIDS_LOG_EVENT: {
          auto* previous_gtids_log_event = new PreviousGtidsLogEvent();
          previous_gtids_log_event->deserialize(event);
          log_events.emplace_back(previous_gtids_log_event);
          break;
        }
        default:
          OMS_STREAM_ERROR << "Unknown event type:" << header.get_type_code();
          break;
      }
    }
    free(event);
    pos = header.get_next_position();
    if (single && !log_events.empty()) {
      // Get a single record that meets the conditions and break out of the loop
      break;
    }
  }
  ifstream.close();
  return OMS_OK;
}

const std::string& HeartbeatEvent::get_binlog_file_name() const
{
  return _binlog_file_name;
}

void HeartbeatEvent::set_binlog_file_name(const std::string& binlog_file_name)
{
  _binlog_file_name = binlog_file_name;
}

size_t HeartbeatEvent::flush_to_buff(unsigned char* data)
{
  this->get_header()->flush_to_buff(data);
  size_t pos = COMMON_HEADER_LENGTH;

  memcpy(data + pos, this->get_binlog_file_name().c_str(), this->get_binlog_file_name().size());
  pos += this->get_binlog_file_name().size();
  return write_checksum(data, pos);
}

void HeartbeatEvent::deserialize(unsigned char* buff)
{}

HeartbeatEvent::HeartbeatEvent(std::string binlog_file_name, uint64_t pos)
    : _binlog_file_name(std::move(binlog_file_name))
{
  uint32_t event_len = COMMON_HEADER_LENGTH + HEARTBEAT_HEADER_LEN + _binlog_file_name.size() + get_checksum_len();
  this->set_header(new OblogEventHeader(HEARTBEAT_LOG_EVENT, 0, event_len, pos));
}
std::string HeartbeatEvent::print_event_info()
{
  return std::string();
}

}  // namespace logproxy
};  // namespace oceanbase
