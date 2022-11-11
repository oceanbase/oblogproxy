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

#include "communication/channel_factory.h"

namespace oceanbase {
namespace logproxy {

static inline Channel* _s_create_plain(const Peer& peer)
{
  return new (std::nothrow) PlainChannel(peer);
}

static inline Channel* _s_create_tls(const Peer& peer)
{
  return new (std::nothrow) TlsChannel(peer);
}

std::string ChannelFactory::_s_channel_type;
std::function<Channel*(const Peer&)> ChannelFactory::_s_creator;

ChannelFactory::~ChannelFactory()
{
  if (_s_channel_type == "tls") {
    TlsChannel::close_global();
  }
}

int ChannelFactory::init(const Config& config)
{
  int ret = OMS_OK;
  const std::string& mode = config.communication_mode.val();
  if (mode == "server") {
    _is_server_mode = true;
  } else if (mode == "client") {
    _is_server_mode = false;
  } else {
    OMS_ERROR << "Invalid config of communication_mode: " << mode << ". can be one of server/client";
    return OMS_FAILED;
  }
  OMS_INFO << "ChannelFactory init with " << mode << " mode";

  _s_channel_type = config.channel_type.val();
  if (_s_channel_type == "plain") {
    _s_creator = _s_create_plain;

  } else if (_s_channel_type == "tls") {

    ret = TlsChannel::init_global(config);
    if (ret == OMS_OK) {
      _s_creator = _s_create_tls;
    }

  } else {

    OMS_ERROR << "Unsupported channel type: " << _s_channel_type;
    ret = OMS_FAILED;
  }
  return ret;
}

Channel& ChannelFactory::fetch(uint64_t id)
{
  auto iter = _channels.find(id);
  if (iter == _channels.end()) {
    return _dummy;
  }
  return *(iter->second);
}

Channel& ChannelFactory::add(uint64_t id, const Peer& peer)
{
  //  const std::lock_guard<std::mutex> lock_guard(_lock);

  auto iter = _channels.find(id);
  if (iter != _channels.end()) {
    OMS_ERROR << "Duplicate channel with id: " << id;
    return _dummy;
  }

  Channel* ch = _s_creator(peer);
  if (ch == nullptr) {
    OMS_ERROR << "Failed to allocate Channel memory";
    return _dummy;
  }
  _channels.emplace(id, ch);

  int ret = _is_server_mode ? ch->after_accept() : ch->after_connect();
  if (ret != OMS_OK) {
    delete ch;
    _channels.erase(id);
    return _dummy;
  }
  return *ch;
}

void ChannelFactory::del(const Channel& channel)
{
  auto iter = _channels.find(channel.peer().id());
  if (iter != _channels.end()) {
    delete iter->second;
    _channels.erase(iter);
  }
}

void ChannelFactory::clear(int reserved_fd)
{
  for (auto& channel : _channels) {
    if (reserved_fd != 0 && channel.second != nullptr && channel.second->peer().fd == reserved_fd) {
      channel.second->disable_owned_fd();
    }
    delete channel.second;
  }
  _channels.clear();
}

}  // namespace logproxy
}  // namespace oceanbase
