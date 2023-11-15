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
// see https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basic_eof_packet.html
class EofPacket : public DataPacket {
public:
  EofPacket(uint16_t warnings, ServerStatus status_flags);

  ~EofPacket() override = default;

  uint16_t get_warnings() const;

  ServerStatus get_status_flags() const;

  ssize_t serialize(PacketBuf& packet_buf, Capability client_capabilities) const override;

private:
  static constexpr uint8_t header = 0xFE;

private:
  const uint16_t warnings_;
  const ServerStatus status_flags_;
};

}  // namespace binlog
}  // namespace oceanbase

#include "eof_packet.inl"
