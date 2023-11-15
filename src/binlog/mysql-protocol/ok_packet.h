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
#include "server_status.h"

#include <string>

namespace oceanbase {
namespace binlog {
// see https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basic_ok_packet.html
class OkPacket : public DataPacket {
public:
  OkPacket(
      uint64_t affected_rows, uint64_t last_insert_id, ServerStatus status_flags, uint16_t warnings, std::string info);

  ~OkPacket() override = default;

  uint64_t get_affected_rows() const;

  uint64_t get_last_insert_id() const;

  ServerStatus get_status_flags() const;

  uint16_t get_warnings() const;

  const std::string& get_info() const;

  ssize_t serialize(PacketBuf& packet_buf, Capability client_capabilities) const override;

private:
  static constexpr uint8_t header = 0x00;

private:
  const uint64_t affected_rows_;
  const uint64_t last_insert_id_;
  const ServerStatus status_flags_;
  const uint16_t warnings_;
  const std::string info_;
  const std::string session_state_info_;  // TODO: not fully supported yet
};

}  // namespace binlog
}  // namespace oceanbase

#include "ok_packet.inl"