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

#include "event.h"
#include "common/log.h"
#include "codec/message.h"
#include "communication/channel.h"
#include "codec/encoder.h"
#include "codec/decoder.h"

namespace oceanbase {
namespace logproxy {

enum class EventResult {
  ER_SUCCESS = 0,
  ER_CLOSE_CHANNEL  /// will close the connection
};

class Communicator {
public:
  using ReadCbFunc = std::function<EventResult(const PeerInfo&, const Message&)>;
  using ErrorCbFunc = std::function<EventResult(const PeerInfo&, PacketError)>;
  using CloseCbFunc = std::function<void(const PeerInfo&)>;

public:
  Communicator();

  virtual ~Communicator();

  int init();

  int listen(uint16_t listen_port);

  int start();

  int poll();

  int stop();

  /**
   * add a new connection
   * @param peer the information of the connection which `fd` included
   * @param ch   the channel, if you have. we will create a new channel if null
   * @note is_server_mode if the `ch` is null, we will create a new one and do the
   * `ch.after_accept` or `ch.after_connect` respectively (like in TLS mode, it will
   * do the handshake)
   */
  int add_channel(const PeerInfo& peer, Channel* ch = nullptr);

  /**
   * Get the channel associate with peer
   * @return The channel returned will increase reference
   */
  Channel* get_channel(const PeerInfo& peer);

  /**
   * remove a channel
   * @param peer  the connection information
   * @param steal true means no close the connection otherwise it will be closed
   */
  int remove_channel(const PeerInfo& peer, bool steal = false);

  int clear_channels();

  int send_message(const PeerInfo& peer, const Message& msg, bool direct = false);

  void close_listen();

  /**
   * do preparing before fork action.
   * we lock inside in case of any other action was stopped or pending,
   * after then we invoked fork(), it's safely to do 'unlock' make any action go continue
   */
  void fork_prepare();

  void fork_after();

  void fork_child_reinit();

  void debug_events();

  /**
   * The callback when receiving message
   */
  int set_read_callback(ReadCbFunc callback);

  /**
   * The callback when receiving message comes wrong
   */
  int set_error_callback(ErrorCbFunc callback);

  /**
   * The callback when closing one connection
   */
  int set_disconnection_callback(CloseCbFunc callback);

private:
  Channel* get_channel_locked(const PeerInfo& peer);

  /**
   * MUST BE call in thread-safe context
   */
  int release_channel_event(Channel& ch, bool steal);

  static void on_event(int fd, short event, void* arg);
  static void accept_conn_cb(struct evconnlistener*, evutil_socket_t fd, struct sockaddr* address, int, void* ctx);

  int write_message(Channel* ch, const Message& msg);

  PacketError receive_message(Channel* ch, Message*& msg);

private:
  std::mutex _lock;

  struct evconnlistener* _listener = nullptr;

  std::unordered_map<PeerInfo, Channel*, PeerInfo> _channels;
  std::atomic<ReadCbFunc*> _read_callback{nullptr};
  std::atomic<ErrorCbFunc*> _error_callback{nullptr};
  std::atomic<CloseCbFunc*> _disconnection_callback{nullptr};

  struct event_base* _event_base = nullptr;

  MessageDecoder* _decoders[3];
  MessageEncoder* _encoders[3];

  ChannelFactory _channel_factory;
};

}  // namespace logproxy
}  // namespace oceanbase
