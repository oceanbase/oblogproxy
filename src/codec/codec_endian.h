/**
 * Copyright (c) 2021 OceanBase
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

#include <stdint.h>

namespace oceanbase {
namespace logproxy {

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define le_to_cpu(integer) (integer)
#define cpu_to_le(integer) (integer)

inline uint16_t be_to_cpu(uint16_t integer)
{
  return __builtin_bswap16(integer);
}

inline uint32_t be_to_cpu(uint32_t integer)
{
  return __builtin_bswap32(integer);
}

inline uint64_t be_to_cpu(uint64_t integer)
{
  return __builtin_bswap64(integer);
}

inline uint16_t cpu_to_be(uint16_t integer)
{
  return __builtin_bswap16(integer);
}

inline uint32_t cpu_to_be(uint32_t integer)
{
  return __builtin_bswap32(integer);
}

inline uint64_t cpu_to_be(uint64_t integer)
{
  return __builtin_bswap64(integer);
}

#else

#endif

}  // namespace logproxy
}  // namespace oceanbase
