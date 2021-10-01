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

enum class Endian {
  BIG,
  LITTLE,
};

inline bool is_little_endian()
{
  int32_t i = 0x01020304;
  char* buf = (char*)&i;
  return buf[0] == 0x04;
}

template <class Integer>
Integer bswap(Integer integer)
{
  char* src_buf = (char*)&integer;
  Integer ret = 0;
  char* dst_buf = (char*)&ret;
  const int bytes = sizeof(integer);
  for (int i = 0; i < bytes; i++) {
    dst_buf[i] = src_buf[bytes - i - 1];
  }
  return ret;
}

template <class Integer>
Integer le_to_cpu(Integer integer)
{
  if (is_little_endian()) {
    return integer;
  }
  return bswap<Integer>(integer);
}

template <class Integer>
Integer cpu_to_le(Integer integer)
{
  return le_to_cpu<Integer>(integer);
}

template <class Integer>
Integer be_to_cpu(Integer integer)
{
  if (!is_little_endian()) {
    return integer;
  }
  return bswap<Integer>(integer);
}

template <class Integer>
Integer cpu_to_be(Integer integer)
{
  return be_to_cpu<Integer>(integer);
}

template <class Integer, Endian endian>
Integer transform_endian(Integer integer)
{
  if (endian == Endian::BIG) {
    if (!is_little_endian()) {
      return integer;
    }
    return bswap(integer);
  } else {
    if (is_little_endian()) {
      return integer;
    }
    return bswap(integer);
  }
}
}  // namespace logproxy
}  // namespace oceanbase
