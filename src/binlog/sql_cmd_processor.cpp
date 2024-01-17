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

#include <algorithm>
#include "sql_cmd_processor.h"
#include "binlog_index.h"
#include "common_util.h"
#include "ob_log_event.h"
#include "binlog_state_machine.h"
#include "fork_thread.h"
#include "env.h"            // g_bc_executor
#include "binlog_dumper.h"  // BINLOG_FATAL_ERROR
#include "common_util.h"
#include "SQLParserResult.h"
#include "sql/show_binlog_events.h"
#include "sql/purge_binlog.h"
#include "sql/show_binlog_server.h"
#include "sql/drop_binlog.h"
#include "sql/show_binlog_status.h"
#include "sql/set_statement.h"
#include "sql/statements.h"

#include <unordered_map>

namespace oceanbase {
namespace binlog {
static std::unordered_map<hsql::StatementType, SqlCmdProcessor*> _s_supported_sql_cmd_processors = {
    {hsql::StatementType::COM_SHOW_BINLOGS, &ShowBinaryLogsProcessor::instance()},
    {hsql::StatementType::COM_SHOW_BINLOG_EVENTS, &ShowBinlogEventsProcessor::instance()},
    {hsql::StatementType::COM_SHOW_MASTER_STAT, &ShowMasterStatusProcessor::instance()},
    {hsql::StatementType::COM_PURGE_BINLOG, &PurgeBinaryLogsProcessor::instance()},
    {hsql::StatementType::COM_SHOW_BINLOG_SERVER, &ShowBinlogServerProcessor::instance()},
    {hsql::StatementType::COM_CREATE_BINLOG, &CreateBinlogProcessor::instance()},
    {hsql::StatementType::COM_DROP_BINLOG, &DropBinlogProcessor::instance()},
    {hsql::StatementType::COM_SHOW_BINLOG_STAT, &ShowBinlogStatusProcessor::instance()},
    {hsql::StatementType::COM_SELECT, &SelectProcessor::instance()},
    {hsql::StatementType::COM_SET, &SetVarProcessor::instance()},
    {hsql::StatementType::COM_SHOW, &ShowVarProcessor::instance()},
};

SqlCmdProcessor* sql_cmd_processor(hsql::StatementType type)
{
  auto iter = _s_supported_sql_cmd_processors.find(type);
  if (iter != _s_supported_sql_cmd_processors.end()) {
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

  vector<logproxy::BinlogIndexRecord*> index_records;
  int ret =
      logproxy::fetch_index_vector(conn->get_full_binlog_path() + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_records);
  if (ret != OMS_OK) {
    logproxy::release_vector(index_records);
    return conn->send_eof_packet();  // TODO: eof packet or error packet?
  }

  for (const auto& record : index_records) {
    conn->start_row();
    conn->store_string(CommonUtils::fill_binlog_file_name(record->get_index()));
    conn->store_uint64(record->get_position());
    if (conn->send_row() != IoResult::SUCCESS) {
      logproxy::release_vector(index_records);
      return IoResult::FAIL;
    }
  }
  logproxy::release_vector(index_records);
  return conn->send_eof_packet();
}

IoResult ShowBinlogEventsProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  OMS_INFO("Start to execute the show binlog event command");
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
    OMS_INFO("[show binlog event] binlog file:{}", binlog_file);
  }

  if (p_statement->start_pos != nullptr) {
    start_pos = p_statement->start_pos->get_value();
    OMS_INFO("[show binlog event] start pos:{}", start_pos);
  }

  if (p_statement->limit != nullptr) {
    if (p_statement->limit->offset != nullptr) {
      offset_str = p_statement->limit->offset->get_value();
      OMS_INFO("[show binlog event] offset:{}", offset_str);
    }

    if (p_statement->limit->limit != nullptr) {
      limit_str = p_statement->limit->limit->get_value();
      OMS_INFO("[show binlog event] limit:{}", limit_str);
    }
  }

  vector<logproxy::BinlogIndexRecord*> index_records;
  logproxy::fetch_index_vector(conn->get_full_binlog_path() + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_records);
  uint64_t end_pos = 0;
  std::string binlog_file_full_path;
  if (binlog_file.empty() && !index_records.empty()) {
    // find first binlog file
    binlog_file = binlog::CommonUtils::fill_binlog_file_name(index_records.front()->get_index());
    binlog_file_full_path = index_records.front()->get_file_name();
  }

  for (const auto& record : index_records) {
    if (std::equal(binlog_file.begin(),
            binlog_file.end(),
            binlog::CommonUtils::fill_binlog_file_name(record->get_index()).c_str())) {
      end_pos = record->get_position();
      binlog_file_full_path = record->get_file_name();
    };
  }
  logproxy::release_vector(index_records);

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

  bool error = false;
  FILE* fp = logproxy::FsUtil::fopen_binary(binlog_file_full_path);
  if (fp == nullptr) {
    return conn->send_err_packet(
        1220, "Error when executing command SHOW BINLOG EVENTS: Could not find target log", "HY000");
  }

  unsigned char magic[BINLOG_MAGIC_SIZE];
  // read magic number
  logproxy::FsUtil::read_file(fp, magic, 0, sizeof(magic));

  if (memcmp(magic, logproxy::binlog_magic, sizeof(magic)) != 0) {
    OMS_STREAM_ERROR << "The file format is invalid.";
    logproxy::FsUtil::fclose_binary(fp);
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND, "The file format is invalid.", "HY000");
  }

  std::vector<logproxy::ObLogEvent*> binlog_events;
  seek_events(binlog_file_full_path, binlog_events, logproxy::FORMAT_DESCRIPTION_EVENT, true);
  if (binlog_events.empty()) {
    OMS_ERROR("Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.");
    logproxy::FsUtil::fclose_binary(fp);
    release_vector(binlog_events);
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND,
        "Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.",
        "HY000");
  }

  auto* format_event = dynamic_cast<logproxy::FormatDescriptionEvent*>(binlog_events.at(0));
  uint8_t checksum_flag = format_event->get_checksum_flag();
  OMS_INFO("The current checksum of the binlog file is:{}", checksum_flag);
  release_vector(binlog_events);

  uint64_t index = 0;
  uint64_t count = 0;
  size_t pos = position;

  OMS_STREAM_INFO << "pos:" << pos << " end pos:" << end_pos;
  unsigned char ev_header_buff[COMMON_HEADER_LENGTH];
  while (pos < end_pos) {
    // read common header
    size_t ret = logproxy::FsUtil::read_file(fp, ev_header_buff, pos, sizeof(ev_header_buff));
    if (ret != OMS_OK) {
      error = true;
      break;
    }
    logproxy::OblogEventHeader header = logproxy::OblogEventHeader();
    header.deserialize(ev_header_buff);
    auto event = std::unique_ptr<unsigned char[]>(new unsigned char[header.get_event_length()]);
    ret = logproxy::FsUtil::read_file(fp, event.get(), pos, header.get_event_length());
    if (ret != OMS_OK) {
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
        case logproxy::QUERY_EVENT: {
          auto query_event = logproxy::QueryEvent("", "");
          query_event.set_checksum_flag(checksum_flag);
          query_event.deserialize(event.get());
          info = query_event.print_event_info();
          break;
        }
        case logproxy::ROTATE_EVENT: {
          auto rotate_event = logproxy::RotateEvent(0, "", 0, 0);
          rotate_event.set_checksum_flag(checksum_flag);
          rotate_event.deserialize(event.get());
          info = rotate_event.print_event_info();
          break;
        }
        case logproxy::FORMAT_DESCRIPTION_EVENT: {
          auto format_description_event = logproxy::FormatDescriptionEvent();
          format_description_event.deserialize(event.get());
          info = format_description_event.print_event_info();
          break;
        }
        case logproxy::XID_EVENT: {
          auto xid_event = logproxy::XidEvent();
          xid_event.set_checksum_flag(checksum_flag);
          xid_event.deserialize(event.get());
          info = xid_event.print_event_info();
          break;
        }
        case logproxy::TABLE_MAP_EVENT: {
          auto table_map_event = logproxy::TableMapEvent();
          table_map_event.set_checksum_flag(checksum_flag);
          table_map_event.deserialize(event.get());
          info = table_map_event.print_event_info();
          break;
        }
        case logproxy::WRITE_ROWS_EVENT: {
          auto write_rows_event = logproxy::WriteRowsEvent(0, 0);
          write_rows_event.set_checksum_flag(checksum_flag);
          write_rows_event.deserialize(event.get());
          info = write_rows_event.print_event_info();
          break;
        }
        case logproxy::UPDATE_ROWS_EVENT: {
          auto update_rows_event = logproxy::UpdateRowsEvent(0, 0);
          update_rows_event.set_checksum_flag(checksum_flag);
          update_rows_event.deserialize(event.get());
          info = update_rows_event.print_event_info();
          break;
        }
        case logproxy::DELETE_ROWS_EVENT: {
          auto delete_row_event = logproxy::DeleteRowsEvent(0, 0);
          delete_row_event.set_checksum_flag(checksum_flag);
          delete_row_event.deserialize(event.get());
          info = delete_row_event.print_event_info();
          break;
        }
        case logproxy::GTID_LOG_EVENT: {
          auto gtid_log_event = logproxy::GtidLogEvent();
          gtid_log_event.set_checksum_flag(checksum_flag);
          gtid_log_event.deserialize(event.get());
          info = gtid_log_event.print_event_info();
          break;
        }
        case logproxy::PREVIOUS_GTIDS_LOG_EVENT: {
          auto previous_gtids_log_event = logproxy::PreviousGtidsLogEvent();
          previous_gtids_log_event.set_checksum_flag(checksum_flag);
          previous_gtids_log_event.deserialize(event.get());
          info = previous_gtids_log_event.print_event_info();
          break;
        }
        default:
          OMS_STREAM_ERROR << "Unknown event type:" << header.get_type_code();
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
      if (conn->send_row() != IoResult::SUCCESS) {
        logproxy::FsUtil::fclose_binary(fp);
        return IoResult::FAIL;
      }
    }
    index++;
    pos = header.get_next_position();
  }

  logproxy::FsUtil::fclose_binary(fp);
  if (count < limit && error) {
    OMS_STREAM_ERROR << "Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.";
    return conn->send_err_packet(BINLOG_ERROR_WHEN_EXECUTING_COMMAND,
        "Error when executing command SHOW BINLOG EVENTS: Wrong offset or I/O error.",
        "HY000");
  }
  return conn->send_eof_packet();
}

static std::string get_executed_gtid_set(const logproxy::BinlogIndexRecord& index_record)
{
  std::stringstream executed_gtid_set_stream;
  std::pair<uint64_t, uint64_t> pair = {1, index_record.get_current_mapping().second};
  std::vector<logproxy::ObLogEvent*> log_events;
  logproxy::seek_events(index_record._file_name, log_events, logproxy::PREVIOUS_GTIDS_LOG_EVENT, true);
  if (!log_events.empty()) {
    auto* previous_gtids_log_event = dynamic_cast<logproxy::PreviousGtidsLogEvent*>(log_events.at(0));
    std::map<std::string, logproxy::GtidMessage*> gtids = previous_gtids_log_event->get_gtid_messages();
    int count = 0;
    for (auto gtid : gtids) {
      if (count != 0) {
        executed_gtid_set_stream << ",";
      }
      if (!gtid.second->get_txn_range().empty()) {
        if (pair.first > gtid.second->get_txn_range().back().second) {
          gtid.second->get_txn_range().emplace_back(pair);
        } else {
          gtid.second->get_txn_range().back().second = pair.second;
        }
      } else {
        gtid.second->get_txn_range().emplace_back(pair);
      }
      executed_gtid_set_stream << gtid.second->format_string();
      count++;
    }
  }
  logproxy::release_vector(log_events);
  std::string executed_gtid_set = executed_gtid_set_stream.str();
  OMS_STREAM_INFO << "executed_gtid_set:" << executed_gtid_set;
  return executed_gtid_set;
}

IoResult ShowMasterStatusProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  constexpr int max_column_len = 255;
  uint16_t utf8_cs = 33;

  ColumnPacket file_column_packet{
      "File", "", utf8_cs, max_column_len, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket position_column_packet{"Position",
      "",
      utf8_cs,
      8,
      ColumnType::ct_longlong,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket binlog_do_db_column_packet{
      "Binlog_Do_DB", "", utf8_cs, max_column_len, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};
  ColumnPacket binlog_ignore_db_column_packet{"Binlog_Ignore_DB",
      "",
      utf8_cs,
      max_column_len,
      ColumnType::ct_var_string,
      ColumnDefinitionFlags::binary_flag | ColumnDefinitionFlags::blob_flag,
      0};
  ColumnPacket executed_gtid_set_column_packet{"Executed_Gtid_Set",
      "",
      utf8_cs,
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

  logproxy::BinlogIndexRecord index_record;
  logproxy::get_index(conn->get_full_binlog_path() + BINLOG_DATA_DIR + BINLOG_INDEX_NAME,
      index_record,
      1,
      conn->get_full_binlog_path() + BINLOG_DATA_DIR);

  if (!index_record._file_name.empty()) {
    std::string file = CommonUtils::fill_binlog_file_name(index_record.get_index());
    uint64_t position = index_record.get_position();
    std::string binlog_do_db;
    std::string binlog_ignore_db;
    std::string executed_gtid_set;
    if (logproxy::Config::instance().binlog_gtid_display.val()) {
      executed_gtid_set = get_executed_gtid_set(index_record);
    }
    conn->start_row();
    conn->store_string(file);
    conn->store_uint64(position);
    conn->store_string(binlog_do_db);
    conn->store_string(binlog_ignore_db);
    conn->store_string(executed_gtid_set);
    if (conn->send_row() != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
  }
  return conn->send_eof_packet();
}

int purge_binlog_file(const std::vector<std::string>& file_paths)
{
  for (const std::string& purge_file : file_paths) {
    std::error_code error_code;
    if (std::filesystem::remove(purge_file, error_code) && !error_code) {
      OMS_STREAM_INFO << "Deleted " << purge_file << " binlog file \n";
    } else {
      OMS_STREAM_ERROR << "Failed to purge binlog file:[" << purge_file << "]";
    }
  }
  return OMS_OK;
}

IoResult PurgeBinaryLogsProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{

  auto* p_statement = (hsql::PurgeBinlogStatement*)statement;
  std::string binlog_file;
  std::string purge_ts;
  std::string cluster;
  std::string tenant;
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

  std::vector<std::string> purge_binlog_files;
  std::string error_msg;
  std::string base_path = conn->get_full_binlog_path() + BINLOG_DATA_DIR;
  int ret = logproxy::purge_binlog_index(base_path, binlog_file, purge_ts, error_msg, purge_binlog_files);
  if (ret != OMS_OK) {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, error_msg, "HY000");
  }
  g_bc_executor->submit(purge_binlog_file, purge_binlog_files);
  return conn->send_ok_packet();
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
  std::string cluster;
  std::string tenant;
  if (p_statement->tenant != nullptr) {
    cluster = p_statement->tenant->cluster;
    tenant = p_statement->tenant->tenant;
  }

  // row packet
  conn->start_row();
  conn->store_string(cluster);
  conn->store_string(tenant);
  conn->store_string(conn->get_local_ip());
  conn->store_uint64(conn->get_local_port());
  conn->store_string("OK");
  conn->store_null();
  if (conn->send_row() != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  return conn->send_eof_packet();
}

IoResult CreateBinlogProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::CreateBinlogStatement*)statement;
  logproxy::OblogConfig config{""};

  if (p_statement->tenant != nullptr) {
    config.cluster.set(p_statement->tenant->cluster);
    config.tenant.set(p_statement->tenant->tenant);
  } else {
    conn->send_err_packet(BINLOG_FATAL_ERROR, "cluster or tenant cannot be empty", "HY000");
    return IoResult::FAIL;
  }

  if (p_statement->ts != nullptr) {
    if (!p_statement->ts->get_value().empty()) {
      config.start_timestamp_us.set(std::atoll(p_statement->ts->get_value().c_str()));
    }
  }

  if (p_statement->user_info != nullptr) {
    config.sys_user.set(p_statement->user_info->user);
    config.sys_password.set(p_statement->user_info->password);
  }

  if (!p_statement->binlog_options->empty()) {
    for (const hsql::BinlogOption* binlog_option : *p_statement->binlog_options) {
      switch (binlog_option->option_type) {
        case hsql::CLUSTER_URL: {
          if (binlog_option->value == nullptr) {
            conn->send_err_packet(BINLOG_FATAL_ERROR, "cluster url cannot be empty", "HY000");
            return IoResult::FAIL;
          }
          config.cluster_url.set(binlog_option->value);
          break;
        }
        case hsql::SERVER_UUID: {
          config.server_uuid.set(binlog_option->value);
          break;
        }
        case hsql::INITIAL_TRX_XID: {
          config.initial_trx_xid.set(binlog_option->value);
          break;
        }
        case hsql::INITIAL_TRX_GTID_SEQ: {
          config.initial_trx_gtid_seq.set(std::atoll(binlog_option->value));
          break;
        }
        case hsql::INITIAL_TRX_SEEKING_ABORT_TIMESTAMP: {
          config.initial_trx_seeking_abort_timestamp.set(std::atoll(binlog_option->value));
          break;
        }
      }
    }
  }

  init_oblog_config(config);

  std::vector<StateMachine*> state_machines;
  binlog::g_state_machine->fetch_state_vector(get_default_state_file_path(), state_machines);

  bool is_existed = false;
  for (const StateMachine* state_machine : state_machines) {
    if (strcmp(config.cluster.val().c_str(), state_machine->get_cluster().c_str()) == 0 &&
        strcmp(config.tenant.val().c_str(), state_machine->get_tenant().c_str()) == 0) {

      OMS_INFO("Current tenant:{}.{} BC server status:{}",
          config.cluster.val(),
          config.cluster.val(),
          print(state_machine->get_converter_state()));

      if (state_machine->get_converter_state() == INIT || state_machine->get_converter_state() == RUNNING) {
        // Determine whether the process exists, and if it exists, it will not be pulled repeatedly
        if (state_machine->get_pid() > 0 && 0 == kill(state_machine->get_pid(), 0)) {
          OMS_STREAM_INFO << "The current binlog converter [" << state_machine->get_cluster() << ","
                          << state_machine->get_tenant() << "]"
                          << "is alive and the pull action is terminated";
          conn->send_ok_packet();
          logproxy::release_vector(state_machines);
          return IoResult::SUCCESS;
        }
        is_existed = true;
      }
    }
  }
  logproxy::release_vector(state_machines);
  StateMachine state_machine;
  state_machine.set_cluster(config.cluster.val());
  state_machine.set_tenant(config.tenant.val());
  state_machine.set_config(config.serialize_configs());
  if (is_existed) {
    binlog::g_state_machine->update_state(get_default_state_file_path(), state_machine);
  } else {
    binlog::g_state_machine->add_state(get_default_state_file_path(), state_machine);
  }
  OMS_STREAM_INFO << "Start Binlog Converter with config:" << config.debug_str();
  g_bc_executor->submit(logproxy::start_binlog_converter, config.serialize_configs());

  return conn->send_ok_packet();
}

void CreateBinlogProcessor::init_oblog_config(logproxy::OblogConfig& config)
{
  config.add("log_bin_prefix",
      logproxy::Config::instance().binlog_log_bin_basename.val() + "/" + config.cluster.val() + "/" +
          config.tenant.val());
  config.add("server_uuid", config.server_uuid.val());
  config.add("worker_path",
      logproxy::Config::instance().binlog_log_bin_basename.val() + "/" + config.cluster.val() + "/" +
          config.tenant.val());

  // Parameters required by obcdc
  config.add("enable_convert_timestamp_to_unix_timestamp", "1");
  config.add("enable_output_trans_order_by_sql_operation", "1");
  config.add("sort_trans_participants", "1");
  // 4.x enables to output row data in the order of column declaration
  config.add("enable_output_by_table_def", "1");
  config.add("memory_limit", logproxy::Config::instance().binlog_memory_limit.val());
  config.add("working_mode", logproxy::Config::instance().binlog_working_mode.val());

  if (logproxy::Config::instance().table_whitelist.val().empty()) {
    config.table_whites.set(config.tenant.val() + ".*.*");
  } else {
    config.table_whites.set(logproxy::Config::instance().table_whitelist.val());
  }

  if (!config.sys_user.empty()) {
    config.user.set(config.sys_user.val());
  } else {
    config.user.set(logproxy::Config::instance().ob_sys_username.val());
  }
  if (!config.sys_password.empty()) {
    config.password.set(config.sys_password.val());
  } else {
    config.password.set(logproxy::Config::instance().ob_sys_password.val());
  }
}

IoResult DropBinlogProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::DropBinlogStatement*)statement;
  bool if_exists = p_statement->if_exists;
  std::string cluster;
  std::string tenant;
  if (nullptr != p_statement->tenant_info) {
    cluster = p_statement->tenant_info->cluster;
    tenant = p_statement->tenant_info->tenant;
  } else {
    return conn->send_err_packet(BINLOG_FATAL_ERROR, "Cluster or tenant cannot be empty", "HY000");
  }

  std::vector<StateMachine*> state_machines;
  binlog::g_state_machine->fetch_state_vector(get_default_state_file_path(), state_machines);

  for (StateMachine* state_machine : state_machines) {
    if (std::equal(cluster.begin(), cluster.end(), state_machine->get_cluster().c_str()) &&
        std::equal(tenant.begin(), tenant.end(), state_machine->get_tenant().c_str())) {
      OMS_STREAM_INFO << "Current tenant:" << cluster << "." << tenant
                      << " BC server status:" << state_machine->get_converter_state();

      // Release the BC process, clean up the Binlog file and release related resources
      g_bc_executor->submit(logproxy::stop_binlog_converter, state_machine->to_string());
      logproxy::release_vector(state_machines);

      return conn->send_ok_packet();
    }
  }
  logproxy::release_vector(state_machines);
  if (if_exists) {
    return conn->send_ok_packet();
  }

  return conn->send_err_packet(BINLOG_FATAL_ERROR, "BC process does not exist", "HY000");

  return conn->send_ok_packet();
}

IoResult ShowBinlogStatusProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  uint16_t utf8_cs = 33;

  ColumnPacket cluster_column_packet{
      "cluster", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket tenant_column_packet{
      "tenant", "", utf8_cs, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket status_column_packet{
      "status", "", utf8_cs, 8, ColumnType::ct_var_string, ColumnDefinitionFlags::not_null_flag, 31};

  auto* p_statement = (hsql::ShowBinlogStatusStatement*)statement;

  bool for_tenant_exists = false;

  std::string cluster;
  std::string tenant;

  if (p_statement->tenant != nullptr) {
    cluster = p_statement->tenant->cluster;
    tenant = p_statement->tenant->tenant;
    for_tenant_exists = true;
  }

  std::string status;

  if (conn->send_result_metadata({
          cluster_column_packet,
          tenant_column_packet,
          status_column_packet,
      }) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  // Query tenant status and assemble indicator information
  if (for_tenant_exists) {
    // row packet
    conn->start_row();
    conn->store_string(cluster);
    conn->store_string(tenant);

    std::vector<StateMachine*> state_machines;
    binlog::g_state_machine->fetch_state_vector(get_default_state_file_path(), state_machines);
    for (StateMachine* state_machine : state_machines) {
      if (std::equal(state_machine->get_cluster().begin(), state_machine->get_cluster().end(), cluster.c_str()) &&
          std::equal(state_machine->get_tenant().begin(), state_machine->get_tenant().end(), tenant.c_str())) {
        serialize_binlog_metrics(status, state_machine);
      }
    }
    logproxy::release_vector(state_machines);

    conn->store_string(status);
    if (conn->send_row() != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
  } else {
    std::vector<StateMachine*> state_machines;
    binlog::g_state_machine->fetch_state_vector(get_default_state_file_path(), state_machines);
    for (StateMachine* state_machine : state_machines) {
      // row packet
      conn->start_row();
      conn->store_string(state_machine->get_cluster());
      conn->store_string(state_machine->get_tenant());

      serialize_binlog_metrics(status, state_machine);
      conn->store_string(status);
      if (conn->send_row() != IoResult::SUCCESS) {
        logproxy::release_vector(state_machines);
        return IoResult::FAIL;
      }
    }
    logproxy::release_vector(state_machines);
  }
  return conn->send_eof_packet();
}

void ShowBinlogStatusProcessor::serialize_binlog_metrics(string& status, const StateMachine* state_machine)
{
  logproxy::ProcessMetric process_metric{};
  if (logproxy::g_proc_metric.metric_group.find(state_machine->get_work_path()) !=
      logproxy::g_proc_metric.metric_group.end()) {
    process_metric = *(logproxy::g_proc_metric.metric_group.find(state_machine->get_work_path())->second);
  }
  Json::Value binlog_json = process_metric.serialize_to_json_value();

  Json::Value binlog_metrics;
  string index_file = state_machine->get_work_path() + BINLOG_DATA_DIR + BINLOG_INDEX_NAME;

  vector<logproxy::BinlogIndexRecord*> index_records;
  fetch_index_vector(index_file, index_records);
  int metric_idx = 0;
  for (auto* index : index_records) {
    Json::Value item;
    item["binlog_name"] = CommonUtils::fill_binlog_file_name(index->get_index());
    item["binlog_size"] = index->get_position();
    binlog_metrics[metric_idx] = item;
    metric_idx++;
  }
  logproxy::release_vector(index_records);
  binlog_json["binlog_files"] = binlog_metrics;
  Json::StreamWriterBuilder builder;
  status = Json::writeString(builder, binlog_json);
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
          OMS_INFO("set session var {}={}", std::string(var->column), var->value->get_value());
          conn->set_session_var(var->column, var->value->get_value());
        }
        break;
      }
      case hsql::Global: {
        if (var->column != nullptr && var->value != nullptr) {
          /**
           * TODO Temporarily do not process global system variables
           */

          OMS_INFO("set global var {}={}", std::string(var->column), var->value->get_value());
        }
        break;
      }
      case hsql::Local: {
        if (var->column != nullptr && var->value != nullptr) {
          /**
           * TODO Temporarily do not process local variables
           */

          OMS_INFO("set local var {}={}", std::string(var->column), var->value->get_value());
        }
        break;
      }
    }
  }
  conn->send_ok_packet();
  return IoResult::SUCCESS;
}

IoResult ShowVarProcessor::process(Connection* conn, const hsql::SQLStatement* statement)
{
  uint16_t utf8_cs = 33;

  ColumnPacket name_column_packet{
      "Variable_name", "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
  ColumnPacket value_column_packet{
      "Value", "", utf8_cs, 36, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};

  if (conn->send_result_metadata({
          name_column_packet,
          value_column_packet,
      }) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  auto* p_statement = (hsql::ShowStatement*)statement;
  switch (p_statement->type) {
    case hsql::kShowColumns:
      break;
    case hsql::kShowTables:
      break;
    case hsql::kShowVar: {
      if (p_statement->_var_name != nullptr) {
        return show_specified_var(conn, p_statement);
      } else {
        return show_non_specified_var(conn, p_statement);
      }
    }
  }
  return conn->send_eof_packet();
}

IoResult ShowVarProcessor::show_non_specified_var(Connection* conn, const hsql::ShowStatement* p_statement)
{
  switch (p_statement->var_type) {
    case hsql::Global: {
      for (auto var : g_sys_var->get_all_global_var()) {
        // row packet
        conn->start_row();
        conn->store_string(var.first);
        conn->store_string(var.second);
        if (conn->send_row() != IoResult::SUCCESS) {
          return IoResult::FAIL;
        }
      }
      return conn->send_eof_packet();
    }
    case hsql::Local:
      break;
    case hsql::Session: {
      auto vars = conn->get_session_var();
      for (auto var : vars) {
        // row packet
        conn->start_row();
        conn->store_string(var.first);
        conn->store_string(var.second);
        if (conn->send_row() != IoResult::SUCCESS) {
          return IoResult::FAIL;
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
  if (conn->send_row() != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }
  return conn->send_eof_packet();
}

int ShowVarProcessor::query_var(
    Connection* conn, std::string& var_name, std::string& value, hsql::VarLevel type, std::string& err_msg)
{
  switch (type) {
    case hsql::Global: {
      if (g_sys_var->get_global_var(var_name, value) != OMS_OK) {
        err_msg = "Unknown system variable " + var_name;
        return OMS_FAILED;
      }
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
  uint16_t utf8_cs = 33;
  ColumnPacket var_column_packet{
      var_column, "", utf8_cs, 56, ColumnType::ct_var_string, ColumnDefinitionFlags::pri_key_flag, 31};
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
  if (conn->send_row() != IoResult::SUCCESS) {
    return IoResult::FAIL;
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
  auto p_func_processor = logproxy::func_processor(var_name);
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

}  // namespace binlog
}  // namespace oceanbase