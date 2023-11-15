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
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include "byte_order.h"

namespace oceanbase {
namespace binlog {

inline PacketBuf::PacketBuf(uint32_t capacity) : PacketBuf(capacity, capacity)
{}

inline PacketBuf::PacketBuf(uint32_t min_capacity, uint32_t max_capacity)
    : buf_(nullptr),
      capacity_(min_capacity),
      min_capacity_(min_capacity),
      max_capacity_(max_capacity),
      read_index_(0),
      write_index_(0)
{
  if (capacity_ > 0) {
    buf_ = new uint8_t[capacity_];
  }
}

inline PacketBuf::~PacketBuf()
{
  delete[] buf_;
}

inline bool PacketBuf::is_readable() const
{
  return write_index_ > read_index_;
}

inline bool PacketBuf::is_readable(size_t nof_bytes) const
{
  return write_index_ - read_index_ >= nof_bytes;
}

inline size_t PacketBuf::readable_bytes() const
{
  return write_index_ - read_index_;
}

inline bool PacketBuf::is_fast_writable() const
{
  return capacity_ > write_index_;
}

inline bool PacketBuf::is_fast_writable(size_t nof_bytes) const
{
  return capacity_ - write_index_ >= nof_bytes;
}

inline size_t PacketBuf::fast_writable_bytes() const
{
  return capacity_ - write_index_;
}

inline void PacketBuf::clear()
{
  read_index_ = 0;
  write_index_ = 0;
  error_msg_ = "";
}

inline void PacketBuf::reset()
{
  clear();
  if (capacity_ > min_capacity_) {
    delete[] buf_;
    buf_ = new uint8_t[min_capacity_];
    capacity_ = min_capacity_;
  }
}

inline ssize_t PacketBuf::write_uint1(uint8_t v)
{
  if (!ensure_writable(1)) {
    return -1;
  }
  return write_htole8(buf_, write_index_, v);
}

inline ssize_t PacketBuf::write_uint2(uint16_t v)
{
  if (!ensure_writable(2)) {
    return -1;
  }
  return write_htole16(buf_, write_index_, v);
}

inline ssize_t PacketBuf::write_uint3(uint32_t v)
{
  if (!ensure_writable(3)) {
    return -1;
  }
  return write_htole24(buf_, write_index_, v);
}

inline ssize_t PacketBuf::write_uint4(uint32_t v)
{
  if (!ensure_writable(4)) {
    return -1;
  }
  return write_htole32(buf_, write_index_, v);
}

inline ssize_t PacketBuf::write_uint6(uint64_t v)
{
  if (!ensure_writable(6)) {
    return -1;
  }
  return write_htole48(buf_, write_index_, v);
}

inline ssize_t PacketBuf::write_uint8(uint64_t v)
{
  if (!ensure_writable(8)) {
    return -1;
  }
  return write_htole64(buf_, write_index_, v);
}

inline ssize_t PacketBuf::write_uint(uint64_t v)
{
  uint8_t length = encoded_bytes_length(v);

  if (!ensure_writable(length)) {
    return -1;
  }

  if (1 == length) {
    write_htole8(buf_, write_index_, v);
  } else if (3 == length) {
    write_htole8(buf_, write_index_, 0xFC);
    write_htole16(buf_, write_index_, v);
  } else if (4 == length) {
    write_htole8(buf_, write_index_, 0xFD);
    write_htole24(buf_, write_index_, v);
  } else {
    // 9 == length
    write_htole8(buf_, write_index_, 0xFE);
    write_htole64(buf_, write_index_, v);
  }
  return length;
}

inline ssize_t PacketBuf::write_string(const char* str, uint32_t len, bool prefix_with_encoded_len)
{
  uint32_t total_len = (prefix_with_encoded_len ? encoded_bytes_length(len) : 0) + len;
  if (!ensure_writable(total_len)) {
    return -1;
  }

  if (prefix_with_encoded_len) {
    write_uint(len);
  }
  memcpy(buf_ + write_index_, str, len);
  write_index_ += len;
  return total_len;
}

inline ssize_t PacketBuf::write_string(const std::string& str, bool prefix_with_encoded_len)
{
  return write_string(str.data(), str.length(), prefix_with_encoded_len);
}

inline ssize_t PacketBuf::write_string(const char* str)
{
  return write_string(str, strlen(str) + 1, false);
}

inline ssize_t PacketBuf::read_uint1(uint8_t& v)
{
  if (!ensure_readable(1)) {
    return -1;
  }
  return read_le8toh(buf_, read_index_, v);
}

inline ssize_t PacketBuf::read_uint2(uint16_t& v)
{
  if (!ensure_readable(2)) {
    return -1;
  }
  return read_le16toh(buf_, read_index_, v);
}

inline ssize_t PacketBuf::read_uint3(uint32_t& v)
{
  if (!ensure_readable(3)) {
    return -1;
  }
  return read_le24toh(buf_, read_index_, v);
}

inline ssize_t PacketBuf::read_uint4(uint32_t& v)
{
  if (!ensure_readable(4)) {
    return -1;
  }
  return read_le32toh(buf_, read_index_, v);
}

inline ssize_t PacketBuf::read_uint6(uint64_t& v)
{
  if (!ensure_readable(6)) {
    return -1;
  }
  return read_le48toh(buf_, read_index_, v);
}

inline ssize_t PacketBuf::read_uint8(uint64_t& v)
{
  if (!ensure_readable(8)) {
    return -1;
  }
  return read_le64toh(buf_, read_index_, v);
}

inline ssize_t PacketBuf::read_uint(uint64_t& v)
{
  if (!ensure_readable(1)) {
    return -1;
  }
  uint8_t first_byte = buf_[read_index_];  // advance read_index_ atomically
  if (first_byte < 0xFB) {                 // 251
    v = first_byte;
    ++read_index_;
    return 1;
  }

  switch (first_byte) {
    case 0xFC:
      if (!ensure_readable(3)) {
        return -1;
      }
      ++read_index_;
      uint16_t tmp_h16;
      read_le16toh(buf_, read_index_, tmp_h16);
      v = tmp_h16;
      return 3;
    case 0xFD:
      if (!ensure_readable(4)) {
        return -1;
      }
      ++read_index_;
      uint32_t tmp_h24;
      read_le24toh(buf_, read_index_, tmp_h24);
      v = tmp_h24;
      return 4;
    case 0xFE:
      if (!ensure_readable(9)) {
        return -1;
      }
      ++read_index_;
      read_le64toh(buf_, read_index_, v);
      return 9;
    default:  // 0xFB and 0xFF
      set_error_msg("Try to read an encoded integer, but the first byte is " + std::to_string(first_byte));
      return -1;
  }
}

inline ssize_t PacketBuf::read_fixed_length_string(std::string& str, uint32_t len)
{
  if (!ensure_readable(len)) {
    return -1;
  }
  str = std::string(reinterpret_cast<const char*>(buf_ + read_index_), len);
  read_index_ += len;
  return len;
}

inline ssize_t PacketBuf::read_length_encoded_string(std::string& str)
{
  uint64_t len;
  ssize_t len_read_sz = read_uint(len);
  if (len_read_sz < 0) {
    return -1;
  }
  ssize_t str_read_sz = read_fixed_length_string(str, len);
  if (str_read_sz < 0) {
    read_index_ -= len_read_sz;  // backoff the read_index
    return -1;
  }
  return len_read_sz + str_read_sz;
}

inline ssize_t PacketBuf::read_null_terminated_string(std::string& str)
{
  str = std::string(reinterpret_cast<const char*>(buf_ + read_index_));
  uint32_t read_sz = str.length() + 1;
  read_index_ += read_sz;
  return read_sz;
}

inline ssize_t PacketBuf::read_bytes(const uint8_t*& buf, uint32_t len)
{
  if (!ensure_readable(len)) {
    return -1;
  }
  buf = buf_ + read_index_;
  read_index_ += len;
  return len;
}

inline ssize_t PacketBuf::read_skip(uint32_t len)
{
  if (!ensure_readable(len)) {
    return -1;
  }
  read_index_ += len;
  return len;
}

inline uint32_t PacketBuf::read_remaining_bytes(const uint8_t*& buf)
{
  buf = buf_ + read_index_;
  uint32_t sz = write_index_ - read_index_;
  read_index_ = write_index_;
  return sz;
}

inline bool PacketBuf::ensure_readable(size_t bytes)
{
  if (!is_readable(bytes)) {
    set_error_msg(
        "Try to read " + std::to_string(bytes) + "B, but only " + std::to_string(readable_bytes()) + "B readable");
    return false;
  }
  return true;
}

inline bool PacketBuf::ensure_writable(size_t bytes)
{
  uint32_t new_write_index = write_index_ + bytes;
  if (new_write_index <= capacity_) {
    return true;
  }
  if (new_write_index > max_capacity_) {
    set_error_msg("Packet buffer size too large: " + std::to_string(new_write_index) +
                  ", max allowed size: " + std::to_string(max_capacity_));
    return false;
  }
  extend(std::min((new_write_index + io_size - 1) & ~(io_size - 1), max_capacity_));
  return true;
}

inline void PacketBuf::extend(uint32_t new_capacity)
{
  // assert(new_capacity > capacity_);
  auto* new_buf = new uint8_t[new_capacity];
  memcpy(new_buf + read_index_, buf_ + read_index_, write_index_ - read_index_);
  delete[] buf_;
  buf_ = new_buf;
  capacity_ = new_capacity;
}

inline const std::string& PacketBuf::get_error_msg() const
{
  return error_msg_;
}

inline void PacketBuf::set_error_msg(std::string error_msg)
{
  error_msg_ = std::move(error_msg);
}

inline uint8_t* PacketBuf::get_buf() const
{
  return buf_;
}

inline uint32_t PacketBuf::get_read_index() const
{
  return read_index_;
}

inline uint32_t PacketBuf::get_write_index() const
{
  return write_index_;
}

inline void PacketBuf::set_read_index(uint32_t read_index)
{
  read_index_ = read_index;
}

inline void PacketBuf::set_write_index(uint32_t write_index)
{
  write_index_ = write_index;
}

}  // namespace binlog
}  // namespace oceanbase