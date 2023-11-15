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

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <endian.h>

namespace oceanbase {
namespace binlog {
static inline uint8_t encoded_bytes_length(uint64_t v)
{
  if (v < 0xFB) {  // 251
    return 1;
  }

  if (v < (1UL << 16)) {
    return 3;
  }

  if (v < (1UL << 24)) {
    return 4;
  }

  return 9;
}

static inline uint8_t read_le8toh(const uint8_t* buf, uint32_t& read_index, uint8_t& v)
{
  v = buf[read_index++];
  return 1;
}

static inline uint8_t read_le16toh(const uint8_t* buf, uint32_t& read_index, uint16_t& v)
{
  auto* p = (uint16_t*)(buf + read_index);
  v = le16toh(*p);
  read_index += 2;
  return 2;
}

static inline uint8_t read_le24toh(const uint8_t* buf, uint32_t& read_index, uint32_t& v)
{
  uint16_t l_16;
  uint8_t m_8;
  read_le16toh(buf, read_index, l_16);
  read_le8toh(buf, read_index, m_8);
  v = (static_cast<uint32_t>(m_8) << 16) | l_16;
  return 3;
}

static inline uint8_t read_le32toh(const uint8_t* buf, uint32_t& read_index, uint32_t& v)
{
  auto* p = (uint32_t*)(buf + read_index);
  v = le32toh(*p);
  read_index += 4;
  return 4;
}

static inline uint8_t read_le48toh(const uint8_t* buf, uint32_t& read_index, uint64_t& v)
{
  uint32_t l_32;
  uint16_t m_16;
  read_le32toh(buf, read_index, l_32);
  read_le16toh(buf, read_index, m_16);
  v = (static_cast<uint64_t>(m_16) << 32) | l_32;
  return 6;
}

static inline uint8_t read_le64toh(const uint8_t* buf, uint32_t& read_index, uint64_t& v)
{
  auto* p = (uint64_t*)(buf + read_index);
  v = le64toh(*p);
  read_index += 8;
  return 8;
}

static inline uint8_t write_htole8(uint8_t* buf, uint32_t& write_index, uint8_t v)
{
  buf[write_index++] = v;
  return 1;
}

static inline uint8_t write_htole16(uint8_t* buf, uint32_t& write_index, uint16_t v)
{
  v = htole16(v);
  memcpy(buf + write_index, &v, 2);
  write_index += 2;
  return 2;
}

static inline uint8_t write_htole24(uint8_t* buf, uint32_t& write_index, uint32_t v)
{
  write_htole16(buf, write_index, static_cast<uint16_t>(v));
  write_htole8(buf, write_index, static_cast<uint8_t>(v >> 16));
  return 3;
}

static inline uint8_t write_htole32(uint8_t* buf, uint32_t& write_index, uint32_t v)
{
  v = htole32(v);
  memcpy(buf + write_index, &v, 4);
  write_index += 4;
  return 4;
}

static inline uint8_t write_htole48(uint8_t* buf, uint32_t& write_index, uint64_t v)
{
  write_htole32(buf, write_index, static_cast<uint32_t>(v));
  write_htole16(buf, write_index, static_cast<uint16_t>(v >> 32));
  return 6;
}

static inline uint8_t write_htole64(uint8_t* buf, uint32_t& write_index, uint64_t v)
{
  v = htole64(v);
  memcpy(buf + write_index, &v, 8);
  write_index += 8;
  return 8;
}

}  // namespace binlog
}  // namespace oceanbase
