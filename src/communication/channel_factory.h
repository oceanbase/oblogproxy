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

#include <mutex>
#include <unordered_map>
#include "channel.h"
#include "peer.h"

namespace oceanbase {
namespace logproxy {
/**
 * channel factory owner all channel property,
 * all creating, releasing, modifying should invoke
 */
class ChannelFactory {
  OMS_SINGLETON(ChannelFactory);
  OMS_AVOID_COPY(ChannelFactory);

  virtual ~ChannelFactory();

public:
  int init(const Config& config);

  Channel& fetch(uint64_t id);

  Channel& add(uint64_t id, const Peer& peer);

  void del(const Channel&);

  void clear(int reserved_fd = 0);

  inline size_t size()
  {
    return _channels.size();
  }

private:
  bool _is_server_mode = true;

  std::unordered_map<uint64_t, Channel*> _channels;

  static std::string _s_channel_type;
  static std::function<Channel*(const Peer&)> _s_creator;

  DummyChannel _dummy;
};

}  // namespace logproxy
}  // namespace oceanbase
