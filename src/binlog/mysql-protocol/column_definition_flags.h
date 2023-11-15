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
// see https://dev.mysql.com/doc/dev/mysql-server/latest/group__group__cs__column__definition__flags.html
enum class ColumnDefinitionFlags : uint32_t {
  not_null_flag = 1U << 0,             // Field can't be NULL
  pri_key_flag = 1u << 1,              // Field is part of a primary key
  unique_key_flag = 1u << 2,           // Field is part of a unique key
  multiple_key_flag = 1u << 3,         // Field is part of a key
  blob_flag = 1u << 4,                 // Field is a blob
  unsigned_flag = 1u << 5,             // Field is unsigned
  zerofill_flag = 1u << 6,             // Field is zerofill
  binary_flag = 1u << 7,               // Field is binary
  enum_flag = 1u << 8,                 // field is an enum
  auto_increment_flag = 1u << 9,       // field is an autoincrement field
  timestamp_flag = 1u << 10,           // Field is a timestamp
  set_flag = 1u << 11,                 // field is a set
  no_default_value_flag = 1u << 12,    // Field doesn't have default value
  on_update_now_flag = 1u << 13,       // Field is set to NOW on UPDATE
  part_key_flag = 1u << 14,            // Intern; Part of some key
  num_flag = 1u << 15,                 // Field is num (for clients)
  group_flag = 1u << 15,               // Intern: Group field
  unique_flag = 1u << 16,              // Intern: Used by sql_yacc
  bincmp_flag = 1u << 17,              // Intern: Used by sql_yacc
  get_fixed_fields_flag = 1u << 18,    // Used to get fields in item tree
  field_in_part_func_flag = 1u << 19,  // Field part of partition func
  field_in_add_index = 1u << 20,       // Intern: Field in TABLE object for new version of altered table
  field_is_renamed = 1u << 21,         // Intern: Field is being renamed
  field_storage_media_low = 1u << 22,
  field_storage_media_high = 1u << 23,
  field_column_format_low = 1u << 24,
  field_column_format_high = 1u << 25,
  field_is_dropped = 1u << 26,    // Intern: Field is being dropped
  explicit_null_flag = 1u << 27,  // Field is explicitly specified as NULL by the user
  not_secondary_flag = 1u << 29,
  field_is_invisible = 1u << 30,
};

inline constexpr ColumnDefinitionFlags operator&(ColumnDefinitionFlags x, ColumnDefinitionFlags y)
{
  using CapT = std::underlying_type_t<ColumnDefinitionFlags>;
  return static_cast<ColumnDefinitionFlags>(static_cast<CapT>(x) & static_cast<CapT>(y));
}

inline constexpr ColumnDefinitionFlags operator|(ColumnDefinitionFlags x, ColumnDefinitionFlags y)
{
  using CapT = std::underlying_type_t<ColumnDefinitionFlags>;
  return static_cast<ColumnDefinitionFlags>(static_cast<CapT>(x) | static_cast<CapT>(y));
}

inline constexpr ColumnDefinitionFlags operator^(ColumnDefinitionFlags x, ColumnDefinitionFlags y)
{
  using CapT = std::underlying_type_t<ColumnDefinitionFlags>;
  return static_cast<ColumnDefinitionFlags>(static_cast<CapT>(x) ^ static_cast<CapT>(y));
}

inline constexpr ColumnDefinitionFlags operator~(ColumnDefinitionFlags x)
{
  using CapT = std::underlying_type_t<ColumnDefinitionFlags>;
  return static_cast<ColumnDefinitionFlags>(~static_cast<CapT>(x));
}

}  // namespace binlog
}  // namespace oceanbase
