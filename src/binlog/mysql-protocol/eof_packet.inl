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
#include "common.h"

namespace oceanbase {
namespace binlog {

inline EofPacket::EofPacket(uint16_t warnings, ServerStatus status_flags)
    : warnings_(warnings), status_flags_(status_flags)
{}

inline uint16_t EofPacket::get_warnings() const
{
  return warnings_;
}

inline ServerStatus EofPacket::get_status_flags() const
{
  return status_flags_;
}

inline ssize_t EofPacket::serialize(PacketBuf& packet_buf, oceanbase::binlog::Capability client_capabilities) const
{
  bool client_protocol_41_enabled = has_capability(client_capabilities, Capability::client_protocol_41);
  size_t serialized_sz = 1 + (client_protocol_41_enabled ? (2 + 2) : 0);
  assert(packet_buf.is_fast_writable(serialized_sz));
  OMS_UNUSED(serialized_sz);
  uint32_t sz = 0;

  sz += packet_buf.write_uint1(header);
  if (client_protocol_41_enabled) {
    sz += packet_buf.write_uint2(warnings_);
    sz += packet_buf.write_uint2(static_cast<uint16_t>(status_flags_));
  }

  assert(serialized_sz == sz);
  return sz;
}

}  // namespace binlog
}  // namespace oceanbase