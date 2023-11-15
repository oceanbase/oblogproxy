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

#include <string.h>
#include "msg_buf.h"
#include "codec_endian.h"
#include "log.h"

namespace oceanbase {
namespace logproxy {
size_t MsgBuf::byte_size() const
{
  size_t result = 0;
  for (auto& chunk : _chunks) {
    result += chunk.size();
  }
  return result;
}

size_t MsgBuf::bytes(char* buff)
{
  size_t pos = 0;
  for (auto& chunk : _chunks) {
    memcpy(buff + pos, chunk.buffer(), chunk.size());
    pos += chunk.size();
  }
  return pos;
}

const std::deque<MsgBuf::Chunk>& MsgBuf::get_chunks() const
{
  return _chunks;
}

////////////////////////////////////////////////////////////////////////////////
MsgBufReader::MsgBufReader(const MsgBuf& buffer) : _buffer(buffer)
{
  _iter = buffer.begin();
  _byte_size = _buffer.byte_size();
}

int MsgBufReader::read(char* buf, size_t size)
{
  if (size == 0) {
    return 0;
  }
  if (buf == nullptr) {
    OMS_STREAM_ERROR << "Invalid argument. buffer=" << (intptr_t)buf << ", size=" << size;
    return -1;
  }
  return read(buf, size, false);
}

int MsgBufReader::read(char* buf, size_t size, bool skip)
{
  auto chunk_iter = _iter;
  size_t chunk_pos = _pos;
  size_t read_size = 0;
  for (; read_size < size && chunk_iter != _buffer.end();) {
    const MsgBuf::Chunk& chunk = *chunk_iter;
    const size_t chunk_size = chunk.size() - chunk_pos;
    if (read_size + chunk_size >= size) {
      if (!skip) {
        memcpy(buf + read_size, chunk.buffer() + chunk_pos, size - read_size);
      }
      chunk_pos += size - read_size;
      read_size = size;

      _iter = chunk_iter;
      _pos = chunk_pos;

      _read_size += size;
      return 0;
    } else {
      if (!skip) {
        memcpy(buf + read_size, chunk.buffer() + chunk_pos, chunk_size);
      }
      read_size += chunk_size;
      ++chunk_iter;
      chunk_pos = 0;
    }
  }

  OMS_STREAM_ERROR << "Size read: " << read_size << " not enough, expected size: " << size;
  return -1;
}

int MsgBufReader::read_uint8(uint8_t& i)
{
  return read((char*)&i, sizeof(i));
}

int MsgBufReader::read_uint16(uint16_t& i)
{
  return read((char*)&i, sizeof(i));
}

int MsgBufReader::read_uint24(uint32_t& i)
{
  return read((char*)&i, 3);
}

int MsgBufReader::read_uint32(uint32_t& i)
{
  return read((char*)&i, sizeof(i));
}

int MsgBufReader::read_uint48(uint64_t& i)
{
  return read((char*)&i, 6);
}

int MsgBufReader::read_uint64(uint64_t& i)
{
  return read((char*)&i, sizeof(i));
}

int MsgBufReader::next(const char** buffer, int* size)
{
  if (_iter == _buffer.end()) {
    OMS_STREAM_DEBUG << "got EOF while call next";
    return -1;
  }

  do {
    auto& chunk = *_iter;
    *buffer = chunk.buffer() + _pos;
    *size = chunk.size() - _pos;
    ++_iter;
    _pos = 0;
  } while (*size == 0 && _iter != _buffer.end());

  if (*size == 0 && _iter == _buffer.end()) {
    OMS_STREAM_DEBUG << "Touch the end of buffer";
    return -1;
  }
  return 0;
}

int MsgBufReader::backward(size_t count)
{
  if (count > _read_size) {
    OMS_STREAM_ERROR << "Failed to backward data. count=" << count << ", read_size=" << _read_size;
    return -1;
  }

  size_t backward_count = count;
  auto chunk_iter = _iter;
  size_t chunk_pos = _pos;
  while (backward_count > chunk_pos) {
    backward_count -= chunk_pos;
    --chunk_iter;
    chunk_pos = chunk_iter->size();
  }
  if (backward_count > 0) {
    chunk_pos -= backward_count;
  }
  _iter = chunk_iter;
  _pos = chunk_pos;
  _read_size -= count;
  return 0;
}

int MsgBufReader::forward(size_t count)
{
  return read(nullptr, count, true);
}

size_t MsgBufReader::read_size() const
{
  return _read_size;
}

size_t MsgBufReader::byte_size() const
{
  return _byte_size;
}

size_t MsgBufReader::remain_size() const
{
  return _byte_size - _read_size;
}

bool MsgBufReader::has_more() const
{
  auto iter = _iter;
  size_t pos = _pos;
  while (iter != _buffer.end()) {
    auto& chunk = *iter;
    if (pos < chunk.size()) {
      return true;
    }
    ++iter;
    pos = 0;
  }
  return false;
}

std::string MsgBufReader::debug_info() const
{
  LogStream ls(0, "", 0, nullptr);
  ls << "[MsgBuf] all: " << byte_size() << ", "
     << "read: " << read_size() << ", "
     << "remain: " << remain_size();
  return ls.str();
}

int MysqlBufReader::read_lenenc_uint(uint64_t& value)
{
  value = 0;
  uint8_t byte = 0;
  read((char*)&byte, 1);

  if (byte < 251) {
    value = byte;
  } else if (byte == 252) {
    read((char*)&value, 2);
  } else if (byte == 253) {
    read((char*)&value, 3);
  } else if (byte == 254) {
    read((char*)&value, 8);
  } else if (byte == 251) {
    return 251;
  } else if (byte == 255) {
    return 255;
  }
  return 0;
}

void MysqlBufReader::read_lenenc_str(std::string& value)
{
  uint64_t length = 0;
  read_lenenc_uint(length);

  // FIXME... length secure check
  value.resize(length);
  read((char*)value.data(), length);
}

}  // namespace logproxy
}  // namespace oceanbase
