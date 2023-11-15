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

namespace oceanbase {
namespace binlog {
enum class SqlCmd {
  // mysql
  show_binary_logs,
  show_binlog_events,
  show_master_status,
  purge_binary_logs,
  set_var,

  // ob binlog server
  show_binlog_server,
  create_binlog,
  drop_binlog,
  show_binlog_status,


  /* This should be the last !!! */
  end,
};

}  // namespace binlog
}  // namespace oceanbase