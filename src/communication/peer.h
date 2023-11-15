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

#include "timer.h"

namespace oceanbase {
namespace logproxy {
struct Peer {
public:
  int fd = 0;

public:
  explicit Peer(in_addr_t a = 0, in_port_t p = 0, int fd = 0) : fd(fd)
  {
    // HIGH
    //  ^
    //  | 32bit: peer ip
    //  | 16bit: peer port
    //  | 16bit: fd & UINT16_MAX
    // LOW
    _id = (((uint64_t)a) << 32);
    _id |= (uint32_t(p) << 16);
    _id |= (fd & UINT16_MAX);

    _addr = a;
    _port = p;
  }

  Peer(const Peer& rhs) = default;

  Peer(Peer&& rhs) noexcept = default;

  Peer& operator=(const Peer& rhs) = default;

  Peer& operator=(Peer&& rhs) = default;

  inline uint64_t id() const
  {
    return _id;
  }

  inline bool operator==(const Peer& other) const
  {
    return this->_id == other._id;
  }

  inline bool operator!=(const Peer& other) const
  {
    return this->_id != other._id;
  }

  inline bool operator<(const Peer& rhs) const
  {
    return _id < rhs._id;
  }

  inline std::size_t operator()(const Peer& p) const
  {
    return p._id;
  }

  inline friend LogStream& operator<<(LogStream& ss, const Peer& o)
  {
    ss << "fd:" << o.fd;
    return ss;
  }

  std::string to_string() const
  {
    LogStream ls{0, "", 0, nullptr};
    ls << "id:" << _id << ", fd:" << fd << ", addr:" << _addr << ", port:" << _port;
    return ls.str();
  }

  void from_json(const rapidjson::Value& json) {
    _id = json["id"].GetInt64();
    fd = json["fd"].GetInt();
    _addr = json["addr"].GetUint();
    _port = json["port"].GetUint();
  }

  void to_json(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const
  {
    writer.Key("peer");
    writer.StartObject();
    writer.Key("id");writer.Uint64(_id);
    writer.Key("fd");writer.Int(fd);
    writer.Key("addr");writer.Uint(_addr);
    writer.Key("port");writer.Uint(_port);
    writer.EndObject();
  }

private:
  uint64_t _id;
  in_addr_t _addr;
  in_port_t _port;
};

}  // namespace logproxy
}  // namespace oceanbase