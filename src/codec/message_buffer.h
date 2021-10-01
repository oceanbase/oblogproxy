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

#include <stdlib.h>
#include <deque>
#include "common/log.h"
#include "codec/codec_endian.h"

namespace oceanbase {
namespace logproxy {

class MessageBuffer {
public:
  class Chunk {
  public:
    Chunk(char* buffer, size_t size) : _buffer(buffer), _size(size)
    {}

    Chunk(char* buffer, size_t size, bool owned) : _buffer(buffer), _size(size), _owned(owned)
    {}

    Chunk(Chunk&& other) noexcept
    {
      _buffer = other._buffer;
      _size = other._size;
      _owned = other._owned;

      other._owned = false;
    }

    ~Chunk()
    {
      if (_owned && _buffer != nullptr) {
        free(_buffer);
        _buffer = nullptr;
      }
    }

    char* buffer() const
    {
      return _buffer;
    }

    size_t size() const
    {
      return _size;
    }

  private:
    char* _buffer = nullptr;
    size_t _size = 0;
    bool _owned = true;
  };

public:
  using ChunksType = std::deque<Chunk>;
  using ChunkIterator = ChunksType::iterator;
  using ChunkConstIterator = ChunksType::const_iterator;

public:
  MessageBuffer() = default;

  ~MessageBuffer() = default;

  void swap(MessageBuffer& other)
  {
    if (&other != this) {
      this->_chunks.swap(other._chunks);
    }
  }

  void push_back(char* buffer, size_t size, bool owned = true)
  {
    _chunks.emplace_back(buffer, size, owned);
  }

  void push_back_copy(char* buffer, size_t size)
  {
    char* nbuf = static_cast<char*>(malloc(size));
    memcpy(nbuf, buffer, size);
    _chunks.emplace_back(nbuf, size, true);
  }

  void push_front(char* buffer, int size, bool owned = true)
  {
    _chunks.emplace_front(buffer, size, owned);
  }

  size_t count() const
  {
    return _chunks.size();
  }

  ChunkConstIterator begin() const
  {
    return _chunks.begin();
  }

  ChunkConstIterator end() const
  {
    return _chunks.end();
  }

  size_t byte_size() const;

private:
  ChunksType _chunks;
};

// TODO messageBufferWriter
class MessageBufferReader {
public:
  explicit MessageBufferReader(const MessageBuffer& buffer);

  int read_uint8(uint8_t& i);

  template <class Integer, Endian endian = Endian::LITTLE>
  int read_int(Integer& i)
  {
    int ret = read((char*)&i, sizeof(i));
    if (ret != 0) {
      return ret;
    }
    i = transform_endian<Integer, endian>(i);
    return 0;
  }

  int read(char* buffer, size_t size);

  int next(const char** buffer, int* size);

  int backward(size_t count);

  int forward(size_t count);

  size_t read_size() const;

  /**
   * Total size of MessageBuffer in byte
   */
  size_t byte_size() const;

  size_t remain_size() const;

  bool has_more() const;

  std::string debug_info() const;

private:
  /**
   * Read data into buffer. If it is not enough, reading position is untouched
   * @param buffer  target
   * @param size size to read, in byte
   * @param skip true: no reading(memcpy), just skip it
   */
  int read(char* buffer, size_t size, bool skip);

private:
  const MessageBuffer& _buffer;
  MessageBuffer::ChunkConstIterator _iter;
  size_t _pos = 0;
  size_t _byte_size = 0;
  size_t _read_size = 0;
};

}  // namespace logproxy
}  // namespace oceanbase
