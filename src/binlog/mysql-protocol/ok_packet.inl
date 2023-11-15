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

namespace oceanbase {
namespace binlog {

inline OkPacket::OkPacket(
    uint64_t affected_rows, uint64_t last_insert_id, ServerStatus status_flags, uint16_t warnings, std::string info)
    : affected_rows_(affected_rows),
      last_insert_id_(last_insert_id),
      status_flags_(status_flags),
      warnings_(warnings),
      info_(std::move(info))
{}

inline uint64_t OkPacket::get_affected_rows() const
{
  return affected_rows_;
}

inline uint64_t OkPacket::get_last_insert_id() const
{
  return last_insert_id_;
}

inline ServerStatus OkPacket::get_status_flags() const
{
  return status_flags_;
}

inline uint16_t OkPacket::get_warnings() const
{
  return warnings_;
}

inline const std::string& OkPacket::get_info() const
{
  return info_;
}

inline ssize_t OkPacket::serialize(PacketBuf& packet_buf, Capability client_capabilities) const
{
  uint32_t sz = 0;

  sz += packet_buf.write_uint1(header);
  sz += packet_buf.write_uint(affected_rows_);
  sz += packet_buf.write_uint(last_insert_id_);
  if (has_capability(client_capabilities, Capability::client_protocol_41)) {
    sz += packet_buf.write_uint2(static_cast<uint16_t>(status_flags_));
    sz += packet_buf.write_uint2(warnings_);
  } else if (has_capability(client_capabilities, Capability::client_transactions)) {
    sz += packet_buf.write_uint2(static_cast<uint16_t>(status_flags_));
  }

  if (has_capability(client_capabilities, Capability::client_session_track)) {
    if (has_status_flag(status_flags_, ServerStatus::session_state_changed) || info_.length() > 0) {
      sz += packet_buf.write_string(info_.data(), info_.length(), true);
    }
    if (has_status_flag(status_flags_, ServerStatus::session_state_changed)) {
      assert(false);  // TODO: write session_state_info_, haven't supported yet
    }
  } else {
    if (info_.length() > 0) {
      sz += packet_buf.write_string(info_.data(), info_.length(), true);
    }
  }

  return sz;
}

}  // namespace binlog
}  // namespace oceanbase