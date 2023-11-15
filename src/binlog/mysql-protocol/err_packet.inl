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

#include <cassert>
#include <cstring>
#include "common.h"

namespace oceanbase {
namespace binlog {

inline ErrPacket::ErrPacket(uint16_t err_code, std::string err_msg, const std::string& sql_state)
    : err_code_(err_code), err_msg_(std::move(err_msg)), sql_state_{}
{
  assert(err_msg_.length() <= max_err_msg_sz);
  assert(sql_state.length() == sql_state_length);
  memcpy(sql_state_, sql_state.c_str(), sql_state_length);
  sql_state_[sql_state_length] = 0;
}

inline uint16_t ErrPacket::get_err_code() const
{
  return err_code_;
}

inline const std::string& ErrPacket::get_err_msg() const
{
  return err_msg_;
}

inline const char* ErrPacket::get_sql_state() const
{
  return sql_state_;
}

inline ssize_t ErrPacket::serialize(PacketBuf& packet_buf, Capability client_capabilities) const
{
  bool client_protocol_41_enabled = has_capability(client_capabilities, Capability::client_protocol_41);
  size_t serialized_sz = 1 + 2 + (client_protocol_41_enabled ? (1 + sql_state_length) : 0) + err_msg_.length();
  assert(packet_buf.is_fast_writable(serialized_sz));
  OMS_UNUSED(serialized_sz);
  uint32_t sz = 0;

  sz += packet_buf.write_uint1(header);
  sz += packet_buf.write_uint2(err_code_);
  if (client_protocol_41_enabled) {
    sz += packet_buf.write_uint1(sql_state_marker);
    sz += packet_buf.write_string(sql_state_, sql_state_length);
  }
  sz += packet_buf.write_string(err_msg_.data(), err_msg_.length());

  assert(serialized_sz == sz);
  return sz;
}

}  // namespace binlog
}  // namespace oceanbase