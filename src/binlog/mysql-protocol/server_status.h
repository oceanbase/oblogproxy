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
#include <type_traits>

namespace oceanbase {
namespace binlog {
enum class ServerStatus : uint16_t {
  in_trans = 1 << 0,
  autocommit = 1 << 1,
  more_results_exists = 1 << 3,
  query_no_good_index_used = 1 << 4,
  query_no_index_used = 1 << 5,
  cursor_exists = 1 << 6,
  last_row_sent = 1 << 7,
  db_dropped = 1 << 8,
  no_backslash_escapes = 1 << 9,
  metadata_changed = 1 << 10,
  query_was_slow = 1 << 11,
  ps_out_params = 1 << 12,
  in_trans_readonly = 1 << 13,
  session_state_changed = 1 << 14,
};

inline constexpr ServerStatus operator&(ServerStatus x, ServerStatus y)
{
  using SST = std::underlying_type_t<ServerStatus>;
  return static_cast<ServerStatus>(static_cast<SST>(x) & static_cast<SST>(y));
}

inline constexpr ServerStatus operator|(ServerStatus x, ServerStatus y)
{
  using SST = std::underlying_type_t<ServerStatus>;
  return static_cast<ServerStatus>(static_cast<SST>(x) | static_cast<SST>(y));
}

inline constexpr ServerStatus operator^(ServerStatus x, ServerStatus y)
{
  using SST = std::underlying_type_t<ServerStatus>;
  return static_cast<ServerStatus>(static_cast<SST>(x) ^ static_cast<SST>(y));
}

inline constexpr ServerStatus operator~(ServerStatus x)
{
  using SST = std::underlying_type_t<ServerStatus>;
  return static_cast<ServerStatus>(~static_cast<SST>(x));
}

inline constexpr bool has_status_flag(ServerStatus status_flags, ServerStatus expected_status)
{
  using CapT = std::underlying_type_t<ServerStatus>;
  return (static_cast<CapT>(status_flags) & static_cast<CapT>(expected_status)) != 0;
}

constexpr ServerStatus ob_binlog_server_status_flags = ServerStatus::autocommit;

}  // namespace binlog
}  // namespace oceanbase
