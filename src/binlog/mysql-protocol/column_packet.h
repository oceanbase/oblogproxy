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

#include "data_packet.h"
#include "column_type.h"
#include "column_definition_flags.h"

#include <string>

namespace oceanbase {
namespace binlog {
// see
// https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_com_query_response_text_resultset_column_definition.html

// This class is an implementation of Protocol::ColumnDefinition41
class ColumnPacket : public DataPacket {
public:
  ColumnPacket(std::string name, std::string origin_name, uint16_t character_set, uint32_t length, ColumnType type,
      ColumnDefinitionFlags flags, uint8_t decimals);

  ColumnPacket(std::string schema, std::string table, std::string origin_table, std::string name,
      std::string origin_name, uint16_t character_set, uint32_t length, ColumnType type, ColumnDefinitionFlags flags,
      uint8_t decimals);

  ~ColumnPacket() override = default;

  const std::string& get_catalog() const;

  const std::string& get_schema() const;

  const std::string& get_table() const;

  const std::string& get_origin_table() const;

  const std::string& get_name() const;

  const std::string& get_origin_name() const;

  uint16_t get_character_set() const;

  uint32_t get_length() const;

  ColumnType get_type() const;

  ColumnDefinitionFlags get_flags() const;

  uint8_t get_decimals() const;

  ssize_t serialize(PacketBuf& packet_buf, Capability client_capabilities) const override;

private:
  static constexpr uint8_t length_of_fixed_length_fields = 0x0C;

private:
  const std::string catalog_ = "def";
  const std::string schema_;        // schema(i.e database) name
  const std::string table_;         // aliased table name
  const std::string origin_table_;  // physical table name
  const std::string name_;          // aliased column name
  const std::string origin_name_;   // physical column name
  const uint16_t character_set_;    // the column character set
  const uint32_t length_;           // maximum length of the field
  const ColumnType type_;
  const ColumnDefinitionFlags flags_;
  const uint8_t decimals_;  // max shown decimal digits
};

}  // namespace binlog
}  // namespace oceanbase

#include "column_packet.inl"
