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

namespace oceanbase {
namespace binlog {
class ColumnCountPacket : public DataPacket {
public:
  explicit ColumnCountPacket(uint16_t nof_columns);

  ~ColumnCountPacket() override = default;

  uint16_t get_nof_columns() const;

  ssize_t serialize(PacketBuf& packet_buf, Capability client_capabilities) const override;

private:
  uint16_t nof_columns_;
};

}  // namespace binlog
}  // namespace oceanbase

#include "column_count_packet.inl"
