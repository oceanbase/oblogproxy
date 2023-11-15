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

#include <cstdlib>
#include <chrono>

namespace oceanbase {
namespace binlog {

inline HandshakePacket::HandshakePacket(uint32_t thread_id, uint8_t character_set_nr)
    : thread_id_(thread_id), character_set_nr_(character_set_nr), server_status_flags_(ob_binlog_server_status_flags)
{
  generate_scramble();
}

inline void HandshakePacket::generate_scramble()
{
  using std::chrono::system_clock;

  unsigned int seed = static_cast<unsigned int>(system_clock::to_time_t(system_clock::now()));
  for (int i = 0; i < scramble_length; ++i) {
    scramble_[i] = static_cast<char>(rand_r(&seed) & 0x7F);  // random ASCII character
    if (scramble_[i] == '\0' || scramble_[i] == '$') {
      scramble_[i] += 1;
    }
  }
  scramble_[scramble_length] = '\0';
}

inline ssize_t HandshakePacket::serialize(PacketBuf& packet_buf, Capability client_capabilities) const
{
  ssize_t sz = 0;
  sz += packet_buf.write_uint1(mysql_protocol_version);
  sz += packet_buf.write_string(mysql_server_version);
  sz += packet_buf.write_uint4(thread_id_);
  sz += packet_buf.write_string(scramble_, scramble_part_1_length, false);
  sz += packet_buf.write_uint1(0);  // terminate scramble part 1 by NULL
  sz += packet_buf.write_uint2(static_cast<uint16_t>(client_capabilities));
  sz += packet_buf.write_uint1(character_set_nr_);
  sz += packet_buf.write_uint2(static_cast<uint16_t>(server_status_flags_));
  sz += packet_buf.write_uint2((static_cast<uint32_t>(client_capabilities)) >> 16);
  sz += packet_buf.write_uint1(scramble_length + 1);
  sz += packet_buf.write_string(reserved, reserved_length, false);
  sz += packet_buf.write_string(scramble_ + scramble_part_1_length);  // NULL terminated
  sz += packet_buf.write_string(auth_plugin_name);
  return sz;
}

inline uint32_t HandshakePacket::get_thread_id() const
{
  return thread_id_;
}

inline uint8_t HandshakePacket::get_character_set_nr() const
{
  return character_set_nr_;
}

inline ServerStatus HandshakePacket::get_server_status_flags() const
{
  return server_status_flags_;
}

inline const char* HandshakePacket::get_scramble() const
{
  return scramble_;
}

}  // namespace binlog
}  // namespace oceanbase
