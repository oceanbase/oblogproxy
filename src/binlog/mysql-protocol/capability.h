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
// see https://dev.mysql.com/doc/dev/mysql-server/latest/group__group__cs__capabilities__flags.html#details
enum class Capability : uint32_t {
  client_long_password = 1u << 0,
  client_found_rows = 1u << 1,
  client_long_flag = 1u << 2,
  client_connect_with_db = 1u << 3,
  client_no_schema = 1u << 4,
  client_compress = 1u << 5,
  client_odbc = 1u << 6,
  client_local_files = 1u << 7,
  client_ignore_space = 1u << 8,
  client_protocol_41 = 1u << 9,
  client_interactive = 1u << 10,
  client_ssl = 1u << 11,
  client_ignore_sigpipe = 1u << 12,
  client_transactions = 1u << 13,
  client_reserved = 1u << 14,
  client_reserved2 = 1u << 15,
  client_secure_connection = 1u << 15,
  client_multi_statements = 1u << 16,
  client_multi_results = 1u << 17,
  client_ps_multi_results = 1u << 18,
  client_plugin_auth = 1u << 19,
  client_connect_attrs = 1u << 20,
  client_plugin_auth_lenenc_client_data = 1u << 21,
  client_can_handle_expired_passwords = 1u << 22,
  client_session_track = 1u << 23,
  client_deprecate_eof = 1u << 24,
  client_optional_resultset_metadata = 1u << 25,
  client_zstd_compression_algorithm = 1u << 26,
  client_query_attributes = 1u << 27,
  multi_factor_authentication = 1u << 28,
  client_capability_extension = 1u << 29,
  client_ssl_verify_server_cert = 1u << 30,
  client_remember_options = 1u << 31,
};

inline constexpr Capability operator&(Capability x, Capability y)
{
  using CapT = std::underlying_type_t<Capability>;
  return static_cast<Capability>(static_cast<CapT>(x) & static_cast<CapT>(y));
}

inline constexpr Capability operator|(Capability x, Capability y)
{
  using CapT = std::underlying_type_t<Capability>;
  return static_cast<Capability>(static_cast<CapT>(x) | static_cast<CapT>(y));
}

inline constexpr Capability operator^(Capability x, Capability y)
{
  using CapT = std::underlying_type_t<Capability>;
  return static_cast<Capability>(static_cast<CapT>(x) ^ static_cast<CapT>(y));
}

inline constexpr Capability operator~(Capability x)
{
  using CapT = std::underlying_type_t<Capability>;
  return static_cast<Capability>(~static_cast<CapT>(x));
}

inline constexpr bool has_capability(Capability capabilities, Capability expected_capability)
{
  using CapT = std::underlying_type_t<Capability>;
  return (static_cast<CapT>(capabilities) & static_cast<CapT>(expected_capability)) != 0;
}

constexpr Capability ob_binlog_server_capabilities =
    Capability::client_long_password | Capability::client_no_schema | Capability::client_ignore_space |
    Capability::client_protocol_41 | Capability::client_interactive | Capability::client_reserved |
    Capability::client_secure_connection | Capability::client_plugin_auth;

}  // namespace binlog
}  // namespace oceanbase
