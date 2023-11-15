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

namespace oceanbase {
namespace binlog {
class HandshakePacket : public DataPacket {
public:
  HandshakePacket(uint32_t thread_id, uint8_t character_set_nr);

  ~HandshakePacket() override = default;

  uint32_t get_thread_id() const;

  uint8_t get_character_set_nr() const;

  ServerStatus get_server_status_flags() const;

  const char* get_scramble() const;

  ssize_t serialize(PacketBuf& packet_buf, Capability client_capabilities) const override;

private:
  void generate_scramble();

private:
  static constexpr uint8_t scramble_part_1_length = 8;
  static constexpr uint8_t scramble_part_2_length = 12;
  static constexpr uint8_t scramble_length = scramble_part_1_length + scramble_part_2_length;
  static constexpr uint8_t mysql_protocol_version = 10;
  static constexpr uint8_t reserved_length = 10;
  static constexpr const char reserved[reserved_length] = {0};
  static constexpr const char* mysql_server_version = "5.7.35-obs";
  static constexpr const char* auth_plugin_name = "mysql_native_password";

private:
  const uint32_t thread_id_;
  const uint8_t character_set_nr_;
  const ServerStatus server_status_flags_;
  char scramble_[scramble_length + 1]{};  // NULL terminated
};

}  // namespace binlog
}  // namespace oceanbase

#include "handshake_packet.inl"
