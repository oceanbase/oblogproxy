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

#include <cstdint>

namespace oceanbase {
namespace binlog {
enum class ServerCommand : uint8_t {
  sleep,
  quit,
  init_db,
  query,
  field_list,
  create_db,
  drop_db,
  refresh,
  shutdown,
  statistics,
  process_info,
  connect,
  process_kill,
  debug,
  ping,
  time,
  delayed_insert,
  change_user,
  binlog_dump,
  table_dump,
  connect_out,
  register_slave,
  stmt_prepare,
  stmt_execute,
  stmt_send_long_data,
  stmt_close,
  stmt_reset,
  set_option,
  stmt_fetch,
  daemon,
  binlog_dump_gtid,
  reset_connection,

  /* Must be last */
  end,
};

inline const char* server_command_names(ServerCommand server_command)
{
  static const char* _s_server_command_names[] = {
      "Sleep",
      "Quit",
      "Init DB",
      "Query",
      "Field List",
      "Create DB",
      "Drop DB",
      "Refresh",
      "Shutdown",
      "Statistics",
      "Processlist",
      "Connect",
      "Kill",
      "Debug",
      "Ping",
      "Time",
      "Delayed insert",
      "Change user",
      "Binlog Dump",
      "Table Dump",
      "Connect Out",
      "Register Slave",
      "Prepare",
      "Execute",
      "Long Data",
      "Close stmt",
      "Reset stmt",
      "Set option",
      "Fetch",
      "Daemon",
      "Binlog Dump GTID",
      "Reset Connection",

      "Error"  // ServerCommand::end
  };
  if (static_cast<uint8_t>(server_command) >= static_cast<uint8_t>(ServerCommand::sleep) &&
      static_cast<uint8_t>(server_command) <= static_cast<uint8_t>(ServerCommand::reset_connection)) {
    return _s_server_command_names[static_cast<size_t>(server_command)];
  }
  return _s_server_command_names[static_cast<size_t>(ServerCommand::end)];
};

}  // namespace binlog
}  // namespace oceanbase
