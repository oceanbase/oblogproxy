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
enum class ColumnType : uint8_t {
  ct_decimal,
  ct_tiny,
  ct_short,
  ct_long,
  ct_float,
  ct_double,
  ct_null,
  ct_timestamp,
  ct_longlong,
  ct_int24,
  ct_date,
  ct_time,
  ct_datetime,
  ct_year,
  ct_newdate,  // internal to mysql. not used in protocol
  ct_varchar,
  ct_bit,
  ct_timestamp2,
  ct_datetime2,    // internal to mysql. not used in protocol
  ct_time2,        // internal to mysql. not used in protocol
  ct_typed_array,  // internal to mysql. not used in protocol
  ct_invalid = 243,
  ct_bool = 244,  // currently just a placeholder
  ct_json = 245,
  ct_newdecimal = 246,
  ct_enum = 247,
  ct_set = 248,
  ct_tiny_blob = 249,
  ct_medium_blob = 250,
  ct_long_blob = 251,
  ct_blob = 252,
  ct_var_string = 253,
  ct_string = 254,
  ct_geometry = 255
};

}  // namespace binlog
}  // namespace oceanbase
