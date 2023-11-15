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

#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>

namespace oceanbase {
namespace binlog {
class PacketBuf {
public:
  explicit PacketBuf(uint32_t capacity);

  PacketBuf(uint32_t min_capacity, uint32_t max_capacity);

  PacketBuf(const PacketBuf&) = delete;
  PacketBuf& operator=(const PacketBuf&) = delete;

  PacketBuf(const PacketBuf&&) = delete;
  PacketBuf& operator=(const PacketBuf&&) = delete;

  ~PacketBuf();

  bool is_readable() const;

  bool is_readable(size_t nof_bytes) const;

  size_t readable_bytes() const;

  bool is_fast_writable() const;

  bool is_fast_writable(size_t nof_bytes) const;

  size_t fast_writable_bytes() const;

  // clear read/write index and error message
  void clear();

  // clear read/write index and error message, and shrink the buffer to min_capacity
  void reset();

  const std::string& get_error_msg() const;

  ssize_t write_uint1(uint8_t v);

  ssize_t write_uint2(uint16_t v);

  ssize_t write_uint3(uint32_t v);

  ssize_t write_uint4(uint32_t v);

  ssize_t write_uint6(uint64_t v);

  ssize_t write_uint8(uint64_t v);

  ssize_t write_uint(uint64_t v);

  ssize_t write_string(const char* str, uint32_t len, bool prefix_with_encoded_len = false);

  ssize_t write_string(const std::string& str, bool prefix_with_encoded_len = false);

  ssize_t write_string(const char* str);

  ssize_t read_uint1(uint8_t& v);

  ssize_t read_uint2(uint16_t& v);

  ssize_t read_uint3(uint32_t& v);

  ssize_t read_uint4(uint32_t& v);

  ssize_t read_uint6(uint64_t& v);

  ssize_t read_uint8(uint64_t& v);

  ssize_t read_uint(uint64_t& v);

  ssize_t read_fixed_length_string(std::string& str, uint32_t len);

  ssize_t read_length_encoded_string(std::string& str);

  ssize_t read_null_terminated_string(std::string& str);

  ssize_t read_bytes(const uint8_t*& buf, uint32_t len);

  ssize_t read_skip(uint32_t len);

  uint32_t read_remaining_bytes(const uint8_t*& buf);

  bool ensure_writable(size_t bytes);

  uint8_t* get_buf() const;

  uint32_t get_read_index() const;

  uint32_t get_write_index() const;

  void set_read_index(uint32_t read_index);

  void set_write_index(uint32_t write_index);

private:
  bool ensure_readable(size_t bytes);

  void extend(uint32_t new_capacity);

  void set_error_msg(std::string error_msg);

private:
  static constexpr uint32_t io_size = 4096;

private:
  /* constraint:
   * 1. 0 <= read_index_ <= write_index_ <= capacity_
   * 2. min_capacity_ <= capacity_ <= max_capacity_
   * */
  uint8_t* buf_;
  uint32_t capacity_;
  const uint32_t min_capacity_;
  const uint32_t max_capacity_;
  uint32_t read_index_;
  uint32_t write_index_;
  std::string error_msg_;
};

}  // namespace binlog
}  // namespace oceanbase

#include "packet_buf.inl"
