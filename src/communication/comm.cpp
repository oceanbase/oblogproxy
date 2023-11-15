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

#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <deque>

#include "communication/comm.h"
#include "communication/io.h"
#include "common/counter.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_conf = Config::instance();

MessageDecoder* Comm::_s_decoders[3];
MessageEncoder* Comm::_s_encoders[3];

////////////////////////////////////////////////////////////////////////////////

Comm::Comm()
{
  _s_decoders[(int)MessageVersion::V0] = &LegacyDecoder::instance();
  _s_decoders[(int)MessageVersion::V1] = &LegacyDecoder::instance();
  _s_decoders[(int)MessageVersion::V2] = &ProtobufDecoder::instance();

  _s_encoders[(int)MessageVersion::V0] = &LegacyEncoder::instance();
  _s_encoders[(int)MessageVersion::V1] = &LegacyEncoder::instance();
  _s_encoders[(int)MessageVersion::V2] = &ProtobufEncoder::instance();

  //  event_enable_debug_logging(EVENT_DBG_ALL);
}

Comm::~Comm()
{
  stop();
  // FIXME... memory leak, but coredump
  //  event_base_free(_event_base);
  _event_base = nullptr;
}

int Comm::stop(int reserved_fd)
{
  close_listen();
  _channel_factory.clear(reserved_fd);

  if (_event_base == nullptr) {
    OMS_INFO << "communication not started";
    return OMS_OK;
  }

  OMS_INFO << "Communicator stopping";
  event_base_loopexit(_event_base, nullptr);
  OMS_INFO << "Communicator stopped";
  return OMS_OK;
}

void Comm::close_listen()
{
  if (_listenfd <= 0) {
    OMS_WARN << ">>> Communicator disabled listening, fd: " << _listenfd;
    close(_listenfd);
    _listenfd = 0;
  }
}

int Comm::init()
{
  if (_event_base != nullptr) {
    OMS_WARN << "Communicator has already started";
    return OMS_OK;
  }

  _event_base = event_base_new();
  if (_event_base == nullptr) {
    OMS_ERROR << "Failed to create event base. system error " << strerror(errno);
    return OMS_FAILED;
  }

  return OMS_OK;
}

int Comm::listen(uint16_t listen_port)
{
  if (_listenfd > 0) {
    return OMS_OK;
  }

  _listenfd = oceanbase::logproxy::listen(nullptr, listen_port, false, true);
  if (_listenfd <= 0) {
    return OMS_FAILED;
  }

  OMS_INFO << "+++ Listen on port: " << listen_port << ", fd: " << _listenfd;
  return OMS_OK;
}

int Comm::start()
{
  OMS_INFO << "+++ Communicator about to start";

  while (_listenfd > 0) {
    int ret = poll();
    if (ret != OMS_OK && ret != OMS_AGAIN) {
      OMS_ERROR << "Failed to poll communication, ret: " << ret;
      break;
    }

    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(struct sockaddr_in);
    int connfd = accept(_listenfd, (struct sockaddr*)&peer_addr, &peer_addr_size);
    if (connfd > 0) {
      _s_evcb_on_accept(connfd, peer_addr);
      continue;  // may be other connection, dealing directly
    }

    if (_routine_callback) {
      _routine_callback();
    }

    usleep(_s_conf.accept_interval_us.val());
  }

  OMS_WARN << "!!! Communicator about to quit";
  return OMS_OK;
}

int Comm::poll()
{
  int ret = event_base_loop(_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
  if (ret == 1) {
    return OMS_AGAIN;
  }
  if (ret != 0) {
    OMS_ERROR << "Failed to run Comm, event base dispatch error, ret: " << ret;
    return OMS_FAILED;
  }
  return OMS_OK;
}

void Comm::_s_evcb_on_accept(int fd, const struct sockaddr_in& peer_addr)
{
  uint16_t peer_port = ntohs(peer_addr.sin_port);
  char peer_host[INET_ADDRSTRLEN];
  inet_ntop(PF_INET, &peer_addr.sin_addr, peer_host, sizeof(peer_host));

  int ret = set_non_block(fd);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to set non block for fd: " << fd;
    close(fd);
    return;
  }

  OMS_DEBUG << "On connect from '" << peer_host << ':' << peer_port << ", fd: " << fd;

  Peer peer(peer_addr.sin_addr.s_addr, peer_port, fd);
  if (add(peer) != OMS_OK) {
    evutil_closesocket(fd);
  }
}

int Comm::add(const Peer& peer)
{
  uint64_t peer_id = peer.id();
  const Channel& exist = _channel_factory.fetch(peer_id);
  if (exist.ok()) {
    OMS_WARN << "Add channel twice, peer: " << peer.to_string() << ", close last: {}";

    // this is unexpected in case of last fd was sent to child process, that current process deleted channel already
    // now we try to close last fd first,
    // then find child process and stop child process(source reader)

    // could be trigger EV_CLOSED after, and release channel there
    close(exist.peer().fd);

    return OMS_FAILED;
  }

  Channel& ch = _channel_factory.add(peer_id, peer);
  if (!ch.ok()) {
    OMS_ERROR << "Failed to create new channel, peer: " << peer.to_string();
    return OMS_FAILED;
  }
  ch.set_communicator(this);

  if (_s_conf.verbose.val()) {
    OMS_INFO << "Add channel, peer: " << ch.peer().to_string();
  }

  // we use level trigger as simple, and expect READ first for handshake packet
  int ret = event_assign(ch._read_event, _event_base, peer.fd, EV_READ | EV_PERSIST, _s_evcb_on_event, &ch);
  if (ret < 0) {
    OMS_ERROR << "Failed to do assign event, peer: " << peer.to_string();
    _channel_factory.del(ch);
    return OMS_FAILED;
  }

  ret = event_add(ch._read_event, nullptr);
  if (ret < 0) {
    OMS_ERROR << "Failed to add event, peer: " << peer.to_string();
    _channel_factory.del(ch);
    return OMS_FAILED;
  }

  if (_s_conf.verbose.val()) {
    OMS_INFO << "Add read channel to Communicator with peer: " << peer.to_string();
  }
  return OMS_OK;
}

static std::deque<const Message*> _s_msg_queue;

void Comm::_s_evcb_on_event(int fd, short event, void* arg)
{
  assert(arg != nullptr);
  // Single thread context here

  Channel& ch = *(Channel*)arg;
  Comm& comm = *ch.communicator();

  OMS_DEBUG << "On event fd: " << fd << " got channel, peer: " << ch.peer().to_string();
  EventResult err = EventResult::ER_SUCCESS;

  if (event & EV_CLOSED) {
    OMS_WARN << "On event close, peer: " << ch.peer().to_string();
    if (event & EV_READ) {
      event_del(ch._read_event);
    }
    if (event & EV_WRITE) {
      event_del(ch._write_event);
    }
    err = EventResult::ER_CLOSE_CHANNEL;

  } else {

    if ((event & EV_WRITE)) {
      OMS_DEBUG << "On event about to write message: " << ch.peer().to_string();
      if (!_s_msg_queue.empty()) {
        const Message* msg = _s_msg_queue.front();
        _s_msg_queue.pop_front();
        if (comm.write_message(ch, *msg) != OMS_OK) {
          err = EventResult::ER_CLOSE_CHANNEL;
        }
      }

      event_del(ch._write_event);  // one-shot
    }

    if (event & EV_READ) {
      Message* msg = nullptr;
      PacketError result = _s_read_message(ch, msg);
      if (result == PacketError::SUCCESS) {
        if (comm._read_callback) {
          err = comm._read_callback(ch.peer(), *msg);
        }
      } else if (result == PacketError::IGNORE) {
        // do nothing
      } else {
        if (_s_conf.verbose.val()) {
          OMS_ERROR << "Failed to handle read message, ret: " << (int)result;
        }
        err = EventResult::ER_CLOSE_CHANNEL;
      }
      delete msg;
    }
  }

  if (err == EventResult::ER_CLOSE_CHANNEL) {
    if (comm._close_callback) {
      comm._close_callback(ch.peer());
    }
    comm.del(ch);
  }
}

/*
 *
 * =========== Packet Header ============
 * [7] magic number
 * [2] version
 */
PacketError Comm::_s_read_message(Channel& ch, Message*& msg)
{
  uint16_t version;
  if (_s_conf.packet_magic.val()) {
    char packet_buf[PACKET_MAGIC_SIZE + PACKET_VERSION_SIZE];
    if (ch.readn(packet_buf, sizeof(packet_buf)) != OMS_OK) {
      OMS_DEBUG << "Failed to read packet magic+version, ch:" << ch.peer().id() << ", error:" << strerror(errno);
      return PacketError::NETWORK_ERROR;
    }
    if (strncmp(packet_buf, PACKET_MAGIC, PACKET_MAGIC_SIZE) != 0) {
      OMS_DEBUG << "Invalid packet magic, ignore and close ch:" << ch.peer().id();
      return PacketError::PROTOCOL_ERROR;
    }
    memcpy(&version, packet_buf + PACKET_MAGIC_SIZE, PACKET_VERSION_SIZE);
  } else {
    if (ch.readn((char*)&version, PACKET_VERSION_SIZE) != OMS_OK) {
      OMS_ERROR << "Failed to read packet version, ch:" << ch.peer().id() << ", error:" << strerror(errno);
      return PacketError::NETWORK_ERROR;
    }
  }

  version = be_to_cpu(version);
  if (!is_version_available(version)) {
    OMS_ERROR << "Invalid packet version:" << version << ", ch:" << ch.peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  auto ret = _s_decoders[version]->decode(ch, (MessageVersion)version, msg);
  if (msg != nullptr) {
    msg->set_version((MessageVersion)version);
  }
  return ret;
}

void Comm::del(const Channel& ch)
{
  if (ch.ok()) {
    OMS_DEBUG << "Try close Channel of peer: " << ch.peer().to_string();
    if (ch._read_event != nullptr && ch._read_event->ev_fd != 0) {
      event_del(ch._read_event);
    }
    if (ch._write_event != nullptr && ch._write_event->ev_fd != 0) {
      event_del(ch._write_event);
    }
    _channel_factory.del(ch);
  }
}

inline void Comm::del(const Peer& peer)
{
  del(_channel_factory.fetch(peer.id()));
}

void Comm::trigger_del(const Peer& peer, const Message& message)
{
  send_message(peer, message, true);
  // we don't care succ or not, just close
  del(peer);
}

int Comm::send_message(const Peer& peer, const Message& msg, bool direct)
{
  Channel& ch = _channel_factory.fetch(peer.id());
  if (!ch.ok()) {
    OMS_ERROR << "Not found channel of peer:" << peer.to_string() << ", just close it";
    return OMS_FAILED;
  }

  if (direct) {
    return write_message(ch, msg);
  }

  int ret = event_assign(ch._write_event, _event_base, peer.fd, EV_WRITE /*| EV_ET*/, _s_evcb_on_event, this);
  if (ret < 0) {
    OMS_ERROR << "Failed to do event_assign write. fd=" << peer.fd;
    return OMS_FAILED;
  }
  ret = event_add(ch._write_event, nullptr);
  if (ret < 0) {
    OMS_ERROR << "Failed to do event_add. fd=" << peer.fd;
    return OMS_FAILED;
  }

  return OMS_OK;
}

int Comm::write_message(Channel& ch, const Message& msg)
{
  if (_s_conf.verbose_packet.val()) {
    OMS_INFO << "About to write mssage: " << msg.debug_string() << ", ch: " << ch.peer().id()
             << ", msg type: " << (int)msg.type();
  }

  _stage_timer.reset();
  size_t raw_len = 0;
  MsgBuf buffer;
  int ret = _s_encoders[(uint16_t)msg.version()]->encode(msg, buffer, raw_len);
  if (ret != OMS_OK) {
    OMS_ERROR << "Encoding message failed";
    return ret;
  }

  Counter::instance().count_key(Counter::SENDER_ENCODE_US, _stage_timer.stopwatch());

  size_t wsize = 0;
  for (const auto& chunk : buffer) {
    if (OMS_OK != ch.writen(chunk.buffer(), chunk.size())) {
      OMS_ERROR << "Failed to send message through channel:" << ch.peer().id() << ", error:" << ch.last_error();
      return OMS_FAILED;
    }
    wsize += chunk.size();
  }

  Counter::instance().count_key(Counter::SENDER_SEND_US, _stage_timer.elapsed());
  Counter::instance().count_write_io(raw_len);
  Counter::instance().count_xwrite_io(wsize);
  return OMS_OK;
}

static int debug_events_cb(const struct event_base*, const struct event* ev, void* ctx)
{
  OMS_DEBUG << "fd: " << event_get_fd(ev) << ", evflag: " << event_get_events(ev);
  return 0;
}

void Comm::debug_events()
{
  event_base_foreach_event(_event_base, debug_events_cb, this);
}

}  // namespace logproxy
}  // namespace oceanbase
