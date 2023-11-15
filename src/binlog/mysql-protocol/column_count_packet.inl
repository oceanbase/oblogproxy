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

namespace oceanbase {
namespace binlog {

inline ColumnCountPacket::ColumnCountPacket(uint16_t nof_columns) : nof_columns_(nof_columns)
{}

inline uint16_t ColumnCountPacket::get_nof_columns() const
{
  return nof_columns_;
}

inline ssize_t ColumnCountPacket::serialize(PacketBuf& packet_buf, Capability client_capabilities) const
{
  return packet_buf.write_uint(nof_columns_);
}

}  // namespace binlog
}  // namespace oceanbase