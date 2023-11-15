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

#include <string>

namespace oceanbase {
namespace binlog {
// see https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basic_err_packet.html
class ErrPacket : public DataPacket {
public:
  ErrPacket(uint16_t err_code, std::string err_msg, const std::string& sql_state);

  ~ErrPacket() override = default;

  uint16_t get_err_code() const;

  const std::string& get_err_msg() const;

  const char* get_sql_state() const;

  ssize_t serialize(PacketBuf& packet_buf, Capability client_capabilities) const override;

private:
  static constexpr size_t max_err_msg_sz = 512;
  static constexpr size_t sql_state_length = 5;
  static constexpr uint8_t sql_state_marker = '#';
  static constexpr uint8_t header = 0xFF;

private:
  const uint16_t err_code_;
  const std::string err_msg_;             // cannot exceed max_err_msg_sz
  char sql_state_[sql_state_length + 1];  // fixed length, terminated by '0'
};

}  // namespace binlog
}  // namespace oceanbase

#include "err_packet.inl"