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

#include <bitset>
#include "config.h"
#include "oblog_config.h"
#include "counter.h"
#include "logmsg_buf.h"
#include "log_record.h"
#include "meta_info.h"
#include "msg_buf.h"
#include "common_util.h"
#include "obaccess/ob_mysql_packet.h"
#include "data_type.h"
#include "binlog_convert.h"
#include "counter.h"
#include "ddl-converter/ddl_converter.h"
namespace oceanbase {
namespace logproxy {
static Config& _s_config = Config::instance();

static __thread LogMsgBuf* _t_s_lmb = nullptr;
#define LogMsgLocalInit                                         \
  if ((_t_s_lmb = new (std::nothrow) LogMsgBuf()) == nullptr) { \
    OMS_STREAM_ERROR << "Failed to alloc LogMsgBuf";            \
    stop();                                                     \
    return;                                                     \
  }
#define LogMsgLocalDestroy delete _t_s_lmb

BinlogConvert::BinlogConvert(
    BinlogConverter& converter, BlockingQueue<ILogRecord*>& rqueue, BlockingQueue<ObLogEvent*>& event_queue)
    : Thread("BinlogConvert"), _oblog(nullptr), _converter(converter), _rqueue(rqueue), _event_queue(event_queue)
{}

int BinlogConvert::init(ConvertMeta& meta, OblogConfig& config, IObCdcAccess* oblog)
{
  Counter::instance().register_gauge("NEventQ", [this]() { return _event_queue.size(); });
  _oblog = oblog;
  set_meta(meta);
  std::uint32_t checksum = 0;
  if (Config::instance().binlog_checksum.val()) {
    checksum = COMMON_CHECKSUM_LENGTH;
  }
  this->_cur_pos = BINLOG_START_POS + checksum;
  if (config.initial_trx_xid.val().empty()) {
    this->_cur_pos -= 16;
  }
  OMS_INFO("cur_pos: {}", this->_cur_pos);

  return recover(meta, config);
}

int BinlogConvert::consume_exactly_once(const ConvertMeta& meta, OblogConfig& config)
{  // find current binlog files
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  uint64_t next_index = index_record._index + 1;
  uint64_t file_size = FsUtil::file_size(index_record.get_file_name());

  auto* rotate_event = new RotateEvent(
      BINLOG_MAGIC_SIZE, binlog::CommonUtils::fill_binlog_file_name(next_index), Timer::now() / 1000000, file_size);
  int64_t record_num = 0;
  int64_t last_txn_record_num = 0;
  vector<GtidLogEvent*> gtid_events;
  bool rotate_existed = false;
  uint8_t checksum_flag = CRC32;
  int64_t offset = seek_gtid_event(
      index_record._file_name, record_num, gtid_events, rotate_existed, last_txn_record_num, checksum_flag);
  // When the file is not initialized, the file offset needs to be set to the current latest offset
  if (offset > 0 && index_record.get_position() != 0) {
    _cur_pos = offset;
  }

  OMS_INFO("Seek gtid events from: {}, result count: {}", index_record._file_name, gtid_events.size());
  size_t txn_ts = 0;
  if (!gtid_events.empty()) {
    GtidLogEvent* event = gtid_events.back();
    _txn_id = event->get_gtid_txn_id();
    txn_ts = event->get_last_committed() * 1000000 + event->get_sequence_number();
    config.start_timestamp_us.set(index_record.get_checkpoint());
    _start_timestamp = index_record.get_checkpoint();
    if (_txn_id == index_record._current_mapping.second) {
      _txn_mapping.first = _txn_id;
      _txn_mapping.second = index_record._current_mapping.first;
    } else if (_txn_id == index_record._before_mapping.second) {
      _txn_mapping.first = _txn_id;
      _txn_mapping.second = index_record._before_mapping.first;
    } else {
      // Could not find mapping record for transaction
      OMS_STREAM_ERROR << "Could not find mapping record for transaction:" << _txn_id;
      release_vector(gtid_events);
      return OMS_FAILED;
    }
    rotate_event->set_op(RotateEvent::ROTATE);
    _skip_record_num = record_num;
    OMS_STREAM_INFO << "current transaction:" << _txn_mapping.first << "<>" << _txn_mapping.second
                    << " skip record num :" << _skip_record_num;
    release_vector(gtid_events);
  } else {
    OMS_STREAM_INFO << "No corresponding gtid event found from " << index_record.get_file_name();

    OMS_INFO("Specify gtid mapping relationship to start,initial_trx_xid:{},initial_trx_gtid_seq:{}",
        config.initial_trx_xid.val(),
        config.initial_trx_gtid_seq.val());
    if (index_record.get_position() == 0) {
      /*!
       * @brief If the gtid mapping relationship is specified,
       *        we should start the binlog service from the specified mapping relationship
       */
      if (!config.initial_trx_xid.val().empty()) {
        _txn_mapping.first = config.initial_trx_gtid_seq.val();
        _txn_mapping.second = config.initial_trx_xid.val();
        _txn_id = config.initial_trx_gtid_seq.val();
        if (config.start_timestamp_us.val() != 0) {
          _start_timestamp = config.start_timestamp_us.val();
        } else {
          OMS_ERROR("When specifying gtid mapping, the start time cannot be empty");
          return OMS_FAILED;
        }

        txn_ts = _start_timestamp;
        OMS_INFO("Specify gtid mapping relationship to start,initial_trx_xid:{},initial_trx_gtid_seq:{}",
            _txn_mapping.second,
            _txn_mapping.first);
      }
      rotate_event->set_op(RotateEvent::INIT);
    } else {
      rotate_event->set_op(RotateEvent::ROTATE);
    }
  }

  if (config.start_timestamp_us.val() != 0) {
    rotate_event->get_header()->set_timestamp(config.start_timestamp_us.val() / 1000000);
  }
  rotate_event->set_existed(rotate_existed);
  // When there is an incomplete transaction, the rotation action should not be performed
  if (_skip_record_num == 0) {
    // Although there is no need to filter DML records at this time, there may be duplicate transactions passed over, so
    // the timestamp of the current transaction is less than or equal to the timestamp stored in the gtid event, and the
    // transaction needs to be skipped.
    if (txn_ts != 0 && _start_timestamp <= txn_ts) {
      _skip_record_num = last_txn_record_num;
    } else {
      OMS_INFO("The transactions in the binlog file are complete and do not need to be filtered");
      _filter = false;
    }
    rotate_event->set_index(next_index);
    if (checksum_flag == OFF && rotate_event->get_checksum_flag() == CRC32) {
      rotate_event->get_header()->set_event_length(
          rotate_event->get_header()->get_event_length() - rotate_event->get_checksum_len());

      rotate_event->get_header()->set_next_position(
          rotate_event->get_header()->get_next_position() - rotate_event->get_checksum_len());
      rotate_event->set_checksum_flag(OFF);
    }
    append_event(_event_queue, rotate_event);
    if (rotate_event->get_op() == RotateEvent::INIT) {
      next_index -= 1;
    }
    _binlog_file_index = next_index;

    if (rotate_event->get_op() == RotateEvent::ROTATE) {
      /*!
       * @brief Since the rotation is triggered, we need to reset the offset of the conversion.
       * Since the Format Description Event and Previous Gtids Log Event are generated by default after the rotation,
       * the offset is fixed
       */
      std::uint32_t checksum = 0;
      if (Config::instance().binlog_checksum.val()) {
        checksum = COMMON_CHECKSUM_LENGTH;
      }
      _cur_pos = BINLOG_START_POS + checksum;
    }

  } else {
    _binlog_file_index = index_record.get_index();
  }

  return OMS_OK;
}
void BinlogConvert::stop()
{
  if (is_run()) {
    Thread::stop();
  }
}
void BinlogConvert::run()
{
  LogMsgLocalInit;

  std::vector<ILogRecord*> records;
  records.reserve(_s_config.read_wait_num.val());
  while (is_run()) {
    _stage_timer.reset();
    records.clear();
    while (!_rqueue.poll(records, _s_config.read_timeout_us.val()) || records.empty()) {
      OMS_STREAM_INFO << "send transfer queue empty, retry...";
    }
    do_convert(records);
    for (ILogRecord* r : records) {
      _oblog->release(r);
    }
    logproxy::Counter::instance().count_key(Counter::SENDER_ENCODE_US, _stage_timer.elapsed());
  }
  LogMsgLocalDestroy;
}

uint64_t get_timestamp_sec(ILogRecord* record)
{
  return record->getTimestamp();
}

uint64_t get_timestamp_usec(ILogRecord* record)
{
  return record->getTimestamp() * 1000 * 1000 + record->getRecordUsec();
}

uint64_t get_checkpoint_usec(ILogRecord* record)
{
  return record->getCheckpoint1() * 1000 * 1000 + record->getCheckpoint2();
}

std::string get_transaction_id(ILogRecord* record)
{
  std::string ret;
  unsigned int count = 0;
  const BinLogBuf* binlogbuf = ((LogRecordImpl*)record)->filterValues(count);
  if (nullptr != binlogbuf) {
    assert(count > 1);
    ret.append(binlogbuf[1].buf);
  }
  return ret;
}

void BinlogConvert::convert_gtid_log_event(ILogRecord* record)
{
  auto* gtid_log_event = new GtidLogEvent();
  gtid_log_event->set_gtid_txn_id(this->_txn_id + 1);
  gtid_log_event->set_gtid_uuid(this->get_meta().server_uuid);

  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + GTID_HEADER_LEN + gtid_log_event->get_checksum_len();
  auto* common_header =
      new OblogEventHeader(GTID_LOG_EVENT, get_timestamp_sec(record), event_len, this->_cur_pos + event_len);
  gtid_log_event->set_header(common_header);
  gtid_log_event->set_ob_txn(get_transaction_id(record));
  gtid_log_event->set_checkpoint(get_checkpoint_usec(record));
  gtid_log_event->set_last_committed(record->getTimestamp());
  gtid_log_event->set_sequence_number(record->getRecordUsec());

  if (!this->_filter) {
    this->_cur_pos = gtid_log_event->get_header()->get_next_position();
    this->_txn_id = gtid_log_event->get_gtid_txn_id();
    append_event(this->_event_queue, gtid_log_event);
    convert_query_event(record);
  } else {

    if (this->_txn_mapping.second == gtid_log_event->get_ob_txn()) {
      if (_specified_gtids) {
        _filter = false;
        this->_cur_pos = gtid_log_event->get_header()->get_next_position();
        this->_txn_id = gtid_log_event->get_gtid_txn_id();
        append_event(this->_event_queue, gtid_log_event);
        convert_query_event(record);
        OMS_INFO("If the gtid is specified, the transmission will resume from the current mapping [{}={}].",
            this->_txn_mapping.second,
            this->_txn_mapping.first);
        return;
      }
      OMS_INFO("Find the last complete transaction and complete the transaction filtering:[{},{}]",
          _txn_mapping.first,
          _txn_mapping.second);
      _within_filtered_transactions = true;
    }
    delete (gtid_log_event);
  }
}

/*
 * @params full_dbname tenant.dbname
 * @returns dbname
 * @description get database name does not contain tenant name
 * @date 2022/10/19 14:43
 */
std::string get_dbname_without_tenant(const std::string& full_dbname)
{
  if (full_dbname.empty()) {
    return full_dbname;
  }
  std::vector<std::string> parts;
  split(full_dbname, '.', parts);
  // When some DDL changes to the database, only the tenant name information will appear
  if (parts.size() == 1) {
    return "";
  }
  return parts[1];
}

void BinlogConvert::convert_query_event(ILogRecord* record)
{
  char* sql = nullptr;
  size_t sql_statment_len = 0;
  if (record->recordType() == EBEGIN) {
    sql = static_cast<char*>(malloc(BEGIN_VAR_LEN));
    write_string(sql, BEGIN_VAR_LEN, BEGIN_VAR, BEGIN_VAR_LEN);
    sql_statment_len = BEGIN_VAR_LEN;
  } else {
    unsigned int new_col_count = 0;
    refresh_table_cache(get_dbname_without_tenant(record->dbname()), record->tbname());
    BinLogBuf* new_bin_log_buf = record->newCols(new_col_count);
    sql_statment_len = new_bin_log_buf->buf_used_size;
    sql = static_cast<char*>(malloc(new_bin_log_buf->buf_used_size));
    write_string(sql, sql_statment_len, new_bin_log_buf->buf, sql_statment_len);
    if (_s_config.binlog_ddl_convert.val()) {
      std::string convert_sql;
      std::string ddl = std::string{sql, sql_statment_len};
      int convert_ret = DdlConverter::convert(ddl, convert_sql);
      if (convert_ret != OMS_OK) {
        convert_sql = std::string{sql, sql_statment_len};
        // etransfer failed to convert incremental DDL, using untransformed DDL
        OMS_STREAM_WARN << "Failed to convert incremental DDL, using untransformed DDL:" << convert_sql;
      }
      free(sql);
      sql = nullptr;
      sql_statment_len = convert_sql.size();
      sql = static_cast<char*>(malloc(sql_statment_len));
      write_string(sql, sql_statment_len, convert_sql.c_str(), sql_statment_len);
    }
  }

  std::string dbname = get_dbname_without_tenant(record->dbname());
  std::string ddl = std::string{sql, sql_statment_len};
  auto* event = new QueryEvent(dbname, ddl);
  OMS_STREAM_DEBUG << event->print_event_info();
  event->set_sql_statment_len(sql_statment_len);
  event->set_query_exec_time(0);
  event->set_thread_id(record->getThreadId());

  /********** status vars **********/

  /**
   Zero or more status variables.
   Each status variable consists of one byte identifying the variable stored,
   followed by the value of the variable.
   */

  std::uint16_t status_var_len = status_vars_bitfield[Q_FLAGS2_CODE] + 1 + status_vars_bitfield[Q_CHARSET_CODE] + 1;
  event->set_status_var_len(status_var_len);
  auto* status_vars = static_cast<char*>(malloc(status_var_len));
  std::uint16_t offset = 0;
  int1store(reinterpret_cast<unsigned char*>(status_vars + offset), Q_FLAGS2_CODE);
  offset += 1;
  int4store(reinterpret_cast<unsigned char*>(status_vars + offset), 0);
  offset += 4;

  int1store(reinterpret_cast<unsigned char*>(status_vars + offset), Q_CHARSET_CODE);
  offset += 1;

  /*!
   * @brief The default character set of higher versions of MySQL Server is utf8mb4, and the collation is
   * utf8mb4_general_ci.
   */

  /*  mysql> SELECT id, character_set_name, collation_name FROM COLLATIONS ORDER BY id;
    +-----+--------------------+--------------------------+
        | id  | character_set_name | collation_name           |
        +-----+--------------------+--------------------------+
        |   1 | big5               | big5_chinese_ci          |
        |   2 | latin2             | latin2_czech_cs          |
        |   3 | dec8               | dec8_swedish_ci          |
        |   4 | cp850              | cp850_general_ci         |
        |   5 | latin1             | latin1_german1_ci        |
        |   6 | hp8                | hp8_english_ci           |
        |   7 | koi8r              | koi8r_general_ci         |
        |   8 | latin1             | latin1_swedish_ci        |
        |   9 | latin2             | latin2_general_ci        |
        |  10 | swe7               | swe7_swedish_ci          |
        |  11 | ascii              | ascii_general_ci         |
        |  12 | ujis               | ujis_japanese_ci         |
        |  13 | sjis               | sjis_japanese_ci         |
        |  14 | cp1251             | cp1251_bulgarian_ci      |
        |  15 | latin1             | latin1_danish_ci         |
        |  16 | hebrew             | hebrew_general_ci        |
        |  18 | tis620             | tis620_thai_ci           |
        |  19 | euckr              | euckr_korean_ci          |
        |  20 | latin7             | latin7_estonian_cs       |
        |  21 | latin2             | latin2_hungarian_ci      |
        |  22 | koi8u              | koi8u_general_ci         |
        |  23 | cp1251             | cp1251_ukrainian_ci      |
        |  24 | gb2312             | gb2312_chinese_ci        |
        |  25 | greek              | greek_general_ci         |
        |  26 | cp1250             | cp1250_general_ci        |
        |  27 | latin2             | latin2_croatian_ci       |
        |  28 | gbk                | gbk_chinese_ci           |
        |  29 | cp1257             | cp1257_lithuanian_ci     |
        |  30 | latin5             | latin5_turkish_ci        |
        |  31 | latin1             | latin1_german2_ci        |
        |  32 | armscii8           | armscii8_general_ci      |
        |  33 | utf8               | utf8_general_ci          |
        |  34 | cp1250             | cp1250_czech_cs          |
        |  35 | ucs2               | ucs2_general_ci          |
        |  36 | cp866              | cp866_general_ci         |
        |  37 | keybcs2            | keybcs2_general_ci       |
        |  38 | macce              | macce_general_ci         |
        |  39 | macroman           | macroman_general_ci      |
        |  40 | cp852              | cp852_general_ci         |
        |  41 | latin7             | latin7_general_ci        |
        |  42 | latin7             | latin7_general_cs        |
        |  43 | macce              | macce_bin                |
        |  44 | cp1250             | cp1250_croatian_ci       |
        |  45 | utf8mb4            | utf8mb4_general_ci       |
        |  46 | utf8mb4            | utf8mb4_bin              |
        |  47 | latin1             | latin1_bin               |
        |  48 | latin1             | latin1_general_ci        |
        |  49 | latin1             | latin1_general_cs        |
        |  50 | cp1251             | cp1251_bin               |
        |  51 | cp1251             | cp1251_general_ci        |
        |  52 | cp1251             | cp1251_general_cs        |
        |  53 | macroman           | macroman_bin             |
        |  54 | utf16              | utf16_general_ci         |
        |  55 | utf16              | utf16_bin                |
        |  56 | utf16le            | utf16le_general_ci       |
        |  57 | cp1256             | cp1256_general_ci        |
        |  58 | cp1257             | cp1257_bin               |
        |  59 | cp1257             | cp1257_general_ci        |
        |  60 | utf32              | utf32_general_ci         |
        |  61 | utf32              | utf32_bin                |
        |  62 | utf16le            | utf16le_bin              |
        |  63 | binary             | binary                   |
        |  64 | armscii8           | armscii8_bin             |
        |  65 | ascii              | ascii_bin                |
        |  66 | cp1250             | cp1250_bin               |
        |  67 | cp1256             | cp1256_bin               |
        |  68 | cp866              | cp866_bin                |
        |  69 | dec8               | dec8_bin                 |
        |  70 | greek              | greek_bin                |
        |  71 | hebrew             | hebrew_bin               |
        |  72 | hp8                | hp8_bin                  |
        |  73 | keybcs2            | keybcs2_bin              |
        |  74 | koi8r              | koi8r_bin                |
        |  75 | koi8u              | koi8u_bin                |
        |  77 | latin2             | latin2_bin               |
        |  78 | latin5             | latin5_bin               |
        |  79 | latin7             | latin7_bin               |
        |  80 | cp850              | cp850_bin                |
        |  81 | cp852              | cp852_bin                |
        |  82 | swe7               | swe7_bin                 |
        |  83 | utf8               | utf8_bin                 |
        |  84 | big5               | big5_bin                 |
        |  85 | euckr              | euckr_bin                |
        |  86 | gb2312             | gb2312_bin               |
        |  87 | gbk                | gbk_bin                  |
        |  88 | sjis               | sjis_bin                 |
        |  89 | tis620             | tis620_bin               |
        |  90 | ucs2               | ucs2_bin                 |
        |  91 | ujis               | ujis_bin                 |
        |  92 | geostd8            | geostd8_general_ci       |
        |  93 | geostd8            | geostd8_bin              |
        |  94 | latin1             | latin1_spanish_ci        |
        |  95 | cp932              | cp932_japanese_ci        |
        |  96 | cp932              | cp932_bin                |
        |  97 | eucjpms            | eucjpms_japanese_ci      |
        |  98 | eucjpms            | eucjpms_bin              |
        |  99 | cp1250             | cp1250_polish_ci         |
        | 101 | utf16              | utf16_unicode_ci         |
        | 102 | utf16              | utf16_icelandic_ci       |
        | 103 | utf16              | utf16_latvian_ci         |
        | 104 | utf16              | utf16_romanian_ci        |
        | 105 | utf16              | utf16_slovenian_ci       |
        | 106 | utf16              | utf16_polish_ci          |
        | 107 | utf16              | utf16_estonian_ci        |
        | 108 | utf16              | utf16_spanish_ci         |
        | 109 | utf16              | utf16_swedish_ci         |
        | 110 | utf16              | utf16_turkish_ci         |
        | 111 | utf16              | utf16_czech_ci           |
        | 112 | utf16              | utf16_danish_ci          |
        | 113 | utf16              | utf16_lithuanian_ci      |
        | 114 | utf16              | utf16_slovak_ci          |
        | 115 | utf16              | utf16_spanish2_ci        |
        | 116 | utf16              | utf16_roman_ci           |
        | 117 | utf16              | utf16_persian_ci         |
        | 118 | utf16              | utf16_esperanto_ci       |
        | 119 | utf16              | utf16_hungarian_ci       |
        | 120 | utf16              | utf16_sinhala_ci         |
        | 121 | utf16              | utf16_german2_ci         |
        | 122 | utf16              | utf16_croatian_ci        |
        | 123 | utf16              | utf16_unicode_520_ci     |
        | 124 | utf16              | utf16_vietnamese_ci      |
        | 128 | ucs2               | ucs2_unicode_ci          |
        | 129 | ucs2               | ucs2_icelandic_ci        |
        | 130 | ucs2               | ucs2_latvian_ci          |
        | 131 | ucs2               | ucs2_romanian_ci         |
        | 132 | ucs2               | ucs2_slovenian_ci        |
        | 133 | ucs2               | ucs2_polish_ci           |
        | 134 | ucs2               | ucs2_estonian_ci         |
        | 135 | ucs2               | ucs2_spanish_ci          |
        | 136 | ucs2               | ucs2_swedish_ci          |
        | 137 | ucs2               | ucs2_turkish_ci          |
        | 138 | ucs2               | ucs2_czech_ci            |
        | 139 | ucs2               | ucs2_danish_ci           |
        | 140 | ucs2               | ucs2_lithuanian_ci       |
        | 141 | ucs2               | ucs2_slovak_ci           |
        | 142 | ucs2               | ucs2_spanish2_ci         |
        | 143 | ucs2               | ucs2_roman_ci            |
        | 144 | ucs2               | ucs2_persian_ci          |
        | 145 | ucs2               | ucs2_esperanto_ci        |
        | 146 | ucs2               | ucs2_hungarian_ci        |
        | 147 | ucs2               | ucs2_sinhala_ci          |
        | 148 | ucs2               | ucs2_german2_ci          |
        | 149 | ucs2               | ucs2_croatian_ci         |
        | 150 | ucs2               | ucs2_unicode_520_ci      |
        | 151 | ucs2               | ucs2_vietnamese_ci       |
        | 159 | ucs2               | ucs2_general_mysql500_ci |
        | 160 | utf32              | utf32_unicode_ci         |
        | 161 | utf32              | utf32_icelandic_ci       |
        | 162 | utf32              | utf32_latvian_ci         |
        | 163 | utf32              | utf32_romanian_ci        |
        | 164 | utf32              | utf32_slovenian_ci       |
        | 165 | utf32              | utf32_polish_ci          |
        | 166 | utf32              | utf32_estonian_ci        |
        | 167 | utf32              | utf32_spanish_ci         |
        | 168 | utf32              | utf32_swedish_ci         |
        | 169 | utf32              | utf32_turkish_ci         |
        | 170 | utf32              | utf32_czech_ci           |
        | 171 | utf32              | utf32_danish_ci          |
        | 172 | utf32              | utf32_lithuanian_ci      |
        | 173 | utf32              | utf32_slovak_ci          |
        | 174 | utf32              | utf32_spanish2_ci        |
        | 175 | utf32              | utf32_roman_ci           |
        | 176 | utf32              | utf32_persian_ci         |
        | 177 | utf32              | utf32_esperanto_ci       |
        | 178 | utf32              | utf32_hungarian_ci       |
        | 179 | utf32              | utf32_sinhala_ci         |
        | 180 | utf32              | utf32_german2_ci         |
        | 181 | utf32              | utf32_croatian_ci        |
        | 182 | utf32              | utf32_unicode_520_ci     |
        | 183 | utf32              | utf32_vietnamese_ci      |
        | 192 | utf8               | utf8_unicode_ci          |
        | 193 | utf8               | utf8_icelandic_ci        |
        | 194 | utf8               | utf8_latvian_ci          |
        | 195 | utf8               | utf8_romanian_ci         |
        | 196 | utf8               | utf8_slovenian_ci        |
        | 197 | utf8               | utf8_polish_ci           |
        | 198 | utf8               | utf8_estonian_ci         |
        | 199 | utf8               | utf8_spanish_ci          |
        | 200 | utf8               | utf8_swedish_ci          |
        | 201 | utf8               | utf8_turkish_ci          |
        | 202 | utf8               | utf8_czech_ci            |
        | 203 | utf8               | utf8_danish_ci           |
        | 204 | utf8               | utf8_lithuanian_ci       |
        | 205 | utf8               | utf8_slovak_ci           |
        | 206 | utf8               | utf8_spanish2_ci         |
        | 207 | utf8               | utf8_roman_ci            |
        | 208 | utf8               | utf8_persian_ci          |
        | 209 | utf8               | utf8_esperanto_ci        |
        | 210 | utf8               | utf8_hungarian_ci        |
        | 211 | utf8               | utf8_sinhala_ci          |
        | 212 | utf8               | utf8_german2_ci          |
        | 213 | utf8               | utf8_croatian_ci         |
        | 214 | utf8               | utf8_unicode_520_ci      |
        | 215 | utf8               | utf8_vietnamese_ci       |
        | 223 | utf8               | utf8_general_mysql500_ci |
        | 224 | utf8mb4            | utf8mb4_unicode_ci       |
        | 225 | utf8mb4            | utf8mb4_icelandic_ci     |
        | 226 | utf8mb4            | utf8mb4_latvian_ci       |
        | 227 | utf8mb4            | utf8mb4_romanian_ci      |
        | 228 | utf8mb4            | utf8mb4_slovenian_ci     |
        | 229 | utf8mb4            | utf8mb4_polish_ci        |
        | 230 | utf8mb4            | utf8mb4_estonian_ci      |
        | 231 | utf8mb4            | utf8mb4_spanish_ci       |
        | 232 | utf8mb4            | utf8mb4_swedish_ci       |
        | 233 | utf8mb4            | utf8mb4_turkish_ci       |
        | 234 | utf8mb4            | utf8mb4_czech_ci         |
        | 235 | utf8mb4            | utf8mb4_danish_ci        |
        | 236 | utf8mb4            | utf8mb4_lithuanian_ci    |
        | 237 | utf8mb4            | utf8mb4_slovak_ci        |
        | 238 | utf8mb4            | utf8mb4_spanish2_ci      |
        | 239 | utf8mb4            | utf8mb4_roman_ci         |
        | 240 | utf8mb4            | utf8mb4_persian_ci       |
        | 241 | utf8mb4            | utf8mb4_esperanto_ci     |
        | 242 | utf8mb4            | utf8mb4_hungarian_ci     |
        | 243 | utf8mb4            | utf8mb4_sinhala_ci       |
        | 244 | utf8mb4            | utf8mb4_german2_ci       |
        | 245 | utf8mb4            | utf8mb4_croatian_ci      |
        | 246 | utf8mb4            | utf8mb4_unicode_520_ci   |
        | 247 | utf8mb4            | utf8mb4_vietnamese_ci    |
        | 248 | gb18030            | gb18030_chinese_ci       |
        | 249 | gb18030            | gb18030_bin              |
        | 250 | gb18030            | gb18030_unicode_520_ci   |
        +-----+--------------------+--------------------------+
        222 rows in set (0.17 sec)*/

  /*!
   * @brief character_set_client
   */
  int2store(reinterpret_cast<unsigned char*>(status_vars + offset), 45);
  offset += 2;

  /*!
   * @brief collation_connection
   */
  int2store(reinterpret_cast<unsigned char*>(status_vars + offset), 45);
  offset += 2;

  /*!
   * @brief collation_server
   */
  int2store(reinterpret_cast<unsigned char*>(status_vars + offset), 83);
  offset += 2;

  std::string status_vars_str = std::string{status_vars, status_var_len};
  event->set_status_vars(status_vars_str);
  /********** status vars **********/

  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + QUERY_HEADER_LEN + event->get_status_var_len() + event->get_db_len() + 1 +
                       event->get_sql_statment_len() + event->get_checksum_len();
  auto* common_header =
      new OblogEventHeader(QUERY_EVENT, get_timestamp_sec(record), event_len, this->_cur_pos + event_len);
  event->set_header(common_header);
  free(sql);
  free(status_vars);
  // set crc32
  this->_cur_pos = event->get_header()->get_next_position();
  append_event(this->_event_queue, event);
}

void BinlogConvert::convert_xid_event(ILogRecord* record)
{
  if (this->_filter) {
    if (_within_filtered_transactions) {
      _filter = false;
    }
    return;
  }
  auto* event = new XidEvent();
  event->set_xid(OMS_ATOMIC_INC(this->_xid));
  // set common _header
  uint32_t xid_event_len = COMMON_HEADER_LENGTH + XID_HEADER_LEN + XID_LEN + event->get_checksum_len();
  // mysql 5.7 no _column_count
  auto* common_header =
      new OblogEventHeader(XID_EVENT, get_timestamp_sec(record), xid_event_len, this->_cur_pos + xid_event_len);
  event->set_header(common_header);
  // set crc32
  uint64_t timestamp;
  this->_cur_pos = event->get_header()->get_next_position();
  this->_xid = event->get_xid();
  timestamp = event->get_header()->get_timestamp();
  append_event(this->_event_queue, event);

  if (this->_cur_pos > _meta.max_binlog_size_bytes) {
    this->_binlog_file_index++;
    auto* rotate_event = new RotateEvent(BINLOG_MAGIC_SIZE,
        binlog::CommonUtils::fill_binlog_file_name(this->_binlog_file_index),
        timestamp,
        this->_cur_pos);
    rotate_event->set_op(RotateEvent::ROTATE);
    rotate_event->set_index(this->_binlog_file_index);
    append_event(this->_event_queue, rotate_event);

    std::uint32_t checksum = 0;
    if (Config::instance().binlog_checksum.val()) {
      checksum = COMMON_CHECKSUM_LENGTH;
    }
    this->_cur_pos = BINLOG_START_POS + checksum;
  }
}

void BinlogConvert::convert_table_map_event(ILogRecord* record)
{
  auto* event = new TableMapEvent();

  // fix part
  std::string tb_name = record->tbname();
  // TM_BIT_LEN_EXACT_F
  event->set_flags((1U << 0));
  // variable part

  std::string dbname = get_dbname_without_tenant(record->dbname());
  event->set_db_name(dbname);
  event->set_db_len(dbname.size());

  ITableMeta* table_meta = record->getTableMeta();
  event->set_tb_name(table_meta->getName());
  event->set_tb_len(event->get_tb_name().size());

  int col_count = table_meta->getColCount();
  event->set_column_count(col_count);

  // Make a hash value based on db name + table name
  event->set_table_id(table_id(dbname, tb_name));

  unsigned char cbuf[sizeof(col_count) + 1];
  unsigned char* cbuf_end;

  auto* col_type = static_cast<unsigned char*>(malloc(col_count));
  auto* null_bits = static_cast<unsigned char*>(malloc((col_count + 7) / 8));
  memset(null_bits, 0, (col_count + 7) / 8);
  auto* col_metadata = (unsigned char*)malloc(col_count * 2);
  memset(col_metadata, 0, col_count * 2);
  int col_metadata_len = 0;

  for (int index = 0; index < col_count; index++) {
    IColMeta* col_meta = table_meta->getCol(index);
    // The DRCMessage data type is consistent with the MySQL data type
    int col_data_type = col_meta->getType();
    switch (col_data_type) {
      case OB_TYPE_TINY_BLOB:
      case OB_TYPE_MEDIUM_BLOB:
      case OB_TYPE_LONG_BLOB:
      case OB_TYPE_BLOB:
        col_data_type = OB_TYPE_BLOB;
        break;
      case OB_TYPE_VAR_STRING:
        col_data_type = OB_TYPE_VARCHAR;
        break;
      case OB_TYPE_DATETIME:
        col_data_type = OB_TYPE_DATETIME2;
        break;
      case OB_TYPE_TIME:
        col_data_type = OB_TYPE_TIME2;
        break;
      case OB_TYPE_TIMESTAMP:
        col_data_type = OB_TYPE_TIMESTAMP2;
        break;
      case OB_TYPE_ENUM:
      case OB_TYPE_SET:
        col_data_type = OB_TYPE_STRING;
        break;
      case OB_TYPE_FLOAT:
        /*!
         * @brief https://dev.mysql.com/doc/refman/8.0/en/floating-point-types.html
         * According to the rules, it can be determined that when the precision is greater than or equal to 24,
         * mysql actually uses double to store data, and the expression in binlog is also double
         */
        if (col_meta->getPrecision() > 24) {
          col_data_type = OB_TYPE_DOUBLE;
        }
        break;
      default:
        // do nothing
        break;
    }

    int1store(col_type + index, col_data_type);
    if (!col_meta->isNotNull()) {
      null_bits[(index / 8)] += 1 << (index % 8);
    }

    col_metadata_len += set_column_metadata(col_metadata + col_metadata_len, *col_meta, event->get_tb_name());
  }

  event->set_column_type(col_type);

  // column meta data,field.cc
  event->set_metadata(col_metadata);
  // column meta data size
  event->set_metadata_len(col_metadata_len);
  // null_bits
  event->set_null_bits(null_bits);

  cbuf_end = packet_store_length(cbuf, col_count);

  size_t body_size = (event->get_db_len() + 2) + (event->get_tb_len() + 2);
  body_size += ((cbuf_end - cbuf) + col_count) + ((col_count + 7) / 8);
  // add meta data len
  cbuf_end = packet_store_length(cbuf, col_metadata_len);

  body_size += (cbuf_end - cbuf);

  // add meta data
  body_size += col_metadata_len;
  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + TABLE_MAP_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header =
      new OblogEventHeader(TABLE_MAP_EVENT, get_timestamp_sec(record), event_len, this->_cur_pos + event_len);
  event->set_header(common_header);
  // set crc32
  this->_cur_pos = event->get_header()->get_next_position();
  append_event(this->_event_queue, event);
}

uint64_t BinlogConvert::table_id(const string& db_name, const string& tb_name)
{
  return _table_cache.get_table_id(db_name, tb_name);
}

void BinlogConvert::get_after_images(ILogRecord* record, int col_count, MsgBuf& col_data) const
{
  unsigned int new_col_count = 0;
  size_t data_len = 0;
  StrArray* new_str_buf = record->parsedNewCols();
  BinLogBuf* new_bin_log_buf = record->newCols(new_col_count);

  for (int i = 0; i < col_count; ++i) {
    const char* data;
    if (record->isParsedRecord()) {
      new_str_buf->elementAt(i, data, data_len);
    } else {
      data = new_bin_log_buf[i].buf;
      data_len = new_bin_log_buf[i].buf_used_size;
    }
    col_data.push_back_copy(const_cast<char*>(data), data_len);
  }
}
void BinlogConvert::get_before_images(ILogRecord* record, int col_count, MsgBuf& col_data) const
{
  unsigned int old_col_count = 0;
  size_t data_len = 0;
  StrArray* old_str_buf = record->parsedOldCols();
  BinLogBuf* old_bin_log_buf = record->oldCols(old_col_count);

  for (int i = 0; i < col_count; ++i) {
    const char* data;
    if (record->isParsedRecord()) {
      old_str_buf->elementAt(i, data, data_len);
    } else {
      data = old_bin_log_buf[i].buf;
      data_len = old_bin_log_buf[i].buf_used_size;
    }
    col_data.push_back_copy(const_cast<char*>(data), data_len);
  }
}

size_t col_val_bytes(ILogRecord* record, ITableMeta* table_meta, MsgBuf& before_val, MsgBuf& after_val,
    size_t& before_pos, size_t& after_pos, RowsEventType rows_event_type, unsigned char* before_bitmap,
    unsigned char* after_bitmap)
{
  unsigned int old_col_count = 0;
  unsigned int new_col_count = 0;
  size_t data_len = 0;
  int col_count = table_meta->getColCount();
  size_t col_bytes = 0;

  if (rows_event_type != INSERT) {
    StrArray* old_str_buf = record->parsedOldCols();
    BinLogBuf* old_bin_log_buf = record->oldCols(old_col_count);
    for (int i = 0; i < col_count; ++i) {
      const char* data;
      if (record->isParsedRecord()) {
        old_str_buf->elementAt(i, data, data_len);
      } else {
        data = old_bin_log_buf[i].buf;
        data_len = old_bin_log_buf[i].buf_used_size;
      }
      if (data_len <= 0) {
        if (data == nullptr) {
          before_bitmap[i / 8] |= (0x01 << ((i % 8)));
          continue;
        }
      }
      std::string str(data, data_len);
      before_pos +=
          get_column_val_bytes(*((table_meta->getCol(i))), data_len, str.data(), before_val, table_meta->getName());
    }
    col_bytes += before_pos;
  }

  if (rows_event_type != DELETE) {
    // after
    StrArray* new_str_buf = record->parsedNewCols();
    BinLogBuf* new_bin_log_buf = record->newCols(new_col_count);
    for (int i = 0; i < col_count; ++i) {
      const char* data;
      if (record->isParsedRecord()) {
        new_str_buf->elementAt(i, data, data_len);
      } else {
        data = new_bin_log_buf[i].buf;
        data_len = new_bin_log_buf[i].buf_used_size;
      }
      if (data_len <= 0) {
        if (data == nullptr) {
          after_bitmap[i / 8] |= (0x01 << ((i % 8)));
          continue;
        }
      }
      std::string str(data, data_len);
      after_pos +=
          get_column_val_bytes(*((table_meta->getCol(i))), data_len, str.data(), after_val, table_meta->getName());
    }
    col_bytes += after_pos;
  }

  return col_bytes;
}

void BinlogConvert::convert_write_rows_event(ILogRecord* record)
{
  std::string tb_name = record->tbname();
  ITableMeta* table_meta = record->getTableMeta();
  int col_count = table_meta->getColCount();
  std::string dbname = get_dbname_without_tenant(record->dbname());
  auto* event = new WriteRowsEvent(table_id(dbname, tb_name), STMT_END_F);
  // event body
  event->set_var_header_len(2);
  size_t body_size = 0;
  int col_bytes = (col_count + 7) / 8;
  event->set_after_image_cols(col_bytes);
  body_size += col_bytes;
  auto* bitmap = static_cast<unsigned char*>(malloc(col_bytes));
  fill_bitmap(col_count, col_bytes, bitmap);
  body_size += col_bytes;
  size_t before_pos = 0;
  size_t after_pos = 0;
  body_size += col_val_bytes(record,
      table_meta,
      event->get_before_row(),
      event->get_after_row(),
      before_pos,
      after_pos,
      INSERT,
      nullptr,
      bitmap);

  event->set_before_pos(before_pos);
  event->set_after_pos(after_pos);
  // default full column map
  event->set_columns_after_bitmaps(bitmap);
  event->set_width(col_count);
  body_size += get_packed_integer(col_count);

  // set common _header,no len(after_row)
  uint32_t event_len = COMMON_HEADER_LENGTH + ROWS_HEADER_LEN + VAR_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header =
      new OblogEventHeader(WRITE_ROWS_EVENT, get_timestamp_sec(record), event_len, this->_cur_pos + event_len);
  event->set_header(common_header);
  // set crc32
  event->set_ob_txn(get_transaction_id(record));
  event->set_checkpoint(get_checkpoint_usec(record));
  this->_cur_pos = event->get_header()->get_next_position();
  append_event(this->_event_queue, event);
}

void BinlogConvert::convert_delete_rows_event(ILogRecord* record)
{
  std::string tb_name = record->tbname();
  ITableMeta* table_meta = record->getTableMeta();
  int col_count = table_meta->getColCount();
  std::string dbname = get_dbname_without_tenant(record->dbname());
  auto* event = new DeleteRowsEvent(table_id(dbname, tb_name), STMT_END_F);

  // event body
  event->set_var_header_len(2);
  size_t body_size = 0;
  int col_bytes = (col_count + 7) / 8;
  event->set_before_image_cols(col_bytes);
  body_size += col_bytes;
  auto* bitmap = static_cast<unsigned char*>(malloc(col_bytes));
  fill_bitmap(col_count, col_bytes, bitmap);
  body_size += col_bytes;

  size_t before_pos = 0;
  size_t after_pos = 0;
  body_size += col_val_bytes(record,
      table_meta,
      event->get_before_row(),
      event->get_after_row(),
      before_pos,
      after_pos,
      DELETE,
      bitmap,
      nullptr);

  event->set_before_pos(before_pos);
  event->set_after_pos(after_pos);

  // default full column map
  event->set_columns_before_bitmaps(bitmap);

  event->set_width(col_count);
  body_size += get_packed_integer(col_count);

  // set common _header,no len(after_row)
  uint32_t event_len = COMMON_HEADER_LENGTH + ROWS_HEADER_LEN + VAR_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header =
      new OblogEventHeader(DELETE_ROWS_EVENT, get_timestamp_sec(record), event_len, this->_cur_pos + event_len);
  event->set_header(common_header);
  // set crc32
  event->set_ob_txn(get_transaction_id(record));
  event->set_checkpoint(get_checkpoint_usec(record));
  this->_cur_pos = event->get_header()->get_next_position();
  append_event(this->_event_queue, event);
}

void fill_bitmap(int col_count, int col_bytes, unsigned char* bitmap)
{
  for (int i = 0; i < col_count / 8; ++i) {
    bitmap[i] = 0x00;
  }

  if (col_count / 8 == col_bytes - 1) {
    bitmap[col_bytes - 1] = (0xFF << (col_count % 8));
  }
}

void BinlogConvert::convert_update_rows_event(ILogRecord* record)
{
  std::string tb_name = record->tbname();
  ITableMeta* table_meta = record->getTableMeta();
  int col_count = table_meta->getColCount();
  std::string dbname = get_dbname_without_tenant(record->dbname());
  auto* event = new UpdateRowsEvent(table_id(dbname, tb_name), STMT_END_F);

  // event body
  event->set_var_header_len(2);
  std::size_t body_size = 0;
  int col_bytes = (col_count + 7) / 8;
  event->set_before_image_cols(col_bytes);
  event->set_after_image_cols(col_bytes);
  auto* before_bitmap = static_cast<unsigned char*>(malloc(col_bytes));
  auto* after_bitmap = static_cast<unsigned char*>(malloc(col_bytes));
  fill_bitmap(col_count, col_bytes, before_bitmap);
  fill_bitmap(col_count, col_bytes, after_bitmap);

  size_t before_pos = 0;
  size_t after_pos = 0;
  body_size += col_val_bytes(record,
      table_meta,
      event->get_before_row(),
      event->get_after_row(),
      before_pos,
      after_pos,
      UPDATE,
      before_bitmap,
      after_bitmap);

  event->set_before_pos(before_pos);
  event->set_after_pos(after_pos);

  // default full column map
  event->set_columns_before_bitmaps(before_bitmap);
  event->set_columns_after_bitmaps(after_bitmap);

  body_size += 4 * col_bytes;

  event->set_width(col_count);
  body_size += get_packed_integer(col_count);
  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + ROWS_HEADER_LEN + VAR_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header =
      new OblogEventHeader(UPDATE_ROWS_EVENT, get_timestamp_sec(record), event_len, this->_cur_pos + event_len);
  event->set_header(common_header);
  // set crc32
  event->set_ob_txn(get_transaction_id(record));
  event->set_checkpoint(get_checkpoint_usec(record));

  this->_cur_pos = event->get_header()->get_next_position();
  append_event(this->_event_queue, event);
}

void BinlogConvert::append_event(BlockingQueue<ObLogEvent*>& queue, ObLogEvent* event)
{
  while (!queue.offer(event, _s_config.send_fail_interval_us.val())) {
    OMS_INFO("storage queue full({}), retry...", queue.size(false));
  }
}

void BinlogConvert::do_convert(const std::vector<ILogRecord*>& records)
{
  for (ILogRecord* record : records) {
    size_t size = 0;
#ifdef COMMUNITY_BUILD
    const char* rbuf = record->toString(&size, true);
#else
    const char* rbuf = record->toString(&size, _t_s_lmb, true);
#endif

    if (rbuf == nullptr) {
      OMS_STREAM_ERROR << "failed parse logmsg Record, !!!EXIT!!!";
      stop();
      break;
    }
    int type = record->recordType();
    switch (type) {
      case EBEGIN:
        // GTID_LOG_EVENT -> QUERY_EVENT
        // init GTID
        convert_gtid_log_event(record);
        break;
      case ECOMMIT:
        // Xid Event
        convert_xid_event(record);
        break;
      case EDDL:
        // Query Event
        if (_filter) {
          unsigned int new_col_count = 0;
          BinLogBuf* new_bin_log_buf = record->newCols(new_col_count);
          OMS_STREAM_INFO << "skip ddl:" << new_bin_log_buf->buf;
          if (this->_txn_mapping.second == get_transaction_id(record)) {
            _filter = false;
          }
          break;
        }
        convert_gtid_log_event(record);
        break;
      case EINSERT:
        // WRITE_ROWS_EVENT
        if (_filter) {
          break;
        }
        convert_table_map_event(record);
        convert_write_rows_event(record);
        break;
      case EDELETE:
        // DELETE_ROWS_EVENT

        if (_filter) {
          break;
        }
        convert_table_map_event(record);
        convert_delete_rows_event(record);

        break;
      case EUPDATE:
        // UPDATE_ROWS_EVENT
        if (_filter) {
          break;
        }
        convert_table_map_event(record);
        convert_update_rows_event(record);
        break;
      case HEARTBEAT:
        // skip heartbeat
        logproxy::Counter::instance().mark_checkpoint(get_checkpoint_usec(record));
        logproxy::Counter::instance().mark_timestamp(get_timestamp_usec(record));
        break;
      default:
        OMS_STREAM_ERROR << "Unsupported record type: " << record->recordType();
        if (_s_config.binlog_ignore_unsupported_event.val()) {
          break;
        }
        stop();
        // exit
    }
  }
}

const ConvertMeta& BinlogConvert::get_meta() const
{
  return _meta;
}

void BinlogConvert::set_meta(const ConvertMeta& meta)
{
  _meta = meta;
}

int BinlogConvert::recover(const ConvertMeta& meta, OblogConfig& config)
{
  auto* rotate_event = new RotateEvent(
      BINLOG_MAGIC_SIZE, binlog::CommonUtils::fill_binlog_file_name(_binlog_file_index), Timer::now() / 1000000, 0);
  if (config.start_timestamp_us.val() != 0) {
    rotate_event->get_header()->set_timestamp(config.start_timestamp_us.val() / 1000000);
  }
  uint8_t checksum_flag = Config::instance().binlog_checksum.val() ? CRC32 : OFF;
  vector<BinlogIndexRecord*> index_records;
  defer(release_vector(index_records));
  fetch_index_vector(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_records);

  /*!
   * Condition 1: When the retrieved index records are empty
   *
   * Condition 2: When there is only one binlog file list retrieved and no checkpoint information is dropped at this
   * time, the binlog cannot be recovered and can only be used as an initialization binlog.
   */
  if (index_records.empty() || (index_records.size() == 1 && index_records.back()->get_position() == 0)) {
    /*!
     * There is no binlog file currently, and the initialization action is performed.
     */
    rotate_event->set_op(RotateEvent::INIT);
    rotate_event->set_index(_binlog_file_index);
    _filter = false;
    /*!
     * @brief If the gtid mapping relationship is specified,
     *        we should start the binlog service from the specified mapping relationship
     */
    if (!config.initial_trx_xid.val().empty()) {
      _txn_mapping.first = config.initial_trx_gtid_seq.val();
      _txn_mapping.second = config.initial_trx_xid.val();
      _txn_id = config.initial_trx_gtid_seq.val() - 1;
      if (config.start_timestamp_us.val() != 0) {
        _start_timestamp = config.start_timestamp_us.val();
      } else {
        OMS_ERROR("When specifying gtid mapping, the start time cannot be empty");
        return OMS_FAILED;
      }

      _filter = true;
      _specified_gtids = true;

      OMS_INFO("Specify gtid mapping relationship to start,initial_trx_xid:{},initial_trx_gtid_seq:{}",
          _txn_mapping.second,
          _txn_mapping.first);
    }

    if (checksum_flag == OFF && rotate_event->get_checksum_flag() == CRC32) {
      rotate_event->get_header()->set_event_length(
          rotate_event->get_header()->get_event_length() - rotate_event->get_checksum_len());

      rotate_event->get_header()->set_next_position(
          rotate_event->get_header()->get_next_position() - rotate_event->get_checksum_len());
      rotate_event->set_checksum_flag(OFF);
    }
    append_event(_event_queue, rotate_event);
    return OMS_OK;
  }

  /*!
   * There is no data after the current binlog file is rotated, and the corresponding mapping relationship does not
   * exist. You need to find the mapping relationship in the previous file.
   */
  if (index_records.back()->_current_mapping.second == 0 && index_records.back()->_before_mapping.second == 0 &&
      index_records.size() > 1) {
    _txn_id = index_records.at(index_records.size() - 2)->_current_mapping.second;
    _txn_mapping.first = _txn_id;
    _txn_mapping.second = index_records.at(index_records.size() - 2)->_current_mapping.first;
  } else {
    _txn_id = index_records.back()->_current_mapping.second;
    _txn_mapping.first = _txn_id;
    _txn_mapping.second = index_records.back()->_current_mapping.first;
  }

  uint64_t next_index = index_records.back()->_index + 1;
  std::string binlog = index_records.back()->_file_name;

  uint64_t complete_transaction_pos;
  uint64_t last_complete_txn_id = 0;
  uint64_t start_complete_txn_id = 0;
  bool rotate_existed = false;
  int ret = get_the_last_complete_txn(
      binlog, complete_transaction_pos, last_complete_txn_id, start_complete_txn_id, rotate_existed);
  if (ret != OMS_OK) {
    return OMS_FAILED;
  }

  rotate_event->set_existed(rotate_existed);

  if (_s_config.binlog_recover_backup.val()) {
    ret = backup(binlog, meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, config, next_index - 1);
    if (ret != OMS_OK) {
      OMS_ERROR("Failed to backup of binlog file {} and index file", binlog);
      return OMS_FAILED;
    }
  }
  /*!
   * Truncate the log after finding the position of the last complete transaction
   */
  error_code err;
  fs::resize_file(binlog, complete_transaction_pos, err);
  if (err) {
    OMS_ERROR("Failed to resize file:{} to {}", binlog, complete_transaction_pos);
    return OMS_FAILED;
  }

  OMS_INFO("The last complete transaction id is {},checkpoint:{}", last_complete_txn_id, complete_transaction_pos);
  if (last_complete_txn_id != 0) {
    _txn_id = last_complete_txn_id;
    if (_txn_id == index_records.back()->_current_mapping.second) {
      _txn_mapping.first = _txn_id;
      _txn_mapping.second = index_records.back()->_current_mapping.first;
    } else if (_txn_id == index_records.back()->_before_mapping.second) {
      _txn_mapping.first = _txn_id;
      _txn_mapping.second = index_records.back()->_before_mapping.first;
    } else {
      // Could not find mapping record for transaction
      OMS_STREAM_ERROR << "Could not find mapping record for transaction:" << _txn_id;
      return OMS_FAILED;
    }
  }

  _binlog_file_index = next_index;
  rotate_event->set_op(RotateEvent::ROTATE);
  rotate_event->set_next_binlog_file_name(binlog::CommonUtils::fill_binlog_file_name(_binlog_file_index));
  rotate_event->set_index(next_index);
  /*!
   * @brief Since the rotation is triggered, we need to reset the offset of the conversion.
   * Since the Format Description Event and Previous Gtids Log Event are generated by default after the rotation,
   * the offset is fixed
   */
  std::uint32_t checksum = 0;
  if (Config::instance().binlog_checksum.val()) {
    checksum = COMMON_CHECKSUM_LENGTH;
  }
  _cur_pos = BINLOG_START_POS + checksum;

  rotate_event->get_header()->set_next_position(
      rotate_event->get_header()->get_event_length() + complete_transaction_pos);

  append_event(_event_queue, rotate_event);
  return OMS_OK;
}

int BinlogConvert::backup(
    const std::string& binlog, const std::string& binlog_index, const OblogConfig& config, uint16_t index)
{
  std::string ts = std::to_string(Timer::now_s());
  std::string path = _s_config.binlog_log_bin_basename.val() + RECOVER_BACKUP_PATH;

  error_code err;
  if (!fs::exists(path, err)) {
    fs::create_directory(path, err);
    if (err) {
      OMS_ERROR("Failed to create directory:{} ,reason: {}", path, err.message());
      return OMS_FAILED;
    }
  }
  std::string backup_binlog = path + config.cluster.val() + "_" + config.tenant.val() + "_" +
                              binlog::CommonUtils::fill_binlog_file_name(index) + "_" + ts;

  std::string backup_index = path + config.cluster.val() + "_" + config.tenant.val() + "_" + "index" + "_" + ts;
  fs::copy(binlog, backup_binlog, err);
  if (err) {
    OMS_ERROR("Failed to copy file:{} to {},reason: {}", binlog, backup_binlog, err.message());
    return OMS_FAILED;
  }

  fs::copy(binlog_index, backup_index, err);
  if (err) {
    OMS_ERROR("Failed to copy file:{} to {},reason: {}", binlog, backup_index, err.message());
    return OMS_FAILED;
  }

  return OMS_OK;
}

void BinlogConvert::refresh_table_cache(const string& db_name, const string& tb_name)
{
  _table_cache.refresh_table_id(db_name, tb_name);
}

}  // namespace logproxy
}  // namespace oceanbase
