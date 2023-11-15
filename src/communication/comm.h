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

#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>

#include "codec/message.h"
#include "channel_factory.h"
#include "codec/encoder.h"
#include "codec/decoder.h"

namespace oceanbase {
namespace logproxy {
enum class EventResult {
  ER_SUCCESS = 0,
  ER_CLOSE_CHANNEL  /// will close the connection
};

class Comm {
public:
  Comm();

  virtual ~Comm();

  int stop(int reserved_fd = 0);

  void close_listen();

  int init();

  int listen(uint16_t listen_port);

  int start();

  int poll();

  int add(const Peer& peer);

  void del(const Channel& ch);

  void del(const Peer& peer);

  void trigger_del(const Peer& peer, const Message& message);

  int send_message(const Peer& peer, const Message& msg, bool direct = false);

  int write_message(Channel& ch, const Message&);

  void debug_events();

  inline size_t channel_count()
  {
    return _channel_factory.size();
  }

  inline void set_routine_callback(const std::function<void()>& routine_callback)
  {
    _routine_callback = routine_callback;
  }

  inline void set_read_callback(const std::function<EventResult(const Peer&, const Message&)>& read_callback)
  {
    _read_callback = read_callback;
  }

  inline void set_write_callback(const std::function<EventResult(const Peer&, const Message&)>& write_callback)
  {
    _write_callback = write_callback;
  }

  void set_close_callback(const std::function<void(const Peer&)>& close_callback)
  {
    _close_callback = close_callback;
  }

private:
  void _s_evcb_on_accept(int fd, const struct sockaddr_in&);

  //  void _s_evcb_on_accept_error(struct evconnlistener* listener, void* ctx);

  static void _s_evcb_on_event(int fd, short event, void* arg);

  static PacketError _s_read_message(Channel& ch, Message*& msg);

private:
  Timer _stage_timer;

  ChannelFactory& _channel_factory = ChannelFactory::instance();
  static MessageDecoder* _s_decoders[3];
  static MessageEncoder* _s_encoders[3];

  int _listenfd = 0;
  //  struct evconnlistener* _listener = nullptr;
  struct event_base* _event_base = nullptr;

  std::function<EventResult(const Peer&, const Message&)> _read_callback;
  std::function<EventResult(const Peer&, const Message&)> _write_callback;
  std::function<void(const Peer&)> _close_callback;
  std::function<void()> _routine_callback;
};

}  // namespace logproxy
}  // namespace oceanbase
