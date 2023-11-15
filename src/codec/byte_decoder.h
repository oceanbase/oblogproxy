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

#include <cstdint>
#include <cstdlib>
namespace oceanbase {
namespace logproxy {
static inline void int1store(unsigned char* T, uint8_t A);
static inline void int1store(unsigned char* T, uint8_t A)
{
  *((uint8_t*)T) = A;
}

static inline void int2store(unsigned char* T, uint16_t A);
static inline void int2store(unsigned char* T, uint16_t A)
{
  *((uint16_t*)T) = A;
}

static inline void be_int2store(unsigned char* T, uint16_t A);
static inline void be_int2store(unsigned char* T, uint16_t A)
{
  uint temp = (uint)(A);
  *(((char*)T) + 1) = (char)(temp);
  *(((char*)T) + 0) = (char)(temp >> 8);
}

static inline void int4store(unsigned char* T, uint32_t A);
static inline void int4store(unsigned char* T, uint32_t A)
{
  *((uint32_t*)T) = A;
}
static inline void be_int4store(unsigned char* T, uint32_t A);
static inline void be_int4store(unsigned char* T, uint32_t A)
{
  *(((char*)T) + 3) = ((A));
  *(((char*)T) + 2) = (((A) >> 8));
  *(((char*)T) + 1) = (((A) >> 16));
  *(((char*)T) + 0) = (((A) >> 24));
}

static inline void int8store(unsigned char* T, uint64_t A);
static inline void int8store(unsigned char* T, uint64_t A)
{
  *((uint64_t*)T) = A;
}
static inline void int3store(unsigned char* T, uint32_t A);
static inline void int3store(unsigned char* T, uint32_t A)
{
  *(T) = (unsigned char)(A);
  *(T + 1) = (unsigned char)(A >> 8);
  *(T + 2) = (unsigned char)(A >> 16);
}

static inline void be_int3store(unsigned char* T, uint32_t A);
static inline void be_int3store(unsigned char* T, uint32_t A)
{
  *(((char*)T) + 2) = ((A));
  *(((char*)T) + 1) = (((A) >> 8));
  *(((char*)T) + 0) = (((A) >> 16));
}

static inline void int5store(unsigned char* T, uint64_t A);
static inline void int5store(unsigned char* T, uint64_t A)
{
  *(T) = (unsigned char)(A);
  *(T + 1) = (unsigned char)(A >> 8);
  *(T + 2) = (unsigned char)(A >> 16);
  *(T + 3) = (unsigned char)(A >> 24);
  *(T + 4) = (unsigned char)(A >> 32);
}

static inline void be_int5store(unsigned char* T, uint64_t A);
static inline void be_int5store(unsigned char* T, uint64_t A)
{
  *(((char*)T) + 4) = ((A));
  *(((char*)T) + 3) = (((A) >> 8));
  *(((char*)T) + 2) = (((A) >> 16));
  *(((char*)T) + 1) = (((A) >> 24));
  *(((char*)T) + 0) = (((A) >> 32));
}

static inline void int6store(unsigned char* T, uint64_t A);
static inline void int6store(unsigned char* T, uint64_t A)
{
  *(T) = (unsigned char)(A);
  *(T + 1) = (unsigned char)(A >> 8);
  *(T + 2) = (unsigned char)(A >> 16);
  *(T + 3) = (unsigned char)(A >> 24);
  *(T + 4) = (unsigned char)(A >> 32);
  *(T + 5) = (unsigned char)(A >> 40);
}
static inline void int_variable_store(unsigned char* T, size_t A, int variable);
static inline void int_variable_store(unsigned char* T, size_t A, int variable)
{
  switch (variable) {
    case 1:
      int1store(T, A);
      break;
    case 2:
      int2store(T, A);
      break;
    case 3:
      int3store(T, A);
      break;
    case 4:
      int4store(T, A);
      break;
    case 5:
      int5store(T, A);
      break;
    case 6:
      int6store(T, A);
      break;
    case 8:
      int8store(T, A);
      break;
    default:
      return;
  }
}

static inline unsigned char* packet_store_length(unsigned char* packet, uint64_t length);
static inline unsigned char* packet_store_length(unsigned char* packet, uint64_t length)
{
  if (length < (uint64_t)251LL) {
    *packet = (unsigned char)length;
    return packet + 1;
  }
  /* 251 is reserved for NULL */
  if (length < (uint64_t)65536LL) {
    *packet++ = 252;
    int2store(packet, (uint32_t)length);
    return packet + 2;
  }
  if (length < (uint64_t)16777216LL) {
    *packet++ = 253;
    int3store(packet, (unsigned long)length);
    return packet + 3;
  }
  *packet++ = 254;
  int8store(packet, length);
  return packet + 8;
}
static inline unsigned char* fill_bit_map(size_t bytes);
static inline unsigned char* fill_bit_map(size_t bytes)
{
  auto* bit_map = static_cast<unsigned char*>(malloc(bytes));
  for (size_t i = 0; i < bytes; ++i) {
    *(bit_map + i) = 0xFF;
  }
  return bit_map;
}

static inline uint8_t int1load(unsigned char* T);
static inline uint8_t int1load(unsigned char* T)
{
  return *((uint8_t*)T);
}

static inline uint16_t int2load(unsigned char* T);
static inline uint16_t int2load(unsigned char* T)
{
  return *((uint16_t*)T);
}

static inline uint32_t int4load(unsigned char* T);
static inline uint32_t int4load(unsigned char* T)
{
  return *((uint32_t*)T);
}

static inline uint64_t int8load(unsigned char* T);
static inline uint64_t int8load(unsigned char* T)
{
  return *((uint64_t*)T);
}

static inline uint64_t int6load(unsigned char* T);
static inline uint64_t int6load(unsigned char* T)
{
  return ((uint64_t)(((uint32_t)(T[0])) + (((uint32_t)(T[1])) << 8) + (((uint32_t)(T[2])) << 16) +
                     (((uint32_t)(T[3])) << 24)) +
          (((uint64_t)(T[4])) << 32) + (((uint64_t)(T[5])) << 40));
}

static inline void hf_int1store(unsigned char* T, uint8_t A);
static inline void hf_int1store(unsigned char* T, uint8_t A)
{
  *((unsigned char*)(T)) = (unsigned char)(A);
}
static inline void hf_int2store(unsigned char* T, uint16_t A);
static inline void hf_int2store(unsigned char* T, uint16_t A)
{
  uint def_temp = (uint)(A);
  ((unsigned char*)(T))[1] = (unsigned char)(def_temp);
  ((unsigned char*)(T))[0] = (unsigned char)(def_temp >> 8);
}
static inline void hf_int3store(unsigned char* T, uint32_t A);
static inline void hf_int3store(unsigned char* T, uint32_t A)
{
  uint64_t def_temp = (uint64_t)(A);
  ((unsigned char*)(T))[2] = (unsigned char)(def_temp);
  ((unsigned char*)(T))[1] = (unsigned char)(def_temp >> 8);
  ((unsigned char*)(T))[0] = (unsigned char)(def_temp >> 16);
}
static inline void hf_int4store(unsigned char* T, uint32_t A);
static inline void hf_int4store(unsigned char* T, uint32_t A)
{
  uint64_t def_temp = (uint64_t)(A);
  ((unsigned char*)(T))[3] = (unsigned char)(def_temp);
  ((unsigned char*)(T))[2] = (unsigned char)(def_temp >> 8);
  ((unsigned char*)(T))[1] = (unsigned char)(def_temp >> 16);
  ((unsigned char*)(T))[0] = (unsigned char)(def_temp >> 24);
}
static inline void hf_int5store(unsigned char* T, uint64_t A);
static inline void hf_int5store(unsigned char* T, uint64_t A)
{
  uint64_t def_temp = (uint64_t)(A);
  uint64_t def_temp2 = (uint64_t)((A) >> 32);
  ((unsigned char*)(T))[4] = (unsigned char)(def_temp);
  ((unsigned char*)(T))[3] = (unsigned char)(def_temp >> 8);
  ((unsigned char*)(T))[2] = (unsigned char)(def_temp >> 16);
  ((unsigned char*)(T))[1] = (unsigned char)(def_temp >> 24);
  ((unsigned char*)(T))[0] = (unsigned char)(def_temp2);
}
static inline void hf_int6store(unsigned char* T, uint64_t A);
static inline void hf_int6store(unsigned char* T, uint64_t A)
{
  uint64_t def_temp = (uint64_t)(A), def_temp2 = (uint64_t)((A) >> 32);
  ((unsigned char*)(T))[5] = (unsigned char)(def_temp);
  ((unsigned char*)(T))[4] = (unsigned char)(def_temp >> 8);
  ((unsigned char*)(T))[3] = (unsigned char)(def_temp >> 16);
  ((unsigned char*)(T))[2] = (unsigned char)(def_temp >> 24);
  ((unsigned char*)(T))[1] = (unsigned char)(def_temp2);
  ((unsigned char*)(T))[0] = (unsigned char)(def_temp2 >> 8);
}
static inline void hf_int7store(unsigned char* T, uint64_t A);
static inline void hf_int7store(unsigned char* T, uint64_t A)
{
  uint64_t def_temp = (ulong)(A);
  uint64_t def_temp2 = (ulong)((A) >> 32);
  ((unsigned char*)(T))[6] = (unsigned char)(def_temp);
  ((unsigned char*)(T))[5] = (unsigned char)(def_temp >> 8);
  ((unsigned char*)(T))[4] = (unsigned char)(def_temp >> 16);
  ((unsigned char*)(T))[3] = (unsigned char)(def_temp >> 24);
  ((unsigned char*)(T))[2] = (unsigned char)(def_temp2);
  ((unsigned char*)(T))[1] = (unsigned char)(def_temp2 >> 8);
  ((unsigned char*)(T))[0] = (unsigned char)(def_temp2 >> 16);
}
static inline void hf_int8store(unsigned char* T, uint64_t A);
static inline void hf_int8store(unsigned char* T, uint64_t A)
{
  auto def_temp3 = (uint64_t)(A);
  auto def_temp4 = (uint64_t)((A) >> 32);
  hf_int4store((unsigned char*)(T) + 0, def_temp4);
  hf_int4store((unsigned char*)(T) + 4, def_temp3);
}

static inline void float8store(unsigned char* T, double V)
{
  *(T) = ((unsigned char*)&V)[7];
  *((T) + 1) = (char)((unsigned char*)&V)[6];
  *((T) + 2) = (char)((unsigned char*)&V)[5];
  *((T) + 3) = (char)((unsigned char*)&V)[4];
  *((T) + 4) = (char)((unsigned char*)&V)[3];
  *((T) + 5) = (char)((unsigned char*)&V)[2];
  *((T) + 6) = (char)((unsigned char*)&V)[1];
  *((T) + 7) = (char)((unsigned char*)&V)[0];
}

static inline void float8get(double* V, const unsigned char* M)
{
  double def_temp;
  ((unsigned char*)&def_temp)[0] = (M)[7];
  ((unsigned char*)&def_temp)[1] = (M)[6];
  ((unsigned char*)&def_temp)[2] = (M)[5];
  ((unsigned char*)&def_temp)[3] = (M)[4];
  ((unsigned char*)&def_temp)[4] = (M)[3];
  ((unsigned char*)&def_temp)[5] = (M)[2];
  ((unsigned char*)&def_temp)[6] = (M)[1];
  ((unsigned char*)&def_temp)[7] = (M)[0];
  (*V) = def_temp;
}

}  // namespace logproxy
}  // namespace oceanbase
