//
// Created by 花轻 on 2024/10/24.
//

#include "parallel_convert.h"
#include "common_util.h"

#include <RoundRobinThreadAffinedTaskScheduler.h>
#include <YieldingWaitStrategy.h>
#include <binlog_converter.h>
#include <env.h>
namespace oceanbase::binlog {

void BinlogEventHandler::onEvent(BinlogEvent& data, std::int64_t sequence, bool endOfBatch)
{
  std::vector<ObLogEvent*> events = std::move(data.events);
  data.events.clear();
  assert(data.events.empty());
  for (ObLogEvent* event : events) {
    while (!queue.offer(event, s_meta.binlog_config()->read_timeout_us())) {}
  }
  events.clear();
}

void BinlogEventConvertHandler::onEvent(BinlogEvent& binlog_event, std::int64_t sequence)
{
  const uint64_t record_count = binlog_event.records.size();
  for (ILogRecord* record : binlog_event.records) {
    switch (record->recordType()) {
      case EBEGIN:
        // GTID_LOG_EVENT -> QUERY_EVENT
        // init GTID
        parallel_convert_gtid_log_event(record, binlog_event.events, ddl_parser, table_cache);
        break;
      case ECOMMIT:
        // Xid Event
        parallel_convert_xid_event(record, binlog_event.events);
        break;
      case EDDL:
        // GTID_LOG_EVENT -> QUERY_EVENT
        if (!parallel_ddl_need_to_be_stored(record, ddl_parser)) {
          break;
        }
        parallel_convert_gtid_log_event(record, binlog_event.events, ddl_parser, table_cache);
        break;
      case EINSERT:
        parallel_convert_table_map_event(record, binlog_event.events, table_cache);
        parallel_convert_write_rows_event(record, binlog_event.events, table_cache);
        break;
      case EDELETE:
        parallel_convert_table_map_event(record, binlog_event.events, table_cache);
        parallel_convert_delete_rows_event(record, binlog_event.events, table_cache);
        break;
      case EUPDATE:
        parallel_convert_table_map_event(record, binlog_event.events, table_cache);
        parallel_convert_update_rows_event(record, binlog_event.events, table_cache);
        break;
      case HEARTBEAT:
        // // skip heartbeat
        logproxy::Counter::instance().mark_checkpoint(CommonUtils::get_checkpoint_usec(record));
        logproxy::Counter::instance().mark_timestamp(CommonUtils::get_timestamp_usec(record));
        parallel_pass_heartbeat_checkpoint(record, binlog_event.events);
        break;
      default:
        OMS_ERROR("Unsupported record type: {}", record->recordType());
        if (s_config.binlog_ignore_unsupported_event.val()) {
          break;
        }
        throw runtime_error("Unsupported record type");
    }
    obcdc_access->release(record);
  }
  Counter::instance().count_convert(record_count);
  binlog_event.records.clear();
}

void ConvertExceptionHandler::handleEventException(const std::exception& ex, std::int64_t sequence, BinlogEvent& evt)
{
  OMS_ERROR("Handle event exception: {},sequence :{}, trace :{}", ex.what(), sequence, CommonUtils::get_stack_trace());
  converter.stop_converter();
}

void ConvertExceptionHandler::handleOnStartException(const std::exception& ex)
{
  OMS_ERROR("Handle event exception: {},sequence :{}, trace :{}", ex.what(), CommonUtils::get_stack_trace());
  converter.stop_converter();
}

void ConvertExceptionHandler::handleOnShutdownException(const std::exception& ex)
{
  OMS_ERROR("Handle event exception: {},sequence :{}, trace :{}", ex.what(), CommonUtils::get_stack_trace());
  converter.stop_converter();
}

void ConvertExceptionHandler::handleOnTimeoutException(const std::exception& ex, std::int64_t sequence)
{
  OMS_ERROR("Handle event exception: {},sequence :{}, trace :{}", ex.what(), sequence, CommonUtils::get_stack_trace());
  converter.stop_converter();
}

inline bool parallel_convert_gtid_log_event(ILogRecord* record, std::vector<ObLogEvent*>& events,
    binlog::DdlParser& ddl_parser, TableCache& table_cache, bool is_ddl)
{
  auto* gtid_log_event = new GtidLogEvent();
  gtid_log_event->set_gtid_uuid(s_meta.binlog_config()->master_server_uuid());

  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + GTID_HEADER_LEN + gtid_log_event->get_checksum_len();
  auto* common_header = new OblogEventHeader(GTID_LOG_EVENT, CommonUtils::get_timestamp_sec(record), event_len, 0);
  gtid_log_event->set_header(common_header);
  gtid_log_event->set_ob_txn(CommonUtils::get_transaction_id(record));
  gtid_log_event->set_checkpoint(CommonUtils::get_checkpoint_usec(record));
  gtid_log_event->set_last_committed(record->getTimestamp());
  gtid_log_event->set_sequence_number(record->getRecordUsec());
  events.push_back(gtid_log_event);
  parallel_convert_query_event(record, events, ddl_parser, table_cache);
  return true;
}

inline void parallel_convert_query_event(
    ILogRecord* record, std::vector<ObLogEvent*>& events, binlog::DdlParser& ddl_parser, TableCache& table_cache)
{
  char* sql = nullptr;
  size_t sql_statment_len = 0;
  bool is_ddl_event = false;
  if (record->recordType() == EBEGIN) {
    sql = static_cast<char*>(malloc(BEGIN_VAR_LEN));
    write_string(sql, BEGIN_VAR_LEN, BEGIN_VAR, BEGIN_VAR_LEN);
    sql_statment_len = BEGIN_VAR_LEN;
  } else {
    is_ddl_event = true;
    unsigned int new_col_count = 0;
    BinLogBuf* new_bin_log_buf = record->newCols(new_col_count);
    sql_statment_len = new_bin_log_buf->buf_used_size;
    sql = static_cast<char*>(malloc(new_bin_log_buf->buf_used_size));
    write_string(sql, sql_statment_len, new_bin_log_buf->buf, sql_statment_len);
    if (s_config.binlog_ddl_convert.val()) {
      std::string convert_sql;
      std::string ddl = std::string{sql, sql_statment_len};
      int convert_ret = ddl_parser.parse(ddl, convert_sql);
      if (convert_ret != OMS_OK) {
        convert_sql = std::string{sql, sql_statment_len};
        // etransfer failed to convert incremental DDL, using untransformed DDL
        OMS_WARN("Failed to convert incremental DDL, using untransformed DDL: {}", convert_sql);
      }
      free(sql);
      sql = nullptr;
      sql_statment_len = convert_sql.size();
      sql = static_cast<char*>(malloc(sql_statment_len));
      write_string(sql, sql_statment_len, convert_sql.c_str(), sql_statment_len);
      table_cache.refresh_table_id(
          CommonUtils::get_dbname_without_tenant(record->dbname(), s_meta.tenant()), record->tbname());
    }
  }

  std::string dbname = CommonUtils::get_dbname_without_tenant(record->dbname(), s_meta.tenant());
  std::string ddl = std::string{sql, sql_statment_len};
  free(sql);
  auto* event = new QueryEvent(dbname, ddl);
  event->set_sql_statment_len(sql_statment_len);
  event->set_query_exec_time(0);
  event->set_thread_id(record->getThreadId());
  event->mark_ddl(is_ddl_event);

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
  uint64_t timestamp = CommonUtils::get_timestamp_sec(record);
  auto* common_header = new OblogEventHeader(QUERY_EVENT, timestamp, event_len, 0);
  event->set_header(common_header);
  free(status_vars);
  events.push_back(event);
}

inline bool parallel_ddl_need_to_be_stored(ILogRecord* record, binlog::DdlParser& ddl_parser)
{
  char* sql = nullptr;
  defer(free(sql));
  size_t sql_statment_len = 0;
  unsigned int new_col_count = 0;
  BinLogBuf* new_bin_log_buf = record->newCols(new_col_count);
  sql_statment_len = new_bin_log_buf->buf_used_size;
  sql = static_cast<char*>(malloc(new_bin_log_buf->buf_used_size));
  write_string(sql, sql_statment_len, new_bin_log_buf->buf, sql_statment_len);
  if (s_config.binlog_ddl_convert.val()) {
    std::string convert_sql;
    std::string ddl = std::string{sql, sql_statment_len};
    int convert_ret = ddl_parser.parse(ddl, convert_sql);
    if (s_config.binlog_ddl_convert_ignore_unsupported_ddl.val()) {
      if (convert_ret != OMS_OK) {
        OMS_INFO("A DDL event that is not supported by the downstream is encountered and is empty after "
                 "conversion,original sql:{}",
            ddl);
        return false;
      }
    }
    return true;
  } else {
    return true;
  }
}

inline void parallel_convert_xid_event(ILogRecord* record, std::vector<ObLogEvent*>& events)
{
  auto* event = new XidEvent();
  // set common _header
  uint32_t xid_event_len = COMMON_HEADER_LENGTH + XID_HEADER_LEN + XID_LEN + event->get_checksum_len();
  // mysql 5.7 no _column_count
  auto* common_header = new OblogEventHeader(XID_EVENT, CommonUtils::get_timestamp_sec(record), xid_event_len, 0);
  event->set_header(common_header);
  events.push_back(event);
}

inline void parallel_convert_table_map_event(
    ILogRecord* record, std::vector<ObLogEvent*>& events, TableCache& table_cache)
{
  auto* event = new TableMapEvent();

  // fix part
  std::string tb_name = record->tbname();
  // TM_BIT_LEN_EXACT_F
  event->set_flags((1U << 0));
  // variable part

  std::string dbname = CommonUtils::get_dbname_without_tenant(record->dbname(), s_meta.tenant());
  event->set_db_name(dbname);
  event->set_db_len(dbname.size());

  ITableMeta* table_meta = record->getTableMeta();
  event->set_tb_name(table_meta->getName());
  event->set_tb_len(event->get_tb_name().size());

  int col_count = table_meta->getColCount();
  event->set_column_count(col_count);

  // Make a hash value based on db name + table name
  event->set_table_id(table_cache.get_table_id(dbname, tb_name));
  unsigned char cbuf[sizeof(col_count) + 1];

  auto* col_type = static_cast<unsigned char*>(malloc(col_count));
  auto* null_bits = static_cast<unsigned char*>(malloc((col_count + 7) / 8));
  memset(null_bits, 0, (col_count + 7) / 8);
  auto* col_metadata = static_cast<unsigned char*>(malloc(col_count * 2));
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

  unsigned char* cbuf_end = packet_store_length(cbuf, col_count);

  size_t body_size = (event->get_db_len() + 2) + (event->get_tb_len() + 2);
  body_size += ((cbuf_end - cbuf) + col_count) + ((col_count + 7) / 8);
  // add meta data len
  cbuf_end = packet_store_length(cbuf, col_metadata_len);

  body_size += (cbuf_end - cbuf);

  // add meta data
  body_size += col_metadata_len;
  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + TABLE_MAP_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header = new OblogEventHeader(TABLE_MAP_EVENT, CommonUtils::get_timestamp_sec(record), event_len, 0);
  event->set_header(common_header);
  events.push_back(event);
}
inline void parallel_convert_write_rows_event(
    ILogRecord* record, std::vector<ObLogEvent*>& events, TableCache& table_cache)
{
  std::string tb_name = record->tbname();
  // event body
  ITableMeta* table_meta = record->getTableMeta();
  int col_count = table_meta->getColCount();
  std::string dbname = CommonUtils::get_dbname_without_tenant(record->dbname(), s_meta.tenant());
  auto* event = new WriteRowsEvent(table_cache.get_table_id(dbname, tb_name), STMT_END_F);
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
  // used for parameters placeholder
  unsigned char* partial_cols_bitmap = nullptr;
  size_t partial_cols_bytes = 0;
  body_size += col_val_bytes(record,
      table_meta,
      event->get_before_row(),
      event->get_after_row(),
      before_pos,
      after_pos,
      INSERT,
      nullptr,
      bitmap,
      partial_cols_bitmap,
      partial_cols_bytes);

  event->set_before_pos(before_pos);
  event->set_after_pos(after_pos);
  // default full column map
  event->set_columns_after_bitmaps(bitmap);
  event->set_width(col_count);
  body_size += get_packed_integer(col_count);

  // set common _header,no len(after_row)
  uint32_t event_len = COMMON_HEADER_LENGTH + ROWS_HEADER_LEN + VAR_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header = new OblogEventHeader(WRITE_ROWS_EVENT, CommonUtils::get_timestamp_sec(record), event_len, 0);
  event->set_header(common_header);
  // set crc32
  event->set_ob_txn(CommonUtils::get_transaction_id(record));
  event->set_checkpoint(CommonUtils::get_checkpoint_usec(record));
  events.push_back(event);
}
inline void parallel_convert_delete_rows_event(
    ILogRecord* record, std::vector<ObLogEvent*>& events, TableCache& table_cache)
{
  std::string tb_name = record->tbname();
  ITableMeta* table_meta = record->getTableMeta();
  int col_count = table_meta->getColCount();
  std::string dbname = CommonUtils::get_dbname_without_tenant(record->dbname(), s_meta.tenant());
  auto* event = new DeleteRowsEvent(table_cache.get_table_id(dbname, tb_name), STMT_END_F);

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
  // used for parameters placeholder
  unsigned char* partial_cols_bitmap = nullptr;
  size_t partial_cols_bytes = 0;
  body_size += col_val_bytes(record,
      table_meta,
      event->get_before_row(),
      event->get_after_row(),
      before_pos,
      after_pos,
      DELETE,
      bitmap,
      nullptr,
      partial_cols_bitmap,
      partial_cols_bytes);

  event->set_before_pos(before_pos);
  event->set_after_pos(after_pos);

  // default full column map
  event->set_columns_before_bitmaps(bitmap);

  event->set_width(col_count);
  body_size += get_packed_integer(col_count);

  // set common _header,no len(after_row)
  uint32_t event_len = COMMON_HEADER_LENGTH + ROWS_HEADER_LEN + VAR_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header = new OblogEventHeader(DELETE_ROWS_EVENT, CommonUtils::get_timestamp_sec(record), event_len, 0);
  event->set_header(common_header);
  // set crc32
  event->set_ob_txn(CommonUtils::get_transaction_id(record));
  event->set_checkpoint(CommonUtils::get_checkpoint_usec(record));
  events.push_back(event);
}

inline void parallel_pass_heartbeat_checkpoint(ILogRecord* record, std::vector<ObLogEvent*>& events)
{
  auto* event = new HeartbeatEvent();
  event->set_checkpoint(CommonUtils::get_checkpoint_usec(record));
  events.push_back(event);
}

inline void parallel_convert_update_rows_event(
    ILogRecord* record, std::vector<ObLogEvent*>& events, TableCache& table_cache)
{
  std::string tb_name = record->tbname();
  EventType update_type = UPDATE_ROWS_EVENT;

  // event body
  ITableMeta* table_meta = record->getTableMeta();
  int col_count = table_meta->getColCount();
  std::string dbname = CommonUtils::get_dbname_without_tenant(record->dbname(), s_meta.tenant());
  auto* event = new UpdateRowsEvent(table_cache.get_table_id(dbname, tb_name), STMT_END_F);

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
  unsigned char* partial_cols_bitmap = nullptr;
  size_t partial_cols_bytes = 0;
  body_size += col_val_bytes(record,
      table_meta,
      event->get_before_row(),
      event->get_after_row(),
      before_pos,
      after_pos,
      UPDATE,
      before_bitmap,
      after_bitmap,
      partial_cols_bitmap,
      partial_cols_bytes);

  event->set_before_pos(before_pos);
  event->set_after_pos(after_pos);

  // default full column map
  event->set_columns_before_bitmaps(before_bitmap);
  event->set_columns_after_bitmaps(after_bitmap);
  if (nullptr != partial_cols_bitmap) {
    update_type = PARTIAL_UPDATE_ROWS_EVENT;
    event->set_partial_columns_bitmaps(partial_cols_bitmap, partial_cols_bytes);
    // value_options
    body_size += 1;
    // partial_columns
    body_size += partial_cols_bytes;
  }

  // before_image_cols + after_image_cols + columns_before_bitmaps + columns_after_bitmaps
  body_size += 4 * col_bytes;

  event->set_width(col_count);
  body_size += get_packed_integer(col_count);
  // set common _header
  uint32_t event_len = COMMON_HEADER_LENGTH + ROWS_HEADER_LEN + VAR_HEADER_LEN + body_size + event->get_checksum_len();
  auto* common_header = new OblogEventHeader(update_type, CommonUtils::get_timestamp_sec(record), event_len, 0);
  event->set_header(common_header);
  // set crc32
  event->set_ob_txn(CommonUtils::get_transaction_id(record));
  event->set_checkpoint(CommonUtils::get_checkpoint_usec(record));

  events.push_back(event);
}

inline void parallel_do_convert(int64_t seq, shared_ptr<Disruptor::disruptor<BinlogEvent>>& disruptor,
    IObCdcAccess*& obcdc, DdlParser& ddl_parser, TableCache& table_cache)
{
  auto ringBuffer = disruptor->ringBuffer();
  auto binlog_event = (*ringBuffer)[seq];
  const uint64_t record_count = binlog_event.records.size();
  for (ILogRecord* record : binlog_event.records) {
    int type = record->recordType();
    switch (type) {
      case EBEGIN:
        // GTID_LOG_EVENT -> QUERY_EVENT
        // init GTID
        parallel_convert_gtid_log_event(record, binlog_event.events, ddl_parser, table_cache);
        break;
      case ECOMMIT:
        // Xid Event
        parallel_convert_xid_event(record, binlog_event.events);
        break;
      case EDDL:
        // GTID_LOG_EVENT -> QUERY_EVENT
        if (!parallel_ddl_need_to_be_stored(record, ddl_parser)) {
          break;
        }
        parallel_convert_gtid_log_event(record, binlog_event.events, ddl_parser, table_cache);
        break;
      case EINSERT:
        parallel_convert_table_map_event(record, binlog_event.events, table_cache);
        parallel_convert_write_rows_event(record, binlog_event.events, table_cache);
        break;
      case EDELETE:
        parallel_convert_table_map_event(record, binlog_event.events, table_cache);
        parallel_convert_delete_rows_event(record, binlog_event.events, table_cache);
        break;
      case EUPDATE:
        parallel_convert_table_map_event(record, binlog_event.events, table_cache);
        parallel_convert_update_rows_event(record, binlog_event.events, table_cache);
        break;
      case HEARTBEAT:
        // // skip heartbeat
        logproxy::Counter::instance().mark_checkpoint(CommonUtils::get_checkpoint_usec(record));
        logproxy::Counter::instance().mark_timestamp(CommonUtils::get_timestamp_usec(record));
        parallel_pass_heartbeat_checkpoint(record, binlog_event.events);
        break;
      default:
        OMS_ERROR("Unsupported record type: {}", record->recordType());
        if (s_config.binlog_ignore_unsupported_event.val()) {
          break;
        }
    }
    obcdc->release(record);
  }
  Counter::instance().count_convert(record_count);
  binlog_event.records.clear();
  (*ringBuffer)[seq] = binlog_event;
  ringBuffer->publish(seq);
}
ParallelConvert::ParallelConvert(
    BinlogConverter& converter, BlockingQueue<ILogRecord*>& rqueue, BlockingQueue<ObLogEvent*>& event_queue)
    : Thread("BinlogConvert"), _converter(converter), _obcdc(nullptr), _rqueue(rqueue), _event_queue(event_queue)
{}

int ParallelConvert::init(IObCdcAccess* obcdc)
{
  _task_scheduler = std::make_shared<Disruptor::RoundRobinThreadAffinedTaskScheduler>();

  if (s_meta.binlog_config()->binlog_convert_ring_buffer_size() < 1) {
    OMS_ERROR("binlog_convert_ring_buffer_size must not be less than 1");
    return OMS_FAILED;
  }

  if (!Disruptor::Util::isPowerOf2(s_meta.binlog_config()->binlog_convert_ring_buffer_size())) {
    OMS_ERROR("binlog_convert_ring_buffer_size must be a power of 2");
    return OMS_FAILED;
  }
  _disruptor = std::make_shared<Disruptor::disruptor<BinlogEvent>>(
      create_binlog_event, s_meta.binlog_config()->binlog_convert_ring_buffer_size(), _task_scheduler);
  _binlog_event_handler = std::make_shared<BinlogEventHandler>(_event_queue);

  for (int64_t i = 0; i < s_meta.binlog_config()->binlog_convert_number_of_concurrences(); ++i) {
    _convert_handler.emplace_back(std::make_shared<BinlogEventConvertHandler>(_ddl_parser, _table_cache, _obcdc));
  }
  auto exception_handle = std::make_shared<ConvertExceptionHandler>(_converter);
  _disruptor->handleExceptionsWith(exception_handle);
  _disruptor->handleEventsWithWorkerPool(_convert_handler)->then(_binlog_event_handler);
  _task_scheduler->start(s_meta.binlog_config()->binlog_convert_thread_size());
  _disruptor->start();
  _obcdc = obcdc;
  Counter::instance().register_gauge("NEventQ", [this]() { return _event_queue.size(); });
  Counter::instance().register_gauge("ConvertRingBufferQ",
      [this]() { return _disruptor->bufferSize() - _disruptor->ringBuffer()->getRemainingCapacity(); });

  if (s_config.binlog_ddl_convert.val()) {
    if (this->_ddl_parser.init() != OMS_OK) {
      OMS_ERROR("Failed to init ddl parser");
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}
void ParallelConvert::stop()
{
  // Fix dima: 2025010800106932017
  // If all converter threads exit at the same time, the records in the ringbuffer will never bee handled.
  _disruptor->shutdown(std::chrono::milliseconds(60 * 1000));
  _task_scheduler->stop();
  Thread::stop();
}
void ParallelConvert::run()
{
  std::vector<ILogRecord*> records;
  while (is_run()) {
    _stage_timer.reset();
    records.reserve(s_meta.binlog_config()->read_wait_num());
    records.clear();
    while (!_rqueue.poll(records, s_meta.binlog_config()->read_timeout_us()) || records.empty()) {
      if (!is_run()) {
        OMS_ERROR("Binlog convert thread has been stopped.");
        break;
      }
      OMS_DEBUG("Empty log Record queue put by clog reader routine , retry... {}", _rqueue.size(false));
    }
    auto ringBuffer = _disruptor->ringBuffer();
    auto seq = ringBuffer->next();
    (*ringBuffer)[seq].records = std::move(records);
    ringBuffer->publish(seq);
  }
  _converter.stop_converter();
}
}