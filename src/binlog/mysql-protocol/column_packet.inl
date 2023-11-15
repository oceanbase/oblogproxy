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

#include <utility>

namespace oceanbase {
namespace binlog {

inline ColumnPacket::ColumnPacket(std::string schema, std::string table, std::string origin_table, std::string name,
    std::string origin_name, uint16_t character_set, uint32_t length, ColumnType type, ColumnDefinitionFlags flags,
    uint8_t decimals)
    : schema_(std::move(schema)),
      table_(std::move(table)),
      origin_table_(std::move(origin_table)),
      name_(std::move(name)),
      origin_name_(std::move(origin_name)),
      character_set_(character_set),
      length_(length),
      type_(type),
      flags_(flags),
      decimals_(decimals)
{}

inline ColumnPacket::ColumnPacket(std::string name, std::string origin_name, uint16_t character_set, uint32_t length,
    ColumnType type, ColumnDefinitionFlags flags, uint8_t decimals)
    : ColumnPacket("", "", "", std::move(name), std::move(origin_name), character_set, length, type, flags, decimals)
{}

inline const std::string& ColumnPacket::get_catalog() const
{
  return catalog_;
}

inline const std::string& ColumnPacket::get_schema() const
{
  return schema_;
}

inline const std::string& ColumnPacket::get_table() const
{
  return table_;
}

inline const std::string& ColumnPacket::get_origin_table() const
{
  return origin_table_;
}

inline const std::string& ColumnPacket::get_name() const
{
  return name_;
}

inline const std::string& ColumnPacket::get_origin_name() const
{
  return origin_name_;
}

inline uint16_t ColumnPacket::get_character_set() const
{
  return character_set_;
}

inline uint32_t ColumnPacket::get_length() const
{
  return length_;
}

inline ColumnType ColumnPacket::get_type() const
{
  return type_;
}

inline ColumnDefinitionFlags ColumnPacket::get_flags() const
{
  return flags_;
}

inline uint8_t ColumnPacket::get_decimals() const
{
  return decimals_;
}

inline ssize_t ColumnPacket::serialize(PacketBuf& packet_buf, Capability client_capabilities) const
{
  ssize_t sz = 0;
  //  ssize_t total_sz = 0;

  sz += packet_buf.write_string(catalog_, true);
  sz += packet_buf.write_string(schema_, true);
  sz += packet_buf.write_string(table_, true);
  sz += packet_buf.write_string(origin_table_, true);
  sz += packet_buf.write_string(name_, true);
  sz += packet_buf.write_string(origin_name_, true);
  sz += packet_buf.write_uint(length_of_fixed_length_fields);
  sz += packet_buf.write_uint2(character_set_);
  sz += packet_buf.write_uint4(length_);
  sz += packet_buf.write_uint1(static_cast<uint8_t>(type_));
  sz += packet_buf.write_uint2(static_cast<uint16_t>(flags_));
  sz += packet_buf.write_uint1(decimals_);
  sz += packet_buf.write_uint2(0);  // Reserved for the future

  return sz;
}

}  // namespace binlog
}  // namespace oceanbase