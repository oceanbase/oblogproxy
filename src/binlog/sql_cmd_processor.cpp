/**
 * Copyright (c) 2024 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "sql_cmd_processor.h"

#include <algorithm>
#include <unordered_map>
#include <json/writer.h>

#include "binlog_index.h"
#include "common_util.h"
#include "ob_log_event.h"
#include "binlog_state_machine.h"
#include "env.h"                            // g_bc_executor
#include "binlog-instance/binlog_dumper.h"  // BINLOG_FATAL_ERROR
#include "instance_client.h"
#include "sql/show_binlog_events.h"
#include "sql/purge_binlog.h"
#include "sql/show_binlog_server.h"
#include "sql/drop_binlog.h"
#include "sql/show_binlog_status.h"
#include "sql/Expr.h"
#include "instance_helper.h"
#include "binlog-instance/binlog_converter.h"
#include "password.h"

#include <metric/prometheus.h>

#define UTF8_CS 33
#define BINARY_CS 63

namespace oceanbase::binlog {

static std::unordered_map<hsql::StatementType, SqlCmdProcessor*> _obm_supported_sql_cmd_processors = {
    {hsql::StatementType::COM_SHOW_BINLOG_SERVER, &ShowBinlogServerProcessor::instance()},
    {hsql::StatementType::COM_CREATE_BINLOG, &CreateBinlogProcessor::instance()},
    {hsql::StatementType::COM_DROP_BINLOG, &DropBinlogProcessor::instance()},
    {hsql::StatementType::COM_SHOW_BINLOG_STAT, &ShowBinlogStatusProcessor::instance()},
    {hsql::StatementType::COM_PURGE_BINLOG, &PurgeBinaryLogsProcessor::instance()},
    {hsql::StatementType::COM_ALTER_BINLOG, &AlterBinlogProcessor::instance()},
    {hsql::StatementType::COM_CREATE_BINLOG_INSTANCE, &CreateBinlogInstanceProcessor::instance()},
    {hsql::StatementType::COM_ALTER_BINLOG_INSTANCE, &AlterBinlogInstanceProcessor::instance()},
    {hsql::StatementType::COM_START_BINLOG_INSTANCE, &StartBinlogInstanceProcessor::instance()},
    {hsql::StatementType::COM_STOP_BINLOG_INSTANCE, &StopBinlogInstanceProcessor::instance()},
    {hsql::StatementType::COM_DROP_BINLOG_INSTANCE, &DropBinlogInstanceProcessor::instance()},
    {hsql::StatementType::COM_SHOW_BINLOG_INSTANCE, &ShowBinlogInstanceProcessor::instance()},
    {hsql::StatementType::COM_SHOW_PROCESS_LIST, &ShowProcesslistProcessor::instance()},
    {hsql::StatementType::COM_SHOW_DUMP_LIST, &ShowDumplistProcessor::instance()},
    {hsql::StatementType::COM_KILL, &KillProcessor::instance()},
    {hsql::StatementType::COM_SHOW_NODES, &ShowNodesProcessor::instance()},
    {hsql::StatementType::COM_SHOW, &ShowVarProcessor::instance()},
    {hsql::StatementType::COM_SWITCH_MASTER, &SwitchMasterInstanceProcessor::instance()},
    {hsql::StatementType::COM_SET_PASSWORD, &SetPasswordProcessor::instance()}};

static std::unordered_map<hsql::StatementType, SqlCmdProcessor*> _obi_supported_sql_cmd_processors = {
    {hsql::StatementType::COM_SHOW_BINLOGS, &ShowBinaryLogsProcessor::instance()},
    {hsql::StatementType::COM_SHOW_BINLOG_EVENTS, &ShowBinlogEventsProcessor::instance()},
    {hsql::StatementType::COM_SHOW_MASTER_STAT, &ShowMasterStatusProcessor::instance()},
    {hsql::StatementType::COM_SHOW_SLAVE_STAT, &ShowSlaveStatusProcessor::instance()},
    {hsql::StatementType::COM_PURGE_BINLOG, &PurgeBinaryLogsProcessor::instance()},
    {hsql::StatementType::COM_SELECT, &SelectProcessor::instance()},
    {hsql::StatementType::COM_SET, &SetVarProcessor::instance()},
    {hsql::StatementType::COM_SHOW, &ShowVarProcessor::instance()},
    {hsql::StatementType::COM_REPORT, &ReportProcessor::instance()},
    {hsql::StatementType::COM_SHOW_PROCESS_LIST, &ShowProcesslistProcessor::instance()},
    {hsql::StatementType::COM_SHOW_DUMP_LIST, &ShowDumplistProcessor::instance()},
    {hsql::StatementType::COM_KILL, &KillProcessor::instance()}};

SqlCmdProcessor* sql_cmd_processor(Connection::ConnectionType conn_type, hsql::StatementType type)
{
  std::unordered_map<hsql::StatementType, SqlCmdProcessor*> supported_sql_cmd_processors =
      Connection::ConnectionType::OBM == conn_type ? _obm_supported_sql_cmd_processors
                                                   : _obi_supported_sql_cmd_processors;
  auto iter = supported_sql_cmd_processors.find(type);
  if (iter != supported_sql_cmd_processors.end()) {
    return iter->second;
  }
  return nullptr;
}

IoResult ShowBinaryLogsProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  uint16_t utf8_cs = 33;
  uint16_t binary_cs = 63;

  ColumnPacket log_name_column_packet{
      "Log_name", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket file_size_column_packet{"File_size",
      "",
      binary_cs,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};

  if (conn->send_result_metadata({log_name_column_packet, file_size_column_packet}) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  vector<BinlogIndexRecord*> index_records;
  defer(release_vector(index_records));
  int ret = g_index_manager->fetch_index_vector(index_records);
  if (ret != OMS_OK) {
    return conn->send_eof_packet();  // TODO: eof packet or error packet?
  }

  for (const auto& record : index_records) {
    conn->start_row();
    conn->store_string(CommonUtils::fill_binlog_file_name(record->get_index()));
    conn->store_uint64(record->get_position());
    IoResult send_ret = conn->send_row();
    if (send_ret != IoResult::SUCCESS) {
      return send_ret;
    }
  }
  return conn->send_eof_packet();
}

IoResult ShowBinlogEventsProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  uint16_t utf8_cs = 33;
  uint16_t binary_cs = 63;

  ColumnPacket log_name_column_packet{
      "Log_name", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket pos_column_packet{"Pos",
      "",
      binary_cs,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket event_type_column_packet{
      "Event_type", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket server_id_column_packet{
      "Server_id", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket end_log_pos_column_packet{
      "End_log_pos", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket info_column_packet{
      "Info", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};

  if (conn->send_result_metadata({log_name_column_packet,
          pos_column_packet,
          event_type_column_packet,
          server_id_column_packet,
          end_log_pos_column_packet,
          info_column_packet}) != IoResult::SUCCESS) {
    OMS_ERROR("Failed to send metadata");
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowBinlogEventsStatement*)statement;
  std::string binlog_file;
  std::string start_pos;
  std::string offset_str;
  std::string limit_str;

  if (p_statement->binlog_file != nullptr) {
    binlog_file = p_statement->binlog_file->get_value();
  }

  if (p_statement->start_pos != nullptr) {
    start_pos = p_statement->start_pos->get_value();
  }

  if (p_statement->limit != nullptr) {
    if (p_statement->limit->offset != nullptr) {
      offset_str = p_statement->limit->offset->get_value();
    }
    if (p_statement->limit->limit != nullptr) {
      limit_str = p_statement->limit->limit->get_value();
    }
  }
  OMS_INFO("{}: [show binlog events] binlog file: {}, start pos: {}, offset: {}, limit: {}",
      conn->trace_id(),
      binlog_file,
      start_pos,
      offset_str,
      limit_str);

  uint64_t end_pos = 0;
  std::string binlog_file_full_path;
  BinlogIndexRecord index_record;
  int ret = binlog_file.empty() ? g_index_manager->get_first_index(index_record)
                                : g_index_manager->get_index(binlog_file, index_record);
  if (OMS_FAILED == ret) {
    OMS_ERROR("{}: Could not find target binlog file: {}", conn->trace_id(), binlog_file);
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND,
        "Error when executing command SHOW BINLOG EVENTS: Could not find target binlog file.",
        "HY000");
  }
  binlog_file = binlog_file.empty() ? CommonUtils::fill_binlog_file_name(index_record.get_index()) : binlog_file;
  end_pos = index_record.get_position();
  binlog_file_full_path = index_record.get_file_name();

  uint64_t position = BINLOG_MAGIC_SIZE;
  if (!start_pos.empty()) {
    position = std::stoull(start_pos);
  }

  uint64_t limit = UINT64_MAX;
  if (!limit_str.empty()) {
    limit = std::stoull(limit_str);
  }

  uint64_t offset = 0;
  if (!offset_str.empty()) {
    offset = std::stoull(offset_str);
  }
  OMS_INFO("{}: [show binlog events] binlog file: {}, binlog file path: {}, start pos: {}, end pos: {}, offset: {}, "
           "limit: {}",
      conn->trace_id(),
      binlog_file,
      binlog_file_full_path,
      position,
      end_pos,
      offset,
      limit);

  bool error = false;
  FILE* fp = logproxy::FsUtil::fopen_binary(binlog_file_full_path);
  if (fp == nullptr) {
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND,
        "Error when executing command SHOW BINLOG EVENTS: Could not find target binlog file",
        "HY000");
  }

  unsigned char magic[BINLOG_MAGIC_SIZE];
  // read magic number
  logproxy::FsUtil::read_file(fp, magic, 0, sizeof(magic));

  if (memcmp(magic, binlog_magic, sizeof(magic)) != 0) {
    OMS_ERROR("{}: [show binlog events] The binlog file format is invalid: {}", conn->trace_id(), binlog_file);
    logproxy::FsUtil::fclose_binary(fp);
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND, "The binlog file format is invalid.", "HY000");
  }

  std::vector<ObLogEvent*> binlog_events;
  seek_events(binlog_file_full_path, binlog_events, FORMAT_DESCRIPTION_EVENT, true);
  if (binlog_events.empty()) {
    OMS_ERROR("{}: Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.", conn->trace_id());
    logproxy::FsUtil::fclose_binary(fp);
    release_vector(binlog_events);
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND,
        "Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.",
        "HY000");
  }

  auto* format_event = dynamic_cast<FormatDescriptionEvent*>(binlog_events.at(0));
  uint8_t checksum_flag = format_event->get_checksum_flag();
  OMS_INFO("{}: [show binlog events] The current checksum of the binlog file is: {}", conn->trace_id(), checksum_flag);
  release_vector(binlog_events);

  uint64_t index = 0;
  uint64_t count = 0;
  int64_t pos = position;

  OMS_INFO("{}: [show binlog events] start pos: {}, end pos: {}", conn->trace_id(), pos, end_pos);
  unsigned char ev_header_buff[COMMON_HEADER_LENGTH];
  while (pos < end_pos) {
    // read common header
    ret = logproxy::FsUtil::read_file(fp, ev_header_buff, pos, sizeof(ev_header_buff));
    if (ret != OMS_OK) {
      OMS_ERROR("{}: [show binlog events] Failed to read common header, pos: {}", conn->trace_id(), pos);
      error = true;
      break;
    }
    OblogEventHeader header = OblogEventHeader();
    header.deserialize(ev_header_buff);
    auto event = std::unique_ptr<unsigned char[]>(new unsigned char[header.get_event_length()]);
    ret = FsUtil::read_file(fp, event.get(), pos, header.get_event_length());
    if (ret != OMS_OK) {
      OMS_ERROR("{}: [show binlog events] Failed to read event data, pos: {}, header: {}",
          conn->trace_id(),
          pos,
          header.str_format());
      error = true;
      break;
    }

    if (count >= limit) {
      break;
    }

    uint64_t event_pos = pos;
    std::string event_type = event_type_to_str(header.get_type_code());
    std::string server_id = ::to_string(header.get_server_id());
    uint64_t event_end_pos = header.get_next_position();
    std::string info;
    if (index >= offset) {
      switch (header.get_type_code()) {
        case QUERY_EVENT: {
          auto query_event = QueryEvent("", "");
          query_event.set_checksum_flag(checksum_flag);
          query_event.deserialize(event.get());
          info = query_event.print_event_info();
          break;
        }
        case ROTATE_EVENT: {
          auto rotate_event = RotateEvent(0, "", 0, 0);
          rotate_event.set_checksum_flag(checksum_flag);
          rotate_event.deserialize(event.get());
          info = rotate_event.print_event_info();
          break;
        }
        case FORMAT_DESCRIPTION_EVENT: {
          auto format_description_event = FormatDescriptionEvent();
          format_description_event.deserialize(event.get());
          info = format_description_event.print_event_info();
          break;
        }
        case XID_EVENT: {
          auto xid_event = XidEvent();
          xid_event.set_checksum_flag(checksum_flag);
          xid_event.deserialize(event.get());
          info = xid_event.print_event_info();
          break;
        }
        case TABLE_MAP_EVENT: {
          auto table_map_event = TableMapEvent();
          table_map_event.set_checksum_flag(checksum_flag);
          table_map_event.deserialize(event.get());
          info = table_map_event.print_event_info();
          break;
        }
        case WRITE_ROWS_EVENT: {
          auto write_rows_event = WriteRowsEvent(0, 0);
          write_rows_event.set_checksum_flag(checksum_flag);
          write_rows_event.deserialize(event.get());
          info = write_rows_event.print_event_info();
          break;
        }
        case UPDATE_ROWS_EVENT:
        case PARTIAL_UPDATE_ROWS_EVENT: {
          auto update_rows_event = UpdateRowsEvent(0, 0);
          update_rows_event.set_checksum_flag(checksum_flag);
          update_rows_event.deserialize(event.get());
          info = update_rows_event.print_event_info();
          break;
        }
        case DELETE_ROWS_EVENT: {
          auto delete_row_event = DeleteRowsEvent(0, 0);
          delete_row_event.set_checksum_flag(checksum_flag);
          delete_row_event.deserialize(event.get());
          info = delete_row_event.print_event_info();
          break;
        }
        case GTID_LOG_EVENT: {
          auto gtid_log_event = GtidLogEvent();
          gtid_log_event.set_checksum_flag(checksum_flag);
          gtid_log_event.deserialize(event.get());
          info = gtid_log_event.print_event_info();
          break;
        }
        case PREVIOUS_GTIDS_LOG_EVENT: {
          auto previous_gtids_log_event = PreviousGtidsLogEvent();
          previous_gtids_log_event.set_checksum_flag(checksum_flag);
          previous_gtids_log_event.deserialize(event.get());
          info = previous_gtids_log_event.print_event_info();
          break;
        }
        default:
          OMS_ERROR("{}: Unknown event type: {}", conn->trace_id(), header.get_type_code());
          break;
      }
      count++;

      // send row packet
      conn->start_row();
      conn->store_string(binlog_file);
      conn->store_uint64(event_pos);
      conn->store_string(event_type);
      conn->store_string(server_id);
      conn->store_uint64(event_end_pos);
      conn->store_string(info);
      IoResult send_ret = conn->send_row();
      if (send_ret != IoResult::SUCCESS) {
        FsUtil::fclose_binary(fp);
        return send_ret;
      }
    }
    index++;
    pos = header.get_next_position();
  }

  FsUtil::fclose_binary(fp);
  if (count < limit && error) {
    OMS_STREAM_ERROR << "Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.";
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND,
        "Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.",
        "HY000");
  }
  return conn->send_eof_packet();
}

IoResult ShowMasterStatusProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  constexpr int max_column_len = 255;

  ColumnPacket file_column_packet{
      "File", "", UTF8_CS, max_column_len, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket position_column_packet{"Position",
      "",
      UTF8_CS,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket binlog_do_db_column_packet{
      "Binlog_Do_DB", "", UTF8_CS, max_column_len, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket binlog_ignore_db_column_packet{"Binlog_Ignore_DB",
      "",
      UTF8_CS,
      max_column_len,
      ColumnType::ct_var_string,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket executed_gtid_set_column_packet{"Executed_Gtid_Set",
      "",
      UTF8_CS,
      max_column_len,
      ColumnType::ct_var_string,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};

  if (conn->send_result_metadata({file_column_packet,
          position_column_packet,
          binlog_do_db_column_packet,
          binlog_ignore_db_column_packet,
          executed_gtid_set_column_packet}) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  BinlogIndexRecord index_record;
  g_index_manager->get_memory_index(index_record);

  if (!index_record.get_file_name().empty()) {
    std::string file = CommonUtils::fill_binlog_file_name(index_record.get_index());
    uint64_t position = index_record.get_position();
    std::string binlog_do_db;
    std::string binlog_ignore_db;
    std::string executed_gtid_set;
    if (s_config.binlog_gtid_display.val()) {
      g_sys_var->get_global_var("gtid_executed", executed_gtid_set);
      OMS_INFO("{}: [show master status] executed_gtid_set: {}", conn->trace_id(), executed_gtid_set);
    }
    conn->start_row();
    conn->store_string(file);
    conn->store_uint64(position);
    conn->store_string(binlog_do_db);
    conn->store_string(binlog_ignore_db);
    conn->store_string(executed_gtid_set);
    IoResult send_ret = conn->send_row();
    if (send_ret != IoResult::SUCCESS) {
      return send_ret;
    }
  }
  return conn->send_eof_packet();
}

IoResult ShowSlaveStatusProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  OMS_INFO("{}: [show slave status] start execute", conn->trace_id());
  ColumnPacket slave_io_state_column_packet{
    "Slave_IO_State", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket master_host_column_packet{
    "Master_Host", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_user_column_packet{
      "Master_User", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_port_column_packet{"Master_Port",
      "",
      UTF8_CS,
      4,
      ColumnType::ct_long,
      ColumnDefinitionFlags::num_flag,
      0};
  ColumnPacket connect_retry_column_packet{"Connect_Retry",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket master_log_file_column_packet{
    "Master_Log_File", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket read_master_log_pos_column_packet{"Read_Master_Log_Pos",
    "",
    UTF8_CS,
    8,
    ColumnType::ct_longlong,
    ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
    0};
  ColumnPacket relay_log_file_column_packet{
    "Relay_Log_File", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket relay_log_pos_column_packet{"Relay_Log_Pos",
    "",
    UTF8_CS,
    8,
    ColumnType::ct_longlong,
    ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
    0};
  ColumnPacket relay_master_log_file_column_packet{
    "Relay_Master_Log_File", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket slave_io_running_column_packet{
    "Slave_IO_Running", "", UTF8_CS, 64, ColumnType::ct_enum, ColumnDefinitionFlags::enum_flag, 31};
  ColumnPacket slave_sql_running_column_packet{
    "Slave_SQL_Running", "", UTF8_CS, 64, ColumnType::ct_enum, ColumnDefinitionFlags::enum_flag, 31};
  ColumnPacket replicate_do_db_column_packet{
    "Replicate_Do_DB", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket replicate_ignore_db_column_packet{
    "Replicate_Ignore_DB", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket replicate_do_table_column_packet{
    "Replicate_Do_Table", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket replicate_ignore_table_column_packet{
    "Replicate_Ignore_Table", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket replicate_wild_do_table_column_packet{
    "Replicate_Wild_Do_Table", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket replicate_wild_ignore_table_column_packet{
    "Replicate_Wild_Ignore_Table", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket last_errno_column_packet{"Last_Errno",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket last_error_column_packet{
      "Last_Error", "", UTF8_CS, 1024, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket skip_counter_column_packet{"Skip_Counter",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};

  ColumnPacket exec_master_log_pos_column_packet{"Exec_Master_Log_Pos",
    "",
    UTF8_CS,
    8,
    ColumnType::ct_longlong,
    ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
    0};

  ColumnPacket relay_log_space_column_packet{"Relay_Log_Space",
    "",
    UTF8_CS,
    8,
    ColumnType::ct_longlong,
    ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
    0};

  ColumnPacket until_condition_column_packet{
      "Until_Condition", "", UTF8_CS, 10, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket until_log_file_column_packet{
      "Until_Log_File", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket until_log_pos_column_packet{"Until_Log_Pos",
    "",
    UTF8_CS,
    8,
    ColumnType::ct_longlong,
    ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
    0};
  ColumnPacket master_ssl_allowed_column_packet{
    "Master_SSL_Allowed", "", UTF8_CS, 3, ColumnType::ct_enum, ColumnDefinitionFlags::enum_flag, 31};
  ColumnPacket master_ssl_ca_file_column_packet{
    "Master_SSL_CA_File", "", UTF8_CS, 512, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_ssl_ca_path_column_packet{
    "Master_SSL_CA_Path", "", UTF8_CS, 512, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_ssl_cert_column_packet{
    "Master_SSL_Cert", "", UTF8_CS, 512, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_ssl_cipher_column_packet{
    "Master_SSL_Cipher", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_ssl_key_column_packet{
    "Master_SSL_Key", "", UTF8_CS, 512, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket seconds_behind_master_column_packet{"Seconds_Behind_Master",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket master_ssl_verify_server_cert_column_packet{
    "Master_SSL_Verify_Server_Cert", "", UTF8_CS, 3, ColumnType::ct_enum, ColumnDefinitionFlags::enum_flag, 31};
  ColumnPacket last_io_errno_column_packet{"Last_IO_Errno",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket last_io_error_column_packet{
    "Last_IO_Error", "", UTF8_CS, 1024, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket last_sql_errno_column_packet{"Last_SQL_Errno",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket last_sql_error_column_packet{
    "Last_SQL_Error", "", UTF8_CS, 1024, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket replicate_ignore_server_ids_column_packet{
    "Replicate_Ignore_Server_Ids", "", UTF8_CS, 1024, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_server_id_column_packet{"Master_Server_Id",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket master_uuid_column_packet{
    "Master_UUID", "", UTF8_CS, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_info_file_column_packet{
    "Master_Info_File", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket sql_delay_column_packet{"SQL_Delay",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket sql_remaining_delay_column_packet{"SQL_Remaining_Delay",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket slave_sql_running_state_column_packet{
    "Slave_SQL_Running_State", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_retry_count_column_packet{"Master_Retry_Count",
    "",
    UTF8_CS,
    4,
    ColumnType::ct_long,
    ColumnDefinitionFlags::num_flag,
    0};
  ColumnPacket master_bind_column_packet{
    "Master_Bind", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket last_io_error_timestamp_column_packet{
    "Last_IO_Error_Timestamp", "", UTF8_CS, 0, ColumnType::ct_timestamp, ColumnDefinitionFlags::timestamp_flag, 0};
  ColumnPacket last_sql_error_timestamp_column_packet{
    "Last_SQL_Error_Timestamp", "", UTF8_CS, 0, ColumnType::ct_timestamp, ColumnDefinitionFlags::timestamp_flag, 0};
  ColumnPacket master_ssl_crl_column_packet{
    "Master_SSL_Crl", "", UTF8_CS, 512, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_ssl_crlpath_column_packet{
    "Master_SSL_Crlpath", "", UTF8_CS, 512, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket retrieved_gtid_set_column_packet{"Retrieved_Gtid_Set",
      "",
      UTF8_CS,
      2048,
      ColumnType::ct_var_string,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket executed_gtid_set_column_packet{"Executed_Gtid_Set",
      "",
      UTF8_CS,
      2048,
      ColumnType::ct_var_string,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket auto_position_column_packet{
    "Auto_Position", "", UTF8_CS, 1, ColumnType::ct_enum, ColumnDefinitionFlags::enum_flag, 31};
  ColumnPacket replicate_rewrite_db_column_packet{
    "Replicate_Rewrite_DB", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket channel_name_column_packet{
    "Channel_name", "", UTF8_CS, 255, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket master_tls_version_column_packet{
    "Master_TLS_Version", "", UTF8_CS, 64, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};

  if (conn->send_result_metadata({
          slave_io_state_column_packet,
          master_host_column_packet,
          master_user_column_packet,
          master_port_column_packet,
          connect_retry_column_packet,
          master_log_file_column_packet,
          read_master_log_pos_column_packet,
          relay_log_file_column_packet,
          relay_log_pos_column_packet,
          relay_master_log_file_column_packet,
          slave_io_running_column_packet,
          slave_sql_running_column_packet,
          replicate_do_db_column_packet,
          replicate_ignore_db_column_packet,
          replicate_do_table_column_packet,
          replicate_ignore_table_column_packet,
          replicate_wild_do_table_column_packet,
          replicate_wild_ignore_table_column_packet,
          last_errno_column_packet,
          last_error_column_packet,
          skip_counter_column_packet,
          exec_master_log_pos_column_packet,
          relay_log_space_column_packet,
          until_condition_column_packet,
          until_log_file_column_packet,
          until_log_pos_column_packet,
          master_ssl_allowed_column_packet,
          master_ssl_ca_file_column_packet,
          master_ssl_ca_path_column_packet,
          master_ssl_cert_column_packet,
          master_ssl_cipher_column_packet,
          master_ssl_key_column_packet,
          seconds_behind_master_column_packet,
          master_ssl_verify_server_cert_column_packet,
          last_io_errno_column_packet,
          last_io_error_column_packet,
          last_sql_errno_column_packet,
          last_sql_error_column_packet,
          replicate_ignore_server_ids_column_packet,
          master_server_id_column_packet,
          master_uuid_column_packet,
          master_info_file_column_packet,
          sql_delay_column_packet,
          sql_remaining_delay_column_packet,
          slave_sql_running_state_column_packet,
          master_retry_count_column_packet,
          master_bind_column_packet,
          last_io_error_timestamp_column_packet,
          last_sql_error_timestamp_column_packet,
          master_ssl_crl_column_packet,
          master_ssl_crlpath_column_packet,
          retrieved_gtid_set_column_packet,
          executed_gtid_set_column_packet,
          auto_position_column_packet,
          replicate_rewrite_db_column_packet,
          channel_name_column_packet,
          master_tls_version_column_packet}) != IoResult::SUCCESS) {
    OMS_ERROR("{}: [show slave status]: send metadata failed ", conn->trace_id());
    return IoResult::FAIL;
  }

  OMS_INFO("{}: [show slave status] executed", conn->trace_id());
  return conn->send_eof_packet();
}

IoResult PurgeBinaryLogsProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::PurgeBinlogStatement*)statement;
  std::string binlog_file;
  std::string purge_ts;
  std::string cluster;
  std::string tenant;
  std::string instance_name;
  if (p_statement->binlog_file != nullptr) {
    binlog_file = p_statement->binlog_file->get_value();
  }

  if (p_statement->purge_ts != nullptr) {
    purge_ts = p_statement->purge_ts->get_value();
  }

  if (p_statement->tenant != nullptr) {
    cluster = p_statement->tenant->cluster;
    tenant = p_statement->tenant->tenant;
    conn->set_ob_cluster(cluster);
    conn->set_ob_tenant(tenant);
  }
  if (nullptr != p_statement->instance_name) {
    instance_name = p_statement->instance_name;
  }
  OMS_INFO("{}: [purge binary logs] cluster: {}, tenant: {}, instance: {}, binlog_file: {}, purge_ts: {}",
      conn->trace_id(),
      cluster,
      tenant,
      instance_name,
      binlog_file,
      purge_ts);

  if (!cluster.empty() && !tenant.empty()) {
    OMS_WARN("{}: [purge binary logs] Not support to forward [purge binlog] for tenant: {}.{}",
        conn->trace_id(),
        cluster,
        tenant);
    return conn->send_ok_packet();
  }

  // request to OBM
  if (conn->get_conn_type() == Connection::ConnectionType::OBM) {
    if (instance_name.empty()) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "Binlog instance is not specified", "HY000");
    }

    BinlogEntry instance;
    g_cluster->query_instance_by_name(instance_name, instance);
    if (instance.instance_name().empty() || instance.state() != InstanceState::RUNNING ||
        instance.state() == InstanceState::GRAYSCALE) {
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "Binlog instance does not exist or is in non-running status", "HY000");
    }

    InstanceClient client;
    if (OMS_OK != client.init(instance)) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "Failed to connect to binlog instance", "HY000");
    }

    std::string purge_sql = build_purge_sql(binlog_file, purge_ts);
    OMS_INFO("{}: [purge binary logs] Forward sql to binlog instance: {}, server_addr: {}, sql: {}",
        conn->trace_id(),
        instance.instance_name(),
        client.get_server_addr(),
        purge_sql);

    MySQLResultSet result_set;
    if (OMS_OK != client.query(purge_sql, result_set)) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, result_set.message, "HY000");
    }
  } else {
    // request to OBI
    std::vector<std::string> purge_binlog_files;
    std::string error_msg;
    std::string base_path = string(".") + BINLOG_DATA_DIR;
    int ret = g_index_manager->purge_binlog_index(base_path, binlog_file, purge_ts, error_msg, purge_binlog_files);
    if (ret != OMS_OK) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, error_msg, "HY000");
    }
    g_executor->submit(purge_binlog_file, purge_binlog_files);
  }

  return conn->send_ok_packet();
}

std::string PurgeBinaryLogsProcessor::build_purge_sql(const string& binlog_file, const string& purge_ts)
{
  std::string purge_sql = "purge binary logs ";
  if (!binlog_file.empty()) {
    purge_sql.append("to").append(" '").append(binlog_file).append("'");
  } else {
    purge_sql.append("before").append(" '").append(purge_ts).append("'");
  }
  return purge_sql;
}

int PurgeBinaryLogsProcessor::purge_binlog_file(const vector<std::string>& file_paths)
{
  for (const std::string& purge_file : file_paths) {
    std::error_code error_code;
    if (std::filesystem::remove(purge_file, error_code) && !error_code) {
      OMS_INFO("Deleted binlog file: {}.", purge_file);
    } else {
      OMS_ERROR("Failed to purge binlog file: [{}]", purge_file);
    }
  }
  return OMS_OK;
}

IoResult ShowBinlogServerProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  uint16_t utf8_cs = 33;
  uint16_t binary_cs = 63;

  ColumnPacket cluster_column_packet{
      "cluster", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket tenant_column_packet{
      "tenant", "", utf8_cs, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket ip_column_packet{
      "ip", "", utf8_cs, 52, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket port_column_packet{"port",
      "",
      binary_cs,
      4,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket status_column_packet{
      "status", "", utf8_cs, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket cause_column_packet{
      "cause", "", utf8_cs, 0, ColumnType::ct_null, static_cast<ColumnDefinitionFlags>(0), 0};

  if (conn->send_result_metadata({cluster_column_packet,
          tenant_column_packet,
          ip_column_packet,
          port_column_packet,
          status_column_packet,
          cause_column_packet}) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowBinlogServerStatement*)statement;
  BinlogEntry condition;
  if (p_statement->tenant != nullptr) {
    condition.set_cluster(p_statement->tenant->cluster);
    condition.set_tenant(p_statement->tenant->tenant);
  } else {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "tenant is not specified", "HY000");
  }

  std::string master_instance_name;
  if (OMS_OK != g_cluster->query_master_instance(condition.cluster(), condition.tenant(), master_instance_name)) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "tenant has no binlog instances yet", "HY000");
  }
  if (master_instance_name.empty()) {
    OMS_ERROR(
        "{}: Tenant [{}.{}] has no master service instance", conn->trace_id(), condition.cluster(), condition.tenant());
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "tenant has no master service instance", "HY000");
  }

  BinlogEntry master_instance;
  g_cluster->query_instance_by_name(master_instance_name, master_instance);
  if (master_instance.instance_name().empty() || master_instance.state() != InstanceState::RUNNING) {
    OMS_ERROR("{}: Master instance [{}] of tenant [{}.{}] is unavailable",
        conn->trace_id(),
        master_instance_name,
        condition.cluster(),
        condition.tenant());
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "master instance of tenant is unavailable", "HY000");
  }

  // row packet
  conn->start_row();
  conn->store_string(master_instance.cluster());
  conn->store_string(master_instance.tenant());
  conn->store_string(master_instance.ip());
  conn->store_uint64(master_instance.port());
  conn->store_string("OK");
  conn->store_null();
  IoResult send_ret = conn->send_row();
  if (send_ret != IoResult::SUCCESS) {
    return send_ret;
  }
  return conn->send_eof_packet();
}

IoResult BaseCreateProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  std::string cluster;
  std::string tenant;
  std::string instance;
  std::map<std::string, std::string> binlog_options;
  /* range: [1, max_instance_replicate_num] */
  uint16_t replicate_num = 1;

  // 1. obtain and validate create binlog options
  obtain_options(statement, binlog_options, cluster, tenant, instance, replicate_num);
  std::string options_str;
  for (const auto& option : binlog_options) {
    options_str.append(option.first).append("=").append(option.second).append(" ");
  }
  OMS_INFO("{}: [create binlog] Parsed options: cluster: {}, tenant: {}, replicate num: {}, options: {}",
      conn->trace_id(),
      cluster,
      tenant,
      replicate_num,
      options_str);

  if (cluster.empty() || tenant.empty()) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Cluster or tenant cannot be empty", "HY000");
  }
  if (replicate_num > s_config.max_instance_replicate_num.val()) {
    OMS_ERROR("{}: Option [REPLICATE NUM] exceeds max_instance_replicate_num: {} > {}",
        replicate_num,
        s_config.max_instance_replicate_num.val());
    return conn->send_err_packet(BINLOG_FATAL_ERROR,
        "The option [REPLICATE NUM] cannot be greater than " +
            std::to_string(s_config.max_instance_replicate_num.val()),
        "HY000");
  }
  if (binlog_options.find("cluster_url") == binlog_options.end()) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Cluster url cannot be empty", "HY000");
  }

  // 2. check if creation can continue
  std::vector<BinlogEntry> instance_vec;
  if (OMS_OK != g_cluster->query_tenant_surviving_instances(cluster, tenant, instance_vec)) {
    OMS_ERROR("Failed to obtain instances for tenant [{}.{}]", cluster, tenant);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error", "HY000");
  }

  DataPacketHolder packet_holder;
  if (!can_create(cluster, tenant, instance, instance_vec, packet_holder)) {
    return packet_holder.send(conn);
  }

  // 3. init InstanceMeta
  std::map<std::string, std::string> instance_configs;
  if (OMS_OK !=
      g_cluster->query_instance_configs(instance_configs, binlog_options["group"], cluster, tenant, instance)) {
    OMS_ERROR("{}: Failed to query binlog configs from metadb.", conn->trace_id());
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error", "HY000");
  }

  OMS_INFO("init instance meta");
  auto* meta = new InstanceMeta(s_config, instance, cluster, tenant);
  defer(delete meta);
  meta->init_meta(instance_configs, binlog_options);
  if ((meta->binlog_config()->initial_ob_txn_gtid_seq() > 0 || !meta->binlog_config()->initial_ob_txn_id().empty()) &&
      (0 == meta->cdc_config()->start_timestamp())) {
    OMS_ERROR("{}: When specifying gtid mapping, the start timestamp cannot be empty", conn->trace_id());
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "When specifying gtid mapping, the start timestamp cannot be empty", "HY000");
  }
  if (0 == meta->cdc_config()->start_timestamp()) {
    meta->cdc_config()->set_start_timestamp(Timer::now_s());
  }

  if (meta->config_primary_secondary_options() != OMS_OK) {
    OMS_ERROR("{}: Binlog service configuration item initialization failed", conn->trace_id());
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "Binlog service configuration item initialization failed", "HY000");
  }
  OMS_INFO("{}: [create binlog] meta info: {}", conn->trace_id(), meta->serialize_to_json());

  // 4. create binlog
  pre_action(cluster, tenant);

  std::vector<std::string> err_msgs;
  for (int i = 0; i < replicate_num; i++) {
    auto* instance_meta = dynamic_cast<InstanceMeta*>(meta->clone());
    if (instance_meta->instance_name().empty()) {
      BinlogInstanceHelper::config_instance_name(instance_vec, instance_meta);
    }

    Node node;
    std::string err_msg;
    if (OMS_OK != SelectionStrategy::load_balancing_node(*instance_meta, node, err_msg)) {
      OMS_ERROR("{}: Failed to load balancing node for instance: {}, error: {}",
          conn->trace_id(),
          instance_meta->instance_name(),
          err_msg);
      err_msgs.emplace_back(err_msg);
      continue;
    }
    OMS_INFO("{}: Binlog instance [{}] is loaded onto node [{}] for creation",
        conn->trace_id(),
        instance_meta->instance_name(),
        node.identifier());
    BinlogInstanceHelper::check_or_config_instance_server_options(instance_vec, node.ip(), instance_meta);
    OMS_INFO("{}: Begin to create binlog instance [{}] with meta: {}",
        conn->trace_id(),
        instance_meta->instance_name(),
        instance_meta->serialize_to_json());

    BinlogEntry entry(true);
    entry.init_instance(node, instance_meta);
    Task create_task(true);
    create_task.init_create_task(node, *instance_meta);

    OMS_INFO("{}: Begin to publish task to create binlog instance [{}] asynchronously with config: {}",
        conn->trace_id(),
        instance_meta->instance_name(),
        instance_meta->serialize_to_json());
    if (OMS_OK != g_cluster->add_instance(entry, create_task)) {
      OMS_ERROR("{}: Failed to publish create instance task", conn->trace_id());
      err_msgs.emplace_back("publishing error");
    }
    instance_vec.emplace_back(entry);
  }

  // 5. return exec result
  if (err_msgs.empty()) {
    return conn->send_ok_packet();
  } else {
    std::string err_msg;
    if (replicate_num == 1) {
      err_msg = err_msgs.front();
    } else {
      err_msg = "Expected instances: " + std::to_string(replicate_num) +
                ", failed instances: " + std::to_string(err_msgs.size()) + ", details: \n";
      for (int i = 0; i < err_msgs.size(); i++) {
        err_msg.append("Failed instance ")
            .append(std::to_string(i))
            .append(":")
            .append(err_msgs.at(i))
            .append(i != (err_msgs.size() - 1) ? "\n" : "");
      }
    }

    return err_msgs.size() == replicate_num ? conn->send_err_packet(BINLOG_FATAL_ERROR, err_msg, "HY000")
                                            : conn->send_ok_packet(1, err_msg);
  }
}

void CreateBinlogProcessor::obtain_options(const hsql::SQLStatement* statement,
    std::map<std::string, std::string>& binlog_options, std::string& cluster, std::string& tenant,
    std::string& instance, uint16_t& replicate_num)
{
  auto* p_statement = (hsql::CreateBinlogStatement*)statement;
  if (p_statement->tenant != nullptr) {
    cluster = p_statement->tenant->cluster;
    tenant = p_statement->tenant->tenant;
  }

  if (p_statement->ts != nullptr && !p_statement->ts->get_value().empty()) {
    std::string start_timestamp_s = p_statement->ts->get_value();
    if (start_timestamp_s.length() == 16) {
      start_timestamp_s = std::to_string(std::atoll(start_timestamp_s.c_str()) / 1000000);
    }
    binlog_options.emplace("start_timestamp", start_timestamp_s);
  }

  if (p_statement->user_info != nullptr) {
    binlog_options.emplace("cluster_user", p_statement->user_info->user);
    binlog_options.emplace("cluster_password", p_statement->user_info->password);
  }

  if (nullptr != p_statement->binlog_options) {
    for (const hsql::SetClause* binlog_option : *(p_statement->binlog_options)) {
      binlog_options[std::string(binlog_option->column)] = binlog_option->value->get_value();
    }
  }

  auto binlog_options_iter = binlog_options.find("replicate_num");
  replicate_num = (binlog_options_iter == binlog_options.end()) ? s_config.default_instance_replicate_num.val()
                                                                : std::atoi(binlog_options_iter->second.c_str());
}

bool CreateBinlogProcessor::can_create(const std::string& cluster, const std::string& tenant,
    const std::string& instance_name, const std::vector<BinlogEntry>& instance_vec, DataPacketHolder& packet_holder)
{
  if (!instance_vec.empty()) {
    OMS_ERROR("The tenant [{}.{}] has already instances in non-drop status, so [create binlog] is no longer executed",
        cluster,
        tenant);
    packet_holder.ok_packet(1, "The tenant already has one or more binlog instances.");
    return false;
  }
  PrometheusExposer::mark_binlog_counter_metric("", "", cluster, tenant, {}, {}, BINLOG_CREATE_TYPE);
  return true;
}

void CreateBinlogProcessor::pre_action(const string& cluster, const string& tenant)
{
  // !! avoid dirty data causing gtid inconsistency
  if (OMS_OK != g_cluster->remove_tenant_gtid_seq(cluster, tenant)) {
    OMS_ERROR("Failed to remove gtid seq of tenant [{}.{}]; there may exist problems if gtid has dirty data",
        cluster,
        tenant);
  }
}

void CreateBinlogInstanceProcessor::obtain_options(const hsql::SQLStatement* statement,
    std::map<std::string, std::string>& binlog_options, string& cluster, string& tenant, std::string& instance,
    uint16_t& replicate_num)
{
  auto* p_statement = (hsql::CreateBinlogInstanceStatement*)statement;

  if (nullptr != p_statement->instance_name) {
    instance = std::string(p_statement->instance_name);
  }
  if (nullptr != p_statement->tenant) {
    cluster = std::string(p_statement->tenant->cluster);
    tenant = std::string(p_statement->tenant->tenant);
  }
  if (nullptr != p_statement->instance_options) {
    for (const hsql::SetClause* option : *(p_statement->instance_options)) {
      std::string key = std::string(option->column);
      transform(key.begin(), key.end(), key.begin(), ::tolower);
      binlog_options[key] = option->value->get_value();
    }
  }
}

bool CreateBinlogInstanceProcessor::can_create(const std::string& cluster, const std::string& tenant,
    const std::string& instance, const std::vector<BinlogEntry>& instance_vec, DataPacketHolder& packet_holder)
{
  if (instance_vec.size() >= s_config.max_instance_replicate_num.val()) {
    packet_holder.err_packet(BINLOG_FATAL_ERROR, "The maximum number of instances has been reached", "HY000");
    return false;
  }
  if (instance.empty()) {
    packet_holder.err_packet(BINLOG_FATAL_ERROR, "Binlog instance cannot be empty", "HY000");
    return false;
  }

  BinlogEntry existing_instance;
  if (OMS_OK != g_cluster->query_instance_by_name(instance, existing_instance)) {
    OMS_ERROR("Failed to query instance: {}", instance);
    packet_holder.err_packet(BINLOG_FATAL_ERROR, "Internal error", "HY000");
    return false;
  }
  if (!existing_instance.instance_name().empty()) {
    OMS_INFO("Matched existing binlog instances [{}] for tenant [{}.{}], status: {}",
        instance,
        cluster,
        tenant,
        instance_state_str(existing_instance.state()));
    OMS_ERROR("Binlog instance with name [{}}] already exists", instance);
    packet_holder.err_packet(BINLOG_FATAL_ERROR, "Binlog instance [" + instance + "] already exists", "HY000");
    return false;
  }
  return true;
}

IoResult AlterBinlogProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::AlterBinlogStatement*)statement;
  std::string cluster = p_statement->tenant->cluster;
  std::string tenant = p_statement->tenant->tenant;
  std::vector<hsql::SetClause*> instance_options = *(p_statement->instance_options);
  OMS_INFO("{}: [alter binlog] tenant [{}.{}], alter options: {}",
      conn->trace_id(),
      cluster,
      tenant,
      instance_options.size());

  BinlogEntry condition;
  condition.set_cluster(cluster);
  condition.set_tenant(tenant);
  std::vector<BinlogEntry*> instance_vec;
  defer(release_vector(instance_vec));
  g_cluster->query_instances(instance_vec, condition);
  for (auto iter = instance_vec.begin(); iter != instance_vec.end();) {
    if ((*iter)->is_dropped()) {
      delete (*iter);
      iter = instance_vec.erase(iter);
    } else {
      iter++;
    }
  }
  if (instance_vec.empty()) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Tenant does not exist", "HY000");
  }

  // 1. alter instances options
  std::string alter_res;
  for (const auto& instance : instance_vec) {
    if (AlterBinlogInstanceProcessor::update_instance_options(*instance, instance_options, alter_res)) {
      alter_res += "\n";
    }
  }

  // 2. replace global config of tenant
  for (auto option : instance_options) {
    if (OMS_OK != g_cluster->replace_config(
                      option->column, option->value->get_value(), Granularity::G_TENANT, condition.full_tenant())) {
      OMS_ERROR("{}: [alter binlog] Failed to replace option for tenant [{}]: {} = {}",
          conn->trace_id(),
          tenant,
          option->column,
          option->value->get_value());
    }
  }

  if (!alter_res.empty()) {
    OMS_INFO("{}: [alter binlog] tenant [{}]: {}", conn->trace_id(), instance_vec.front()->full_tenant(), alter_res);
    return conn->send_ok_packet(1, alter_res);
  }
  return conn->send_ok_packet();
}

IoResult AlterBinlogInstanceProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::AlterBinlogInstanceStatement*)statement;

  std::string instance_name(p_statement->instance_name);
  BinlogEntry instance;
  g_cluster->query_instance_by_name(instance_name, instance);
  if (instance.instance_name().empty()) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Binlog instance does not exist", "HY000");
  }

  std::string alter_res;
  update_instance_options(instance, *(p_statement->instance_options), alter_res);
  if (!alter_res.empty()) {
    OMS_INFO("{}: [alter binlog instance] {}", conn->trace_id(), alter_res);
    return conn->send_ok_packet(1, alter_res);
  }
  return conn->send_ok_packet();
}

bool AlterBinlogInstanceProcessor::update_instance_options(
    BinlogEntry& instance, std::vector<hsql::SetClause*>& instance_options, std::string& alter_res)
{
  // 1. forward to binlog instance: SET GLOBAL key=value
  bool sync_alter_instance = true;
  InstanceClient client;
  if (instance.state() == InstanceState::RUNNING || instance.state() == InstanceState::GRAYSCALE) {
    if (OMS_OK != client.init(instance)) {
      OMS_ERROR("{}: [alter binlog instance] Failed to connect to binlog instance: {}", instance.instance_name());
      sync_alter_instance = false;
    }
  } else {
    sync_alter_instance = false;
  }

  bool alter_with_warning = false;
  if (sync_alter_instance) {
    uint16_t success_size = 0, failed_size = 0;
    for (auto& variable : instance_options) {
      if (strcasecmp(variable->column, "server_id") == 0 || strcasecmp(variable->column, "server_uuid") == 0) {
        OMS_INFO("[alter binlog instances] Skipped variables that need to be restarted to take effect: {}",
            variable->column);
        continue;
      }

      std::string set_sql = "SET GLOBAL " + std::string(variable->column) + "='" + variable->value->get_value() + "'";
      OMS_INFO("[alter binlog instance] binlog instance [{}]: {}", instance.instance_name(), set_sql);
      MySQLResultSet result_set;
      if (OMS_OK == client.query(set_sql, result_set)) {
        success_size += 1;
        continue;
      } else {
        OMS_ERROR("[alter binlog instance] failed to set binlog instance [{}]: {}, error: {}",
            instance.instance_name(),
            set_sql,
            result_set.message);
        failed_size += 1;
      }
    }
    if (0 != failed_size) {
      alter_with_warning = true;
      alter_res += instance.instance_name() + ": alter total [" + std::to_string(instance_options.size()) +
                   "] options, success: " + std::to_string(success_size) + ", failed: " + std::to_string(failed_size);
    }
  } else {
    alter_with_warning = true;
    alter_res += instance.instance_name() + ": only alter mete due to unable to connect to binlog instance";
  }

  // 2. update OBI instance meta
  for (auto variable : instance_options) {
    bool is_matched = instance.config()->binlog_config()->set_plain(variable->column, variable->value->get_value());
    if (!is_matched) {
      instance.config()->cdc_config()->set_plain(variable->column, variable->value->get_value());
    }
  }
  g_cluster->update_instance(instance);
  return alter_with_warning;
}

IoResult StartBinlogInstanceProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  std::string instance_name;
  hsql::InstanceFlag flag;
  auto* p_statement = (hsql::StartBinlogInstanceStatement*)statement;
  instance_name = std::string(p_statement->instance_name);
  flag = p_statement->flag;
  if (hsql::InstanceFlag::OBCDC_ONLY == flag) {
    OMS_ERROR("{}: [start binlog instance] Unsupported flag: {}", conn->trace_id(), flag);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Flag [OBCDC_ONLY] is not supported yet", "HY000");
  }

  BinlogEntry entry;
  if (OMS_OK != g_cluster->query_instance_by_name(instance_name, entry)) {
    OMS_ERROR("{}: [start binlog instance] Failed to query meta for instance: {}", conn->trace_id(), instance_name);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error: failed to query meta for instance", "HY000");
  }
  if (entry.instance_name().empty()) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "binlog instance does not exist: " + instance_name, "HY000");
  }
  auto state = static_cast<InstanceState>(entry.state());
  if (InstanceState::FAILED != state && InstanceState::STOP != state && InstanceState::OFFLINE != state) {
    OMS_WARN("{}: [start binlog instance] Unable to start binlog instance [{}] with state: {}",
        conn->trace_id(),
        instance_name,
        instance_state_str(state));
    return conn->send_err_packet(BINLOG_FATAL_ERROR,
        "Not supported to start binlog instance with status: " + instance_state_str(state),
        "HY000");
  }
  if (entry.work_path().empty()) {
    OMS_ERROR("{}: [start binlog instance] Unable to start binlog instance [{}] without work path: {}",
        conn->trace_id(),
        instance_name);
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "Not supported to start binlog instance without the existing work path", "HY000");
  }
  Node node;
  if (OMS_OK != g_cluster->query_node_by_id(entry.node_id(), node)) {
    OMS_ERROR("{}: [start binlog instance] The node [{}] where instance [{}] is located is not queried.",
        conn->trace_id(),
        entry.node_id(),
        instance_name);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "The node where instance is located does not exist", "HY000");
  }
  if (State::OFFLINE == node.state()) {
    OMS_ERROR("{}: [start binlog instance] The node [{}] where instance [{}] is located is offline.",
        conn->trace_id(),
        entry.node_id(),
        instance_name);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "The node where instance is located is offline", "HY000");
  }

  Task instance_task;
  instance_task.set_status(TaskStatus::WAITING);
  instance_task.set_execution_node(entry.node_id());
  instance_task.set_type(TaskType::START_INSTANCE);
  instance_task.set_instance_name(instance_name);
  auto* task_param = new StartInstanceTaskParam();
  task_param->set_flag(flag);
  instance_task.set_task_param(task_param);

  OMS_INFO("{}: Generate a task to start binlog instance [{}] for tenant [{}.{}] with flag: {}, task: {}",
      conn->trace_id(),
      instance_name,
      entry.cluster(),
      entry.tenant(),
      flag,
      instance_task.serialize_to_json());
  g_cluster->publish_task(instance_task);
  return conn->send_ok_packet();
}

IoResult StopBinlogInstanceProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  std::string instance_name;
  hsql::InstanceFlag flag;
  auto* p_statement = (hsql::StopBinlogInstanceStatement*)statement;
  instance_name = std::string(p_statement->instance_name);
  flag = p_statement->flag;
  if (hsql::InstanceFlag::OBCDC_ONLY == flag) {
    OMS_ERROR("{}: [stop binlog instance] Unsupported flag: {}", conn->trace_id(), flag);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Flag [OBCDC_ONLY] is not supported yet", "HY000");
  }

  BinlogEntry entry;
  if (OMS_OK != g_cluster->query_instance_by_name(instance_name, entry)) {
    OMS_ERROR("{}: [stop binlog instance] Failed to query meta for instance: {}", conn->trace_id(), instance_name);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error: failed to query meta for instance", "HY000");
  }
  if (entry.instance_name().empty()) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "binlog instance does not exist: " + instance_name, "HY000");
  }
  auto state = static_cast<InstanceState>(entry.state());
  if (InstanceState::RUNNING != state && InstanceState::GRAYSCALE != state) {
    OMS_WARN("{}: [stop binlog instance] Unable to stop binlog instance [{}] with state: {}",
        conn->trace_id(),
        instance_name,
        instance_state_str(state));
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "Not supported to stop binlog instance with status: " + instance_state_str(state), "HY000");
  }
  Node node;
  if (OMS_OK != g_cluster->query_node_by_id(entry.node_id(), node)) {
    OMS_ERROR("{}: [stop binlog instance] The node [{}] where instance [{}] is located is not queried.",
        conn->trace_id(),
        entry.node_id(),
        instance_name);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "The node where instance is located does not exist", "HY000");
  }
  if (State::OFFLINE == node.state()) {
    OMS_ERROR("{}: [stop binlog instance] The node [{}] where instance [{}] is located is offline.",
        conn->trace_id(),
        entry.node_id(),
        instance_name);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "The node where instance is located is offline", "HY000");
  }

  Task instance_task;
  instance_task.set_status(TaskStatus::WAITING);
  instance_task.set_execution_node(entry.node_id());
  instance_task.set_type(TaskType::STOP_INSTANCE);
  instance_task.set_instance_name(instance_name);
  auto* task_param = new StopInstanceTaskParam();
  task_param->set_flag(flag);
  instance_task.set_task_param(task_param);

  OMS_INFO("{}: Generate a task to stop binlog instance [{}] for tenant [{}.{}] with flag: {}, task: {}",
      conn->trace_id(),
      instance_name,
      entry.cluster(),
      entry.tenant(),
      flag,
      instance_task.serialize_to_json());
  PrometheusExposer::mark_binlog_counter_metric(entry.node_id(),
      entry.instance_name(),
      entry.cluster(),
      entry.tenant(),
      entry.cluster_id(),
      entry.tenant_id(),
      BINLOG_INSTANCE_STOP_TYPE);
  g_cluster->publish_task(instance_task);
  return conn->send_ok_packet();
}

IoResult BaseDropProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  DataPacketHolder packet_holder;
  std::vector<BinlogEntry*> instances;
  defer(release_vector(instances));
  bool drop_tenant = true;
  bool force = true;
  int32_t defer_execution_sec = -1;

  if (!obtain_instances(statement, instances, drop_tenant, force, defer_execution_sec, packet_holder)) {
    return packet_holder.send(conn);
  }

  std::vector<Task> drop_instance_tasks;
  for (const auto instance : instances) {
    if (instance->is_dropped()) {
      OMS_INFO("{}: Binlog instance [{}] has already been dropped.", conn->trace_id(), instance->instance_name());
      continue;
    }
    if (!force && instance->is_running()) {
      OMS_ERROR("{}: Unsupported to drop instance [{}] under [{}]] status without [force] flag",
          conn->trace_id(),
          instance->instance_name(),
          instance_state_str(instance->state()));
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "Unsupported to drop instance under running status without [force] flag", "HY000");
    }
    generate_drop_task(*instance, drop_tenant, defer_execution_sec, drop_instance_tasks);
  }

  if (OMS_OK != g_cluster->drop_instances(drop_instance_tasks)) {
    OMS_ERROR("Failed to publish drop binlog instance tasks: {}", drop_instance_tasks.size());
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Unexpected errors while dropping binlog", "HY000");
  }
  postAction(instances);
  return conn->send_ok_packet();
}

void BaseDropProcessor::generate_drop_task(
    BinlogEntry& instance, bool drop_tenant, int32_t defer_execution_sec, std::vector<Task>& drop_instance_tasks)
{
  Task drop_task;
  drop_task.set_status(TaskStatus::WAITING);
  drop_task.set_execution_node(instance.node_id());
  drop_task.set_type(TaskType::DROP_INSTANCE);
  drop_task.set_instance_name(instance.instance_name());
  auto* task_param = new DropInstanceTaskParam();
  task_param->set_drop_all(drop_tenant);
  if (defer_execution_sec < 0) {
    defer_execution_sec = s_config.default_defer_drop_sec.val();
  }
  task_param->set_expect_exec_time(Timer::now_s() + defer_execution_sec);
  drop_task.set_task_param(task_param);

  drop_instance_tasks.emplace_back(drop_task);
  OMS_INFO("Generated a task to drop binlog instance [{}] with state [{}], task details: {}",
      instance.instance_name(),
      instance_state_str(instance.state()),
      drop_task.serialize_to_json());
}

bool DropBinlogProcessor::obtain_instances(const hsql::SQLStatement* statement, std::vector<BinlogEntry*>& instance_vec,
    bool& drop_tenant, bool& force, int32_t& defer_execution_sec, DataPacketHolder& packet_holder)
{
  drop_tenant = true;
  auto* p_statement = (hsql::DropBinlogStatement*)statement;
  bool if_exists = p_statement->if_exists;
  std::string cluster;
  std::string tenant;
  if (nullptr != p_statement->tenant_info) {
    cluster = p_statement->tenant_info->cluster;
    tenant = p_statement->tenant_info->tenant;
  } else {
    packet_holder.err_packet(BINLOG_FATAL_ERROR, "Cluster or tenant cannot be empty", "HY000");
    return false;
  }
  if (p_statement->defer_execution_sec != nullptr) {
    defer_execution_sec = std::atoi(p_statement->defer_execution_sec->get_value().c_str());
  }

  BinlogEntry condition;
  condition.set_cluster(cluster);
  condition.set_tenant(tenant);
  g_cluster->query_instances(instance_vec, condition);
  if (instance_vec.empty()) {
    if (if_exists) {
      packet_holder.ok_packet();
    } else {
      packet_holder.err_packet(BINLOG_FATAL_ERROR, "Not found any instances for tenant", "HY000");
    }
    OMS_ERROR(
        "[drop binlog] Not found any binlog instances for tenant [{}.{}]， if exists: {}", cluster, tenant, if_exists);
    return false;
  }

  PrometheusExposer::mark_binlog_counter_metric(
      "", "", cluster, tenant, instance_vec[0]->cluster_id(), instance_vec[0]->tenant_id(), BINLOG_RELEASE_TYPE);
  OMS_INFO(
      "Received request to drop binlog for tenant [{}.{}], instances num: {}", cluster, tenant, instance_vec.size());
  return true;
}

void DropBinlogProcessor::postAction(std::vector<BinlogEntry*>& instance_vec)
{
  OMS_INFO(
      "Try to clear configs for tenant [{}] after publishing drop_instance tasks", instance_vec.front()->full_tenant());
  if (OMS_OK != g_cluster->delete_config("", Granularity::G_TENANT, instance_vec.front()->full_tenant())) {
    OMS_ERROR("Failed to clear configs for tenant: {}", instance_vec.front()->full_tenant());
  }
}

bool DropBinlogInstanceProcessor::obtain_instances(const hsql::SQLStatement* statement,
    std::vector<BinlogEntry*>& instance_vec, bool& drop_tenant, bool& force, int32_t& defer_execution_sec,
    DataPacketHolder& packet_holder)
{
  drop_tenant = false;
  auto* p_statement = (hsql::DropBinlogInstanceStatement*)statement;
  std::string instance_name(p_statement->instance_name);
  force = p_statement->force;
  if (p_statement->defer_execution_sec != nullptr) {
    defer_execution_sec = std::atoi(p_statement->defer_execution_sec->get_value().c_str());
  }

  auto* instance = new BinlogEntry(true);
  g_cluster->query_instance_by_name(instance_name, *instance);
  if (instance->instance_name().empty()) {
    OMS_ERROR("Failed to query binlog instance: {}", instance_name);
    packet_holder.err_packet(BINLOG_FATAL_ERROR, "Binlog instance cannot be found", "HY000");
    return false;
  }
  OMS_INFO("Received request to drop binlog instance [{}]", instance_name);
  instance_vec.emplace_back(instance);
  return true;
}

IoResult ShowBinlogInstanceProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  Timer timer;
  ColumnPacket name_column_packet{
      "name", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket ob_cluster_column_packet{
      "ob_cluster", "", UTF8_CS, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket ob_tenant_column_packet{
      "ob_tenant", "", UTF8_CS, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket ip_column_packet{
      "ip", "", UTF8_CS, 32, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket port_column_packet{
      "port", "", BINARY_CS, 16, ColumnType::ct_short, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket zone_column_packet{
      "zone", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket region_column_packet{
      "region", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket group_column_packet{
      "group", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket running_column_packet{
      "running", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket state_column_packet{
      "state", "", UTF8_CS, 128, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket obcdc_running_column_packet{
      "obcdc_running", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket obcdc_state_column_packet{
      "obcdc_state", "", UTF8_CS, 128, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket service_mode_column_packet{
      "service_mode", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket convert_running_column_packet{
      "convert_running", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket convert_delay_column_packet{
      "convert_delay", "", BINARY_CS, 20, ColumnType::ct_longlong, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket convert_rps_column_packet{
      "convert_rps", "", BINARY_CS, 20, ColumnType::ct_longlong, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket convert_eps_column_packet{
      "convert_eps", "", BINARY_CS, 20, ColumnType::ct_longlong, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket convert_iops_column_packet{
      "convert_iops", "", BINARY_CS, 20, ColumnType::ct_longlong, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket dumpers_column_packet{
      "dumpers", "", BINARY_CS, 20, ColumnType::ct_longlong, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket version_column_packet{
      "version", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket odp_addr_column_packet{
      "odp_addr", "", UTF8_CS, 128, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};

  if (conn->send_result_metadata({name_column_packet,
          ob_cluster_column_packet,
          ob_tenant_column_packet,
          ip_column_packet,
          port_column_packet,
          zone_column_packet,
          region_column_packet,
          group_column_packet,
          running_column_packet,
          state_column_packet,
          obcdc_running_column_packet,
          obcdc_state_column_packet,
          service_mode_column_packet,
          convert_running_column_packet,
          convert_delay_column_packet,
          convert_rps_column_packet,
          convert_eps_column_packet,
          convert_iops_column_packet,
          dumpers_column_packet,
          version_column_packet,
          odp_addr_column_packet}) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  std::vector<BinlogEntry*> instance_entries;
  defer(release_vector(instance_entries));
  auto* p_statement = (hsql::ShowBinlogInstanceStatement*)statement;
  hsql::ShowInstanceMode mode = p_statement->mode;
  bool history = p_statement->history;
  uint32_t offset = 0;
  uint32_t limit = UINT32_MAX;
  if (nullptr != p_statement->limit) {
    if (nullptr != p_statement->limit->offset) {
      offset = std::atol(p_statement->limit->offset->get_value().c_str());
    }
    if (nullptr != p_statement->limit->limit) {
      limit = std::atol(p_statement->limit->limit->get_value().c_str());
    }
  }
  OMS_INFO(
      "[show binlog instance] options, mode: {}, history: {}, offset: {}, limit: {}", mode, history, offset, limit);

  switch (mode) {
    case hsql::ShowInstanceMode::INSTANCE: {
      if (nullptr == p_statement->instance_names) {
        OMS_INFO("{}: [show binlog instance] show instances for all instances", conn->trace_id());

        if (OMS_OK != g_cluster->query_all_instances(instance_entries)) {
          OMS_ERROR("{}: [show binlog instance] Failed to fetch all binlog instances", conn->trace_id());
        }
      } else {
        std::set<std::string> instance_names;
        std::string instance_names_str;
        for (auto instance_name : *(p_statement->instance_names)) {
          instance_names.insert(instance_name);
          instance_names_str.append(instance_name).append(",");
        }
        OMS_INFO("{}: [show binlog instance] show instances for instances: {}", conn->trace_id(), instance_names_str);
        timer.reset();
        if (OMS_OK != g_cluster->query_instances(instance_entries, instance_names)) {
          OMS_ERROR(
              "{}: [show binlog instance] Failed to fetch binlog instances: {}", conn->trace_id(), instance_names_str);
        }
        OMS_INFO("{}: [show binlog instance] fetch {} instances,time consuming {}",
            conn->trace_id(),
            instance_entries.size(),
            timer.elapsed());
      }
      break;
    }
    case hsql::ShowInstanceMode::TENANT: {
      std::string cluster(p_statement->tenant->cluster);
      std::string tenant(p_statement->tenant->tenant);
      OMS_INFO("{}: [show binlog instance] show instances for tenant [{}.{}]", conn->trace_id(), cluster, tenant);

      BinlogEntry condition;
      condition.set_cluster(cluster);
      condition.set_tenant(tenant);
      timer.reset();
      if (OMS_OK != g_cluster->query_instances(instance_entries, condition)) {
        OMS_ERROR("{}: [show binlog instance] Failed to fetch binlog instances for tenant: [{}.{}]",
            conn->trace_id(),
            cluster,
            tenant);
      }
      OMS_INFO("{}: [show binlog instance] fetch {} instances,time consuming {}",
          conn->trace_id(),
          instance_entries.size(),
          timer.elapsed());
      break;
    }
    case hsql::ShowInstanceMode::IP: {
      std::string ip(p_statement->ip);
      OMS_INFO("{}: [show binlog instance] show instances in node with ip: {}", conn->trace_id(), ip);

      BinlogEntry condition;
      condition.set_ip(ip);
      if (OMS_OK != g_cluster->query_instances(instance_entries, condition)) {
        OMS_ERROR(
            "{}: [show binlog instance] Failed to fetch binlog instances in node with ip: {}", conn->trace_id(), ip);
      }
      break;
    }
    case hsql::ShowInstanceMode::ZONE: {
      std::string zone(p_statement->zone);
      OMS_INFO("{}: [show binlog instance] show instances in node with zone: {}", conn->trace_id(), zone);

      if (OMS_OK != g_cluster->query_instances(instance_entries, zone, SlotType::ZONE)) {
        OMS_ERROR("{}: [show binlog instance] Failed to fetch binlog instances in nodes with zone: {}",
            conn->trace_id(),
            zone);
      }
      break;
    }
    case hsql::ShowInstanceMode::REGION: {
      std::string region(p_statement->region);
      OMS_INFO("{}: [show binlog instance] show instances in node with region: {}", conn->trace_id(), region);

      if (OMS_OK != g_cluster->query_instances(instance_entries, region, SlotType::REGION)) {
        OMS_ERROR("{}: [show binlog instance] Failed to fetch binlog instances in nodes with region: {}",
            conn->trace_id(),
            region);
      }
      break;
    }
    case hsql::ShowInstanceMode::GROUP: {
      std::string group(p_statement->group);
      OMS_INFO("{}: [show binlog instance] show instances in node with group: {}", conn->trace_id(), group);

      if (OMS_OK != g_cluster->query_instances(instance_entries, group, SlotType::GROUP)) {
        OMS_ERROR("{}: [show binlog instance] Failed to fetch binlog instances in nodes with group: {}",
            conn->trace_id(),
            group);
      }
      break;
    }
    default:
      OMS_WARN("{}: [show binlog instance] Unsupported mode: {}", conn->trace_id(), mode);
  }

  uint32_t index = 0;
  uint32_t count = 0;
  std::map<BinlogEntry*, ResultSet*> instances_metrics;
  std::map<std::string, Node> nodes;
  std::set<std::string> node_ids;
  std::vector<ResultSet*> result_vec;
  defer(release_vector(result_vec));
  timer.reset();
  for (BinlogEntry* instance : instance_entries) {
    if (!history && instance->is_dropped()) {
      continue;
    }
    if (index++ < offset) {
      continue;
    }
    if (count >= limit) {
      break;
    }

    node_ids.emplace(instance->node_id());
    ResultSet* result = new ResultSet();
    g_sql_executor->submit(InstanceClientWrapper::report_asyn, *instance, result);
    instances_metrics.emplace(instance, result);
    result_vec.emplace_back(result);
    count += 1;
  }
  OMS_INFO("{}: [show binlog instance] report_asyn {} instances,time consuming {}",
      conn->trace_id(),
      count,
      timer.elapsed());
  g_cluster->query_nodes(node_ids, nodes);
  if (nodes.empty()) {
    OMS_ERROR("{}: [show binlog instances] Failed to obtain [{}] nodes related to instances",
        conn->trace_id(),
        node_ids.size());
  }

  for (const auto& entry : instances_metrics) {
    bool waiting = true;
    while (waiting) {
      if (entry.second->state == ResultSetState::WAITING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        OMS_DEBUG("{}: [show binlog instances] sleep 0.2s waiting for report instance {}",
            conn->trace_id(),
            entry.first->instance_name());
        continue;
      }

      waiting = false;
      conn->start_row();
      conn->store_string(entry.first->instance_name());
      conn->store_string(entry.first->cluster());
      conn->store_string(entry.first->tenant());
      conn->store_string(entry.first->ip());
      conn->store_uint64(entry.first->port());

      // node info
      auto iter = nodes.find(entry.first->node_id());
      Node node;
      if (iter != nodes.end()) {
        node = iter->second;
      } else {
        OMS_WARN("{}: [show binlog instance] Not found node {} of instance {}",
            conn->trace_id(),
            entry.first->node_id(),
            entry.first->instance_name());
      }

      conn->store_string(node.zone());
      conn->store_string(node.region());
      conn->store_string(node.group());

      // running state
      conn->store_string(entry.first->can_connected() ? "Yes" : "No");
      conn->store_string(instance_state_str(entry.first->state()));
      conn->store_string(entry.first->can_connected() ? "Yes" : "No");
      conn->store_string(instance_state_str(entry.first->state()));
      conn->store_string(entry.first->can_connected() ? "enabled" : "disabled");

      // OBI metrics
      if (entry.second->state == ResultSetState::READY) {
        conn->store_string(entry.second->mysql_result_set.rows.front().fields()[0]);
        conn->store_uint64(std::atoll(entry.second->mysql_result_set.rows.front().fields()[1].c_str()));
        conn->store_uint64(std::atoll(entry.second->mysql_result_set.rows.front().fields()[2].c_str()));
        conn->store_uint64(std::atoll(entry.second->mysql_result_set.rows.front().fields()[3].c_str()));
        conn->store_uint64(std::atoll(entry.second->mysql_result_set.rows.front().fields()[4].c_str()));
        conn->store_uint64(std::atoll(entry.second->mysql_result_set.rows.front().fields()[5].c_str()));
      } else {
        conn->store_string("No");
        conn->store_null();
        conn->store_null();
        conn->store_null();
        conn->store_null();
        conn->store_null();
      }
      conn->store_string(entry.first->config()->version());
      conn->store_null();

      IoResult send_ret = conn->send_row();
      if (send_ret != IoResult::SUCCESS) {
        return send_ret;
      }
    }
  }

  return conn->send_eof_packet();
}

IoResult ShowBinlogStatusProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  ColumnPacket cluster_column_packet{
      "cluster", "", UTF8_CS, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket tenant_column_packet{
      "tenant", "", UTF8_CS, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket status_column_packet{
      "status", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};

  if (conn->send_result_metadata({
          cluster_column_packet,
          tenant_column_packet,
          status_column_packet,
      }) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowBinlogStatusStatement*)statement;
  std::vector<BinlogEntry*> instance_entries;
  defer(release_vector(instance_entries));
  BinlogEntry condition;
  if (p_statement->tenant != nullptr) {
    condition.set_cluster(p_statement->tenant->cluster);
    condition.set_tenant(p_statement->tenant->tenant);
    OMS_INFO("{}: [show binlog status] cluster: {}, tenant: {}",
        conn->trace_id(),
        p_statement->tenant->cluster,
        p_statement->tenant->cluster);
  }
  condition.set_state(InstanceState::RUNNING);
  g_cluster->query_instances(instance_entries, condition);

  std::map<std::string, std::pair<std::string, std::string>> tenants;
  for (auto entry : instance_entries) {
    tenants[entry->full_tenant()] = std::pair(entry->cluster(), entry->tenant());
  }
  for (auto& entry_pair : tenants) {
    std::string master_instance_name;
    g_cluster->query_master_instance(entry_pair.second.first, entry_pair.second.second, master_instance_name);
    if (master_instance_name.empty()) {
      OMS_ERROR(
          "{}: [show binlog status] Tenant [{}] has no master service instance", conn->trace_id(), entry_pair.first);
      continue;
    }

    BinlogEntry master_instance;
    g_cluster->query_instance_by_name(master_instance_name, master_instance);
    if (master_instance.instance_name().empty() || master_instance.state() != InstanceState::RUNNING) {
      OMS_ERROR("{}: [show binlog status] Master instance [{}] of tenant [{}] is unavailable",
          conn->trace_id(),
          master_instance_name,
          entry_pair.first);
      continue;
    }

    // row packet
    conn->start_row();
    conn->store_string(master_instance.cluster());
    conn->store_string(master_instance.tenant());
    std::string status;
    serialize_binlog_metrics(status, master_instance);
    conn->store_string(status);
    IoResult send_ret = conn->send_row();
    if (send_ret != IoResult::SUCCESS) {
      return send_ret;
    }
  }
  return conn->send_eof_packet();
}

void ShowBinlogStatusProcessor::serialize_binlog_metrics(std::string& status, const BinlogEntry& entry)
{
  // 1. collect process resources metrics
  ProcessMetric process_metric(true);
  collect_metric(entry, process_metric);

  // 2. collect binlog files info
  MySQLResultSet result_set;
  collect_binlog(entry, result_set);

  // 3. assemble as json
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("binlog_instance");
  writer.String(entry.instance_name().c_str());
  writer.Key("resources_metrics");
  rapidjson::Document document;
  rapidjson::Value resources_metrics = process_metric.serialize_to_json_value(document);
  resources_metrics.Accept(writer);
  writer.Key("binlog_files");
  writer.StartArray();
  for (const MySQLRow& row : result_set.rows) {
    writer.StartObject();
    writer.Key("binlog_name");
    writer.String(row.fields()[0].c_str());
    writer.Key("binlog_size");
    writer.Uint64(std::atoll(row.fields()[1].c_str()));
    writer.EndObject();
  }
  writer.EndArray();
  writer.EndObject();

  status = buffer.GetString();
}

void ShowBinlogStatusProcessor::collect_metric(const BinlogEntry& instance, ProcessMetric& metric)
{
  Node node;
  if (OMS_OK != g_cluster->query_node_by_id(instance.node_id(), node)) {
    OMS_ERROR("Failed to get node info for binlog instance: {}", instance.instance_name());
    return;
  }

  for (auto* p_metric : node.metric()->process_group_metric()->metric_group().get_items()) {
    auto p_process_metric = (ProcessMetric*)p_metric;
    if (p_process_metric->pid() == instance.pid()) {
      metric.assign(p_process_metric);
    }
  }
}

void ShowBinlogStatusProcessor::collect_binlog(const BinlogEntry& instance, MySQLResultSet& result_set)
{
  InstanceClient instance_client;
  if (OMS_OK != instance_client.init(instance)) {
    OMS_ERROR("Failed to connect instance: {}", instance.instance_name());
    return;
  }

  int ret = instance_client.query("SHOW BINARY LOGS", result_set);
  if (OMS_OK != ret) {
    OMS_ERROR("[show binlog status] Failed to query binary logs of instance with addr: {}",
        instance_client.get_server_addr());
    return;
  }
}

IoResult SetVarProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::SetStatement*)statement;
  /*
   * Select the required system variables or user variables
   */
  for (hsql::SetClause* var : *(p_statement->sets)) {
    switch (var->type) {
      case hsql::Session: {
        if (var->column != nullptr && var->value != nullptr) {
          OMS_INFO("{}: set session var {}={}", conn->trace_id(), std::string(var->column), var->value->get_value());
          conn->set_session_var(var->column, var->value->get_value());
        }
        break;
      }
      case hsql::Global: {
        if (var->column != nullptr && var->value != nullptr) {
          OMS_INFO("{}: Set global var {}=[{}]", conn->trace_id(), std::string(var->column), var->value->get_value());
          if (strcmp(var->column, "server_uuid") == 0) {
            return conn->send_err_packet(BINLOG_FATAL_ERROR, "Variable 'server_uuid' is a read only variable", "HY000");
          }

          // setting the binlog option of instance
          Model* value = s_meta.binlog_config()->get_plain(std::string(var->column));
          if (nullptr != value) {
            OMS_INFO("{}: Success set binlog variable {}: [{}] -> [{}]",
                conn->trace_id(),
                var->column,
                value->str(),
                var->value->get_value());
            s_meta.binlog_config()->set_plain(std::string(var->column), var->value->get_value());

            sync_set_if_throttle_options(std::string(var->column), std::string(var->value->get_value()));
            break;
          }

          // setting the obcdc option of instance
          Model* obcdc_value = s_meta.cdc_config()->get_plain(std::string(var->column));
          if (nullptr != obcdc_value) {
            OMS_INFO("{}: Success set obcdc variable {}: {} -> {}",
                conn->trace_id(),
                var->column,
                obcdc_value->str(),
                var->value->get_value());
            s_meta.cdc_config()->set_plain(std::string(var->column), var->value->get_value());
          }

          // TODO Temporarily do not process global system variables compatible with mysql
        }
        break;
      }
      case hsql::Local: {
        if (var->column != nullptr && var->value != nullptr) {
          /**
           * TODO Temporarily do not process local variables
           */

          OMS_INFO("{}: set local var {}={}", conn->trace_id(), std::string(var->column), var->value->get_value());
        }
        break;
      }
    }
  }
  conn->send_ok_packet();
  return IoResult::SUCCESS;
}

void SetVarProcessor::sync_set_if_throttle_options(const std::string& col, const std::string& value)
{
  if (strcasecmp(col.c_str(), "throttle_convert_rps") == 0) {
    BinlogConverter::instance().update_throttle_rps(std::atoll(value.c_str()));
  } else if (strcasecmp(col.c_str(), "throttle_convert_iops") == 0) {
    BinlogConverter::instance().update_throttle_iops(std::atoll(value.c_str()));
  } else if (strcasecmp(col.c_str(), "throttle_dump_rps") == 0) {
    g_dumper_manager->update_throttle_rps(std::atoll(value.c_str()));
  } else if (strcasecmp(col.c_str(), "throttle_dump_iops") == 0) {
    g_dumper_manager->update_throttle_iops(std::atoll(value.c_str()));
  } else if (strcasecmp(col.c_str(), "throttle_dump_conn") == 0) {
    g_dumper_manager->update_total_limit(std::atoi(value.c_str()));
  }
}

IoResult ShowVarProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  ColumnPacket name_column_packet{
      "Variable_name", "", UTF8_CS, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket value_column_packet{
      "Value", "", UTF8_CS, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};

  if (conn->send_result_metadata({
          name_column_packet,
          value_column_packet,
      }) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowStatement*)statement;
  switch (p_statement->type) {
    case hsql::kShowColumns:
    case hsql::kShowTables:
      break;
    case hsql::kShowVar: {
      if (nullptr == p_statement->instance_name && conn->get_conn_type() == Connection::ConnectionType::OBI) {
        if (p_statement->_var_name != nullptr) {
          return show_specified_var(conn, p_statement);
        } else {
          return show_non_specified_var(conn, p_statement);
        }
      } else {
        std::string instance_name(p_statement->instance_name);
        BinlogEntry instance;
        if (OMS_OK != g_cluster->query_instance_by_name(instance_name, instance)) {
          OMS_ERROR("{}: [show variables] Failed to query meta for instance: {}", conn->trace_id(), instance_name);
          return conn->send_err_packet(
              BINLOG_FATAL_ERROR, "Internal error: failed to query meta for instance", "HY000");
        }
        if (instance.instance_name().empty() ||
            (instance.state() != InstanceState::RUNNING && instance.state() != InstanceState::GRAYSCALE)) {
          OMS_ERROR("{}: [show variables] instance does not exist or are not in running status: {}",
              conn->trace_id(),
              instance.serialize_to_json());
          return conn->send_err_packet(
              BINLOG_FATAL_ERROR, "binlog instance does not exist or is not in running status", "HY000");
        }

        InstanceClient client;
        if (OMS_OK != client.init(instance)) {
          return conn->send_err_packet(BINLOG_FATAL_ERROR, "failed to connect instance: " + instance_name, "HY000");
        }

        std::string show_sql = gen_show_var_sql(p_statement);
        OMS_INFO("{}: [show var for instance] SQL: {}", conn->trace_id(), show_sql);
        MySQLResultSet rs;
        if (OMS_OK != client.query(show_sql, rs)) {
          return conn->send_err_packet(
              BINLOG_FATAL_ERROR, "failed to show variables for instance: " + instance_name, "HY000");
        }

        for (const MySQLRow& row : rs.rows) {
          conn->start_row();
          conn->store_string(row.fields()[0]);
          conn->store_string(row.fields()[1]);
          IoResult send_ret = conn->send_row();
          if (send_ret != IoResult::SUCCESS) {
            return send_ret;
          }
        }
      }
    }
  }
  return conn->send_eof_packet();
}

IoResult ShowVarProcessor::show_non_specified_var(Connection* conn, const hsql::ShowStatement* p_statement)
{
  switch (p_statement->var_type) {
    case hsql::Global: {
      // variables compatible with mysql
      for (const auto& var : g_sys_var->get_all_global_var()) {
        conn->start_row();
        conn->store_string(var.first);
        conn->store_string(var.second);
        IoResult send_ret = conn->send_row();
        if (send_ret != IoResult::SUCCESS) {
          return send_ret;
        }
      }
      // binlog variables of instance
      for (const auto& binlog_var : s_meta.binlog_config()->get_plains()) {
        conn->start_row();
        conn->store_string(binlog_var.first);
        conn->store_string(binlog_var.second->str());
        IoResult send_ret = conn->send_row();
        if (send_ret != IoResult::SUCCESS) {
          return send_ret;
        }
      }
      // obcdc variables of instance
      for (const auto& cdc_var : s_meta.cdc_config()->get_plains()) {
        if (strcmp(cdc_var.first.c_str(), "cluster_password") == 0) {
          continue;
        }

        conn->start_row();
        conn->store_string(cdc_var.first);
        conn->store_string(cdc_var.second->str());
        IoResult send_ret = conn->send_row();
        if (send_ret != IoResult::SUCCESS) {
          return send_ret;
        }
      }
      // slot variables of instance
      for (const auto& slot_var : s_meta.slot_config()->get_plains()) {
        conn->start_row();
        conn->store_string(slot_var.first);
        conn->store_string(slot_var.second->str());
        IoResult send_ret = conn->send_row();
        if (send_ret != IoResult::SUCCESS) {
          return send_ret;
        }
      }
      return conn->send_eof_packet();
    }
    case hsql::Local:
      break;
    case hsql::Session: {
      auto vars = conn->get_session_var();
      for (const auto& var : vars) {
        // row packet
        conn->start_row();
        conn->store_string(var.first);
        conn->store_string(var.second);
        IoResult send_ret = conn->send_row();
        if (send_ret != IoResult::SUCCESS) {
          return send_ret;
        }
      }
    }
  }
  return conn->send_eof_packet();
}

IoResult ShowVarProcessor::show_specified_var(Connection* conn, const hsql::ShowStatement* p_statement)
{
  string value;
  std::string var_name;
  if (p_statement->_var_name != nullptr) {
    var_name = p_statement->_var_name;
  }
  std::string err_msg;

  // Ignore case of variable names
  std::transform(var_name.begin(), var_name.end(), var_name.begin(), [](unsigned char c) -> unsigned char {
    return std::tolower(c);
  });

  if (query_var(conn, var_name, value, p_statement->var_type, err_msg) != OMS_OK) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, err_msg, "HY000");
  }

  // row packet
  conn->start_row();
  conn->store_string(var_name);
  conn->store_string(value);
  IoResult send_ret = conn->send_row();
  if (send_ret != IoResult::SUCCESS) {
    return send_ret;
  }
  return conn->send_eof_packet();
}

int ShowVarProcessor::query_var(
    Connection* conn, std::string& var_name, std::string& value, hsql::VarLevel type, std::string& err_msg)
{
  switch (type) {
    case hsql::Global: {
      // not support query "cluster_password"
      if (strcmp(var_name.c_str(), "cluster_password") == 0) {
        return OMS_OK;
      }

      // 1. find from binlog variables of instance
      Model* instance_value = s_meta.binlog_config()->get_plain(var_name);
      if (nullptr != instance_value) {
        value = instance_value->str();
        return OMS_OK;
      }
      // 2. find from obcdc variables of instance
      instance_value = s_meta.cdc_config()->get_plain(var_name);
      if (nullptr != instance_value) {
        value = instance_value->str();
        return OMS_OK;
      }
      // 3. find from slot variables of instance
      instance_value = s_meta.slot_config()->get_plain(var_name);
      if (nullptr != instance_value) {
        value = instance_value->str();
        return OMS_OK;
      }
      // 4. find from variables compatible with mysql
      if (OMS_OK == g_sys_var->get_global_var(var_name, value)) {
        return OMS_OK;
      }
      err_msg = "Unknown system variable " + var_name;
      return OMS_FAILED;
      break;
    }
    case hsql::Local: {
      if (g_sys_var->get_global_var(var_name, value) == OMS_OK) {
        err_msg = "Variable '" + var_name + "' is a GLOBAL variable ";
        return OMS_FAILED;
      }
      break;
    }
    case hsql::Session: {
      if (g_sys_var->get_global_var(var_name, value) == OMS_OK) {
        err_msg = "Variable '" + var_name + "' is a GLOBAL variable ";
        return OMS_FAILED;
      }
      value = conn->get_session_var(var_name);
      break;
    }
  }
  return OMS_OK;
}

std::string ShowVarProcessor::gen_show_var_sql(const hsql::ShowStatement* p_statement)
{
  std::string sql = "SHOW ";
  switch (p_statement->var_type) {
    case hsql::Global: {
      sql.append("GLOBAL VARIABLES");
      break;
    }
    case hsql::Local: {
      sql.append("LOCAL VARIABLES");
      break;
    }
    case hsql::Session: {
      sql.append("SESSION VARIABLES");
      break;
    }
    default:
      sql.append("VARIABLES");
  }
  if (nullptr != p_statement->_var_name) {
    sql.append(" LIKE '").append(p_statement->_var_name).append("'");
  }
  return sql;
}

IoResult SelectProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::SelectStatement*)statement;

  if (p_statement->selectList->empty()) {
    return conn->send_eof_packet();
  }
  std::string var_name = p_statement->selectList->at(0)->getName();
  hsql::ExprType type = p_statement->selectList->at(0)->type;

  switch (type) {
    case hsql::kExprVar:
      return handle_query_var(conn, p_statement);
    case hsql::kExprFunctionRef:
      return handle_function(conn, p_statement);
    default:
      // do nothing
      return conn->send_eof_packet();
  }
}

IoResult SelectProcessor::handle_query_var(Connection* conn, const hsql::SelectStatement* p_statement)
{
  std::string var_name = p_statement->selectList->at(0)->getName();
  hsql::VarLevel var_level = p_statement->selectList->at(0)->var_level;
  std::string var_column = var_level == hsql::Global ? ("@@" + var_name) : ("@" + var_name);
  ColumnPacket var_column_packet{
      var_column, "", UTF8_CS, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  if (conn->send_result_metadata({
          var_column_packet,
      }) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  std::string value;
  std::string err_msg;
  // Ignore case of variable names
  std::transform(var_name.begin(), var_name.end(), var_name.begin(), [](unsigned char c) -> unsigned char {
    return std::tolower(c);
  });
  if (ShowVarProcessor::query_var(conn, var_name, value, var_level, err_msg) != OMS_OK) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, err_msg, "HY000");
  }
  // row packet
  conn->start_row();
  conn->store_string(value);
  IoResult send_ret = conn->send_row();
  if (send_ret != IoResult::SUCCESS) {
    return send_ret;
  }
  return conn->send_eof_packet();
}

IoResult SelectProcessor::handle_function(Connection* conn, hsql::SelectStatement* p_statement)
{
  std::string var_name = p_statement->selectList->at(0)->getName();
  // convert all to uppercase
  std::transform(var_name.begin(), var_name.end(), var_name.begin(), [](unsigned char c) -> unsigned char {
    return std::toupper(c);
  });
  auto p_func_processor = func_processor(var_name);
  if (p_func_processor != nullptr) {
    uint16_t utf8_cs = 33;
    ColumnPacket var_column_packet{p_func_processor->function_def(p_statement),
        "",
        utf8_cs,
        56,
        ColumnType::ct_var_string,
        ColumnDefinitionFlags::pri_key_flag,
        31};
    if (conn->send_result_metadata({
            var_column_packet,
        }) != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
    return p_func_processor->process(conn, p_statement);
  }
  return conn->send_eof_packet();
}

IoResult ShowProcesslistProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  ColumnPacket id_column_packet{"Id",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::pri_key_flag,
      0};
  ColumnPacket user_column_packet{
      "User", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket host_column_packet{
      "Host", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket db_column_packet{
      "db", "", UTF8_CS, 8, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket command_column_packet{
      "Command", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket time_column_packet{"Time",
      "",
      BINARY_CS,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket state_column_packet{
      "State", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket info_column_packet{
      "Info", "", UTF8_CS, 8, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};

  if (conn->send_result_metadata({id_column_packet,
          user_column_packet,
          host_column_packet,
          db_column_packet,
          command_column_packet,
          time_column_packet,
          state_column_packet,
          info_column_packet}) != IoResult::SUCCESS) {
    OMS_ERROR("{}: [show processlist] Failed to send metadata", conn->trace_id());
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowProcessListStatement*)statement;
  bool full = p_statement->full;
  std::string instance_name = std::string(p_statement->instance_name == nullptr ? "" : p_statement->instance_name);
  OMS_INFO("{}: [show processlist] show full: {}, instance name: {}", conn->trace_id(), full, instance_name);

  if (instance_name.empty()) {
    std::vector<ProcessInfo> conn_info_vec;
    g_connection_manager->get_conn_info(conn_info_vec);
    uint64_t now_s = Timer::now_s();
    for (const auto& conn_info : conn_info_vec) {
      conn->start_row();
      conn->store_uint64(conn_info.id);
      conn->store_string(conn_info.user);
      conn->store_string(conn_info.host);
      conn->store_null();
      conn->store_string(server_command_names(conn_info.command));
      conn->store_uint64(now_s - conn_info.command_start_ts);
      conn->store_string(process_state_info(conn_info.state));
      std::string info = conn_info.info;
      if (!full && info.length() >= 100) {
        info = info.substr(0, 100);
      }
      conn->store_string(info);
      IoResult send_ret = conn->send_row();
      if (send_ret != IoResult::SUCCESS) {
        return send_ret;
      }
    }
  } else if (conn->get_conn_type() == Connection::ConnectionType::OBM) {
    BinlogEntry instance;
    if (OMS_OK != g_cluster->query_instance_by_name(instance_name, instance)) {
      OMS_ERROR("{}: [show processlist] Failed to query meta for instance: {}", conn->trace_id(), instance_name);
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error: failed to query meta for instance", "HY000");
    }
    if (instance.instance_name().empty() ||
        (instance.state() != InstanceState::RUNNING && instance.state() != InstanceState::GRAYSCALE)) {
      OMS_ERROR("{}: [show processlist] instance does not exist or are not in running status: {}",
          conn->trace_id(),
          instance.serialize_to_json());
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "binlog instance does not exist or is not in running status", "HY000");
    }

    InstanceClient client;
    if (OMS_OK != client.init(instance)) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "failed to connect instance: " + instance_name, "HY000");
    }

    std::string show_sql = full ? "SHOW FULL PROCESSLIST" : "SHOW PROCESSLIST";
    MySQLResultSet rs;
    if (OMS_OK != client.query(show_sql, rs)) {
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "failed to show processlist for instance: " + instance_name, "HY000");
    }

    for (const MySQLRow& row : rs.rows) {
      conn->start_row();
      conn->store_uint64(std::atoll(row.fields()[0].c_str()));
      conn->store_string(row.fields()[1]);
      conn->store_string(row.fields()[2]);
      conn->store_null();
      conn->store_string(row.fields()[4]);
      conn->store_uint64(std::atoll(row.fields()[5].c_str()));
      conn->store_string(row.fields()[6]);
      conn->store_string(row.fields()[7]);
      IoResult send_ret = conn->send_row();
      if (send_ret != IoResult::SUCCESS) {
        return send_ret;
      }
    }
  } else {
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "Unsupported show processlist for instance: " + instance_name, "HY000");
  }

  return conn->send_eof_packet();
}

IoResult ShowDumplistProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  ColumnPacket id_column_packet{"Id",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::pri_key_flag,
      0};
  ColumnPacket user_column_packet{
      "User", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket host_column_packet{
      "Host", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket real_host_column_packet{
      "RealHost", "", UTF8_CS, 8, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket command_column_packet{
      "Command", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket time_column_packet{"Time",
      "",
      BINARY_CS,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket state_column_packet{
      "State", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket dump_position_column_packet{
      "DumpPosition", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket dump_delay_column_packet{"DumpDelay",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket dump_events_num_column_packet{"DumpEventsNum",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket dump_events_size_column_packet{"DumpEventsSize",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket fake_rotate_event_num_column_packet{"FakeRotateEventsNum",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket hb_event_num_column_packet{"HeartbeatEventsNum",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket dump_eps_column_packet{"DumpEps",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket dump_iops_column_packet{"DumpIops",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket info_column_packet{
      "Info", "", UTF8_CS, 8, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket dump_checkpoint_column_packet{"DumpCheckpoint",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  if (conn->send_result_metadata({id_column_packet,
          user_column_packet,
          host_column_packet,
          real_host_column_packet,
          command_column_packet,
          time_column_packet,
          state_column_packet,
          dump_position_column_packet,
          dump_delay_column_packet,
          dump_events_num_column_packet,
          dump_events_size_column_packet,
          fake_rotate_event_num_column_packet,
          hb_event_num_column_packet,
          dump_eps_column_packet,
          dump_iops_column_packet,
          info_column_packet,
          dump_checkpoint_column_packet}) != IoResult::SUCCESS) {
    OMS_ERROR("{}: [show binlog dumplist] Failed to send metadata", conn->trace_id());
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowDumpListStatement*)statement;
  bool full = p_statement->full;
  std::string instance_name = std::string(p_statement->instance_name == nullptr ? "" : p_statement->instance_name);
  OMS_INFO("{}: [show binlog dumplist] show full: {}, instance name: {}", conn->trace_id(), full, instance_name);

  if (instance_name.empty() && conn->get_conn_type() == Connection::ConnectionType::OBI) {
    std::vector<DumpInfo> dump_info_vec;
    g_dumper_manager->get_dump_info(dump_info_vec);
    uint64_t now_s = Timer::now_s();
    for (const auto& dump_info : dump_info_vec) {
      conn->start_row();
      ProcessInfo conn_info = dump_info.conn_info;
      conn->store_uint64(conn_info.id);
      conn->store_string(conn_info.user);
      conn->store_string(conn_info.host);
      conn->store_string(conn_info.real_host);
      conn->store_string(server_command_names(conn_info.command));
      conn->store_uint64(now_s - conn_info.command_start_ts);
      conn->store_string(process_state_info(conn_info.state));
      conn->store_string(dump_info.dump_position);
      conn->store_uint64(dump_info.dump_delay);
      conn->store_uint64(dump_info.dump_events_num);
      conn->store_uint64(dump_info.dump_events_size);
      conn->store_uint64(dump_info.fake_rotate_events_num);
      conn->store_uint64(dump_info.hb_events_num);
      conn->store_uint64(dump_info.dump_eps);
      conn->store_uint64(dump_info.dump_iops);
      std::string info = dump_info.info;
      if (!full && info.length() >= 100) {
        info = info.substr(0, 100);
      }
      conn->store_string(info);
      conn->store_uint64(dump_info.dump_checkpoint);
      IoResult send_ret = conn->send_row();
      if (send_ret != IoResult::SUCCESS) {
        return send_ret;
      }
    }
  } else if (conn->get_conn_type() == Connection::ConnectionType::OBM) {
    BinlogEntry instance;
    if (OMS_OK != g_cluster->query_instance_by_name(instance_name, instance)) {
      OMS_ERROR("{}: [show binlog dumplist] Failed to query meta for instance: {}", conn->trace_id(), instance_name);
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error: failed to query meta for instance", "HY000");
    }
    if (instance.instance_name().empty() ||
        (instance.state() != InstanceState::RUNNING && instance.state() != InstanceState::GRAYSCALE)) {
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "binlog instance does not exist or is not in running status", "HY000");
    }

    InstanceClient client;
    if (OMS_OK != client.init(instance)) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "failed to connect instance: " + instance_name, "HY000");
    }

    std::string show_sql = full ? "SHOW FULL BINLOG DUMPLIST" : "SHOW BINLOG DUMPLIST";
    MySQLResultSet rs;
    if (OMS_OK != client.query(show_sql, rs)) {
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "failed to show binlog dumplist for instance: " + instance_name, "HY000");
    }

    for (const MySQLRow& row : rs.rows) {
      conn->start_row();
      conn->store_uint64(std::atoll(row.fields()[0].c_str()));
      conn->store_string(row.fields()[1]);
      conn->store_string(row.fields()[2]);
      conn->store_string(row.fields()[3]);
      conn->store_string(row.fields()[4]);
      conn->store_uint64(std::atoll(row.fields()[5].c_str()));
      conn->store_string(row.fields()[6]);
      conn->store_string(row.fields()[7]);
      conn->store_uint64(std::atoll(row.fields()[8].c_str()));
      conn->store_uint64(std::atoll(row.fields()[9].c_str()));
      conn->store_uint64(std::atoll(row.fields()[10].c_str()));
      conn->store_uint64(std::atoll(row.fields()[11].c_str()));
      conn->store_uint64(std::atoll(row.fields()[12].c_str()));
      conn->store_uint64(std::atoll(row.fields()[13].c_str()));
      conn->store_uint64(std::atoll(row.fields()[14].c_str()));
      conn->store_string(row.fields()[15]);
      conn->store_uint64(row.fields().size() > 16 ? std::atoll(row.fields()[16].c_str()) : Timer::now_s());
      IoResult send_ret = conn->send_row();
      if (send_ret != IoResult::SUCCESS) {
        return send_ret;
      }
    }
  } else {
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "Unsupported show binlog dumplist for instance: " + instance_name, "HY000");
  }

  return conn->send_eof_packet();
}

IoResult KillProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::KillStatement*)statement;
  bool only_kill_query = p_statement->only_kill_query;
  std::string conn_id = p_statement->process_id->get_value();
  int thread_id = std::atoi(conn_id.c_str());
  std::string instance_name = std::string(p_statement->instance_name == nullptr ? "" : p_statement->instance_name);

  OMS_INFO("{}: [kill]: killed connection: {}, only kill query: {}, instance: {}",
      conn->trace_id(),
      conn_id,
      !only_kill_query,
      instance_name);

  if (instance_name.empty()) {
    if (conn->get_thread_id() != thread_id) {
      g_connection_manager->kill_connection(
          thread_id, only_kill_query ? KilledState::KILL_QUERY : KilledState::KILL_CONNECTION);
    }
  } else if (conn->get_conn_type() == Connection::ConnectionType::OBM) {
    BinlogEntry instance;
    if (OMS_OK != g_cluster->query_instance_by_name(instance_name, instance)) {
      OMS_ERROR("{}: [kill] Failed to query meta for instance: {}", conn->trace_id(), instance_name);
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "Internal error: failed to query meta for instance", "HY000");
    }
    if (instance.instance_name().empty() ||
        (instance.state() != InstanceState::RUNNING && instance.state() != InstanceState::GRAYSCALE)) {
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "binlog instance does not exist or is not in running status", "HY000");
    }

    InstanceClient client;
    if (OMS_OK != client.init(instance)) {
      return conn->send_err_packet(BINLOG_FATAL_ERROR, "failed to connect instance: " + instance_name, "HY000");
    }

    std::string flag = only_kill_query ? "QUERY" : "CONNECTION";
    std::string kill_sql = "KILL " + flag + " " + conn_id;
    MySQLResultSet rs;
    if (OMS_OK != client.query(kill_sql, rs)) {
      return conn->send_err_packet(
          BINLOG_FATAL_ERROR, "failed to kill connection for instance: " + instance_name, "HY000");
    }
  } else {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Unsupported kill instance: " + instance_name, "HY000");
  }

  return conn->send_ok_packet();
}

IoResult ReportProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  ColumnPacket running_column_packet{
      "convert_running", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket convert_delay_column_packet{"convert_delay",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket convert_rps_column_packet{"convert_rps",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket convert_eps_column_packet{"convert_eps",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket convert_iops_column_packet{"convert_iops",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket nof_dump_column_packet{"nof_dump",
      "",
      BINARY_CS,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket minimum_dump_point_column_packet{"min_dump_checkpoint",
      "",
      BINARY_CS,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket gtid_seq_column_packet{
      "incr_gtid_seq", "", UTF8_CS, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::blob_flag, 31};
  ColumnPacket last_gtid_purged_column_packet{
      "last_gtid_purged", "", BINARY_CS, 8, ColumnType::ct_longlong, ColumnDefinitionFlags::binary_flag, 0};
  ColumnPacket convert_checkpoint_column_packet{"convert_checkpoint",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket dump_error_count_column_packet{"dump_error_count",
      "",
      BINARY_CS,
      20,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::not_null_flag,
      0};
  ColumnPacket last_checkpoint_purged_column_packet{
      "last_checkpoint_purged", "", BINARY_CS, 8, ColumnType::ct_longlong, ColumnDefinitionFlags::binary_flag, 0};
  if (conn->send_result_metadata({running_column_packet,
          convert_delay_column_packet,
          convert_rps_column_packet,
          convert_eps_column_packet,
          convert_iops_column_packet,
          nof_dump_column_packet,
          minimum_dump_point_column_packet,
          gtid_seq_column_packet,
          last_gtid_purged_column_packet,
          convert_checkpoint_column_packet,
          dump_error_count_column_packet,
          last_checkpoint_purged_column_packet}) != IoResult::SUCCESS) {
    OMS_ERROR("[report] Failed to send metadata");
    return IoResult::FAIL;
  }

  bool convert_running = Counter::instance().is_run();
  uint64_t convert_delay_ms = Counter::instance().delay_us() / 1000;
  uint64_t convert_rps = Counter::instance().convert_rps();
  uint64_t convert_eps = Counter::instance().write_rps();
  uint64_t convert_iops = Counter::instance().write_iops();
  uint64_t convert_checkpoint = Counter::instance().checkpoint_us();
  uint64_t nof_dump = g_dumper_manager->dumper_num();
  uint64_t minimum_dump_point = g_dumper_manager->min_dumper_checkpoint();
  std::string incr_gtid_seq_str;
  uint64_t last_gtid_purged = 0;
  uint64_t last_checkpoint_purged = 0;

  auto* p_statement = (hsql::ReportStatement*)statement;
  if (nullptr != p_statement->timestamp) {
    std::string timestamp_str = p_statement->timestamp->get_value();
    OMS_INFO("{}: [report] timestamp: {}", conn->trace_id(), timestamp_str);

    uint64_t timestamp = std::atoll(timestamp_str.c_str());
    g_gtid_manager->get_instance_incr_gtids_string(timestamp, incr_gtid_seq_str);
    g_gtid_manager->get_last_gtid_purged(last_gtid_purged);
    g_gtid_manager->get_last_checkpoint_purged(last_checkpoint_purged);
  }

  conn->start_row();
  conn->store_string(convert_running ? "Yes" : "No");
  conn->store_uint64(convert_delay_ms);
  conn->store_uint64(convert_rps);
  conn->store_uint64(convert_eps);
  conn->store_uint64(convert_iops);
  conn->store_uint64(nof_dump);
  conn->store_uint64(minimum_dump_point);
  conn->store_string(incr_gtid_seq_str);
  conn->store_uint64(last_gtid_purged);
  conn->store_uint64(convert_checkpoint);
  conn->store_uint64(g_dumper_manager->get_dump_error_count());
  conn->store_uint64(last_checkpoint_purged);
  IoResult send_ret = conn->send_row();
  if (send_ret != IoResult::SUCCESS) {
    return send_ret;
  }

  return conn->send_eof_packet();
}

IoResult send_nodes_packet(Connection* conn, std::vector<Node*> nodes)
{
  for (const auto* node : nodes) {
    conn->start_row();
    conn->store_string(node->id());
    conn->store_string(node->ip());
    conn->store_uint64(node->port());
    conn->store_string(node->zone());
    conn->store_string(node->region());
    conn->store_string(node->group());
    conn->store_string(node_state_print(node->state()));
    conn->store_string(node->metric()->serialize_to_json());
    conn->store_string(node->node_config()->serialize_to_json());
    conn->store_uint64(node->incarnation());
    conn->store_uint64(node->last_modify());
    auto ret = conn->send_row();
    if (ret != IoResult::SUCCESS) {
      return ret;
    }
  }

  return conn->send_eof_packet();
}

IoResult ShowNodesProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  ColumnPacket id_column_packet{
      "id", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket ip_column_packet{
      "ip", "", UTF8_CS, 32, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket port_column_packet{
      "port", "", BINARY_CS, 16, ColumnType::ct_short, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket zone_column_packet{
      "zone", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket region_column_packet{
      "region", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket group_column_packet{
      "group", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket state_column_packet{
      "state", "", UTF8_CS, 128, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket metric_column_packet{
      "metric", "", UTF8_CS, 128, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket node_config_column_packet{
      "node_config", "", UTF8_CS, 128, ColumnType::ct_var_string, static_cast<ColumnDefinitionFlags>(0), 31};
  ColumnPacket incarnation_mode_column_packet{
      "incarnation", "", UTF8_CS, 20, ColumnType::ct_longlong, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket heartbeat_column_packet{
      "heartbeat", "", BINARY_CS, 20, ColumnType::ct_longlong, static_cast<ColumnDefinitionFlags>(0), 31};

  if (conn->send_result_metadata({id_column_packet,
          ip_column_packet,
          port_column_packet,
          zone_column_packet,
          region_column_packet,
          group_column_packet,
          state_column_packet,
          metric_column_packet,
          node_config_column_packet,
          incarnation_mode_column_packet,
          heartbeat_column_packet}) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowNodesStatement*)statement;
  std::vector<Node*> nodes;
  defer(release_vector(nodes));
  Node condition;
  switch (p_statement->option) {
    case hsql::NODE_ALL: {
      if (g_cluster->query_all_nodes(nodes) != OMS_OK) {
        OMS_ERROR("{}: [show nodes] query all nodes failed", conn->trace_id());
        return conn->send_eof_packet();
      }
      return send_nodes_packet(conn, nodes);
    }
    case hsql::NODE_IP: {
      condition.set_ip(p_statement->ip);
      break;
    }
    case hsql::NODE_ZONE: {
      condition.set_zone(p_statement->zone);
      break;
    }
    case hsql::NODE_GROUP: {
      condition.set_group(p_statement->group);
      break;
    }
    case hsql::NODE_REGION: {
      condition.set_region(p_statement->region);
      break;
    }
    default: {
      return conn->send_eof_packet();
    }
  }

  if (g_cluster->query_nodes(nodes, condition) != OMS_OK) {
    OMS_ERROR("{}: [show nodes] Failed to query nodes", conn->trace_id());
    return conn->send_eof_packet();
  }
  return send_nodes_packet(conn, nodes);
}

IoResult SwitchMasterInstanceProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::SwitchMasterInstanceStatement*)statement;
  std::string instance_name(p_statement->instance_name);
  std::string cluster(p_statement->tenant_name->cluster);
  std::string tenant(p_statement->tenant_name->tenant);
  bool full = p_statement->full;

  OMS_INFO("{}: [switch master] cluster: {}, tenant: {}, instance: {}, full: {}",
      conn->trace_id(),
      cluster,
      tenant,
      instance_name,
      full);
  BinlogEntry instance;
  g_cluster->query_instance_by_name(instance_name, instance);
  if (instance.instance_name().empty() || instance.state() != InstanceState::RUNNING) {
    return conn->send_err_packet(
        BINLOG_FATAL_ERROR, "binlog instance does not exist or is not in running status", "HY000");
  }
  if (strcmp(instance.cluster().c_str(), cluster.c_str()) != 0 ||
      strcmp(instance.tenant().c_str(), tenant.c_str()) != 0) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "binlog instance does not belong to tenant", "HY000");
  }

  std::string prev_master_instance_name;
  g_cluster->query_master_instance(cluster, tenant, prev_master_instance_name);
  BinlogEntry prev_master_instance;
  if (!prev_master_instance_name.empty()) {
    g_cluster->query_instance_by_name(prev_master_instance_name, prev_master_instance);
  }
  OMS_INFO("{}: [switch master] Tenant's [{}] previous master instance [{}(status: {})], and switch master instance "
           "to [{}]",
      conn->trace_id(),
      instance.full_tenant(),
      prev_master_instance.instance_name(),
      prev_master_instance.state(),
      instance.instance_name());
  PrometheusExposer::mark_binlog_counter_metric(instance.node_id(),
      instance.instance_name(),
      instance.cluster(),
      instance.tenant(),
      instance.cluster_id(),
      instance.tenant_id(),
      BINLOG_INSTANCE_MASTER_SWITCH_COUNT_TYPE);
  g_cluster->reset_master_instance(cluster, tenant, prev_master_instance_name);
  if (OMS_OK != g_cluster->determine_primary_instance(cluster, tenant, instance_name, prev_master_instance_name)) {
    OMS_ERROR("{}: [switch master] Failed to switch master instance from [{}] to [{}]",
        conn->trace_id(),
        prev_master_instance_name,
        instance_name);
    PrometheusExposer::mark_binlog_counter_metric(instance.node_id(),
        instance.instance_name(),
        instance.cluster(),
        instance.tenant(),
        instance.cluster_id(),
        instance.tenant_id(),
        BINLOG_INSTANCE_MASTER_SWITCH_FAILED_COUNT_TYPE);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "failed to switch master instance", "HY000");
  }

  bool success = true;
  if (full && prev_master_instance.state() == InstanceState::RUNNING) {
    OMS_INFO("{}: [switch master] Begin to kill dumper connections of previous master instance: {}",
        conn->trace_id(),
        prev_master_instance.instance_name());
    uint16_t killed_dumper_num = 0;
    InstanceClient client;
    if (OMS_OK != client.init(prev_master_instance)) {
      success = false;
      OMS_ERROR("{}: [switch master] Failed to connect to previous master instance: {}",
          conn->trace_id(),
          prev_master_instance.instance_name());
    } else {
      MySQLResultSet rs;
      if (OMS_OK != client.query("SHOW BINLOG DUMPLIST", rs)) {
        success = false;
        OMS_ERROR("{}: [switch master] Failed to query dump list of previous master instance: {}",
            conn->trace_id(),
            prev_master_instance.instance_name());
      }

      for (const MySQLRow& row : rs.rows) {
        MySQLResultSet kill_rs;
        std::string dumper_id = row.fields()[0];
        if (OMS_OK != client.query("KILL " + dumper_id, kill_rs)) {
          success = false;
          OMS_ERROR("{}: [switch master] Failed to kill dumper connection [{}], host: {}",
              conn->trace_id(),
              dumper_id,
              row.fields()[2]);
          continue;
        }
        killed_dumper_num += 1;
        OMS_INFO(
            "{}: [switch master] Kill dumper connection [{}], host: {}", conn->trace_id(), dumper_id, row.fields()[2]);
      }
    }

    OMS_INFO(
        "{}: [switch master] killed [{}] dumpers of previous master instance", conn->trace_id(), killed_dumper_num);
  }

  if (!success) {
    return conn->send_ok_packet(
        1, "all dumper subscriptions on the previous master instance are not completely disconnected");
  }
  return conn->send_ok_packet();
}

IoResult SetPasswordProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::SetPasswordStatement*)statement;
  std::string username(p_statement->user);
  std::string password(p_statement->password);

  if (username.empty()) {
    OMS_ERROR("{}: [set password] empty username");
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "User cannot be empty", "HY000");
  }
  OMS_INFO("{}: [set password]: username: {}, password: {}", conn->trace_id(), username, password);

  User user;
  if (OMS_OK != g_cluster->query_user_by_name(username, user)) {
    OMS_ERROR("{}: [set password] Failed to query user {}", conn->trace_id(), username);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "User cannot be empty", "HY000");
  }
  if (user.username().empty()) {
    OMS_ERROR("{}: [set password] Unknown user {}", conn->trace_id(), username);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "User " + username + " does noe exist", "HY000");
  }
  if (user.tag() == 0 && !user.password().empty()) {
    OMS_ERROR("{}: [set password] Not support to modify the password of sys user {} again", conn->trace_id(), username);
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Only support to modify the password of sys user once", "HY000");
  }

  if (OMS_OK != g_cluster->alter_user_password(user, password)) {
    OMS_ERROR("{}: [set password] Failed to alter password for user [{}]", conn->trace_id(), user.username());
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Set password failed", "HY000");
  }
  return conn->send_ok_packet();
}

}  // namespace oceanbase::binlog
