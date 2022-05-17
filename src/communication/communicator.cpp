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
#include <arpa/inet.h>
#include "event2/listener.h"

#include "communication/communicator.h"
#include "communication/io.h"
#include "common/log.h"
#include "common/common.h"
#include "common/config.h"
#include "common/counter.h"
#include "codec/msg_buf.h"
#include "codec/decoder.h"
#include "codec/encoder.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

////////////////////////////////////////////////////////////////////////////////

Communicator::Communicator()
{
  _decoders[(int)MessageVersion::V0] = &LegacyDecoder::instance();
  _decoders[(int)MessageVersion::V1] = &LegacyDecoder::instance();
  _decoders[(int)MessageVersion::V2] = &ProtobufDecoder::instance();

  _encoders[(int)MessageVersion::V0] = &LegacyEncoder::instance();
  _encoders[(int)MessageVersion::V1] = &LegacyEncoder::instance();
  _encoders[(int)MessageVersion::V2] = &ProtobufEncoder::instance();
}

Communicator::~Communicator()
{
  stop();

  auto event_callback = _read_callback.exchange(nullptr);
  delete event_callback;

  auto error_callback = _error_callback.exchange(nullptr);
  delete error_callback;

  auto disconnection_callback = _disconnection_callback.exchange(nullptr);
  delete disconnection_callback;
}

int Communicator::init()
{
  if (_event_base != nullptr) {
    OMS_WARN << "Communicator has already started";
    return OMS_OK;
  }

  _event_base = event_base_new();
  if (nullptr == _event_base) {
    OMS_ERROR << "Failed to create event base. system error " << strerror(errno);
    return OMS_FAILED;
  }

  return _channel_factory.init(_s_config);
}

void Communicator::accept_conn_cb(struct evconnlistener*, evutil_socket_t fd, struct sockaddr* address, int, void* ctx)
{
  struct sockaddr_in* peer_addr = (struct sockaddr_in*)address;
  uint16_t peer_port = ntohs(peer_addr->sin_port);
  char peer_host[INET_ADDRSTRLEN];
  inet_ntop(PF_INET, &peer_addr->sin_addr, peer_host, sizeof(peer_host));
  OMS_INFO << "On connect from '" << peer_host << ':' << peer_port << ", fd: " << fd;

  Communicator* comm = (Communicator*)ctx;
  PeerInfo peer(fd);
  comm->add_channel(peer);
}

static void accept_error_cb(struct evconnlistener* listener, void* ctx)
{
  struct event_base* base = evconnlistener_get_base(listener);
  int err = EVUTIL_SOCKET_ERROR();
  OMS_ERROR << "Got an error " << err << " (" << evutil_socket_error_to_string(err) << ") on the listener. ";
  event_base_loopexit(base, nullptr);
}

int Communicator::listen(uint16_t listen_port)
{
  if (_listener != nullptr) {
    return OMS_OK;
  }

  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(0);
  sin.sin_port = htons(listen_port);
  _listener = evconnlistener_new_bind(_event_base,
      accept_conn_cb,
      this,
      LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE,
      -1,
      (struct sockaddr*)&sin,
      sizeof(sin));
  if (_listener == nullptr) {
    OMS_ERROR << "Failed to listen socket with port: " << listen_port << ". error=" << strerror(errno);
    return OMS_FAILED;
  }
  evconnlistener_set_error_cb(_listener, accept_error_cb);

  OMS_INFO << "+++ Listen on port: " << listen_port << ", fd: " << evconnlistener_get_fd(_listener);
  return OMS_OK;
}

int Communicator::start()
{
  OMS_INFO << "+++ Communicator about to start";
  int ret = OMS_OK;
  ret = event_base_dispatch(_event_base);
  if (ret == 1) {
    return OMS_AGAIN;
  }
  if (ret != 0) {
    OMS_ERROR << "Failed to run Communicator. event base dispatch error, ret: " << ret;
    return OMS_FAILED;
  }

  //  do {
  //    ret = poll();
  //    if (ret != OMS_OK && ret != OMS_AGAIN) {
  //      OMS_ERROR << "Failed to poll communication, ret: " << ret;
  //      break;
  //    }
  //    debug_events();
  //
  //    ::sleep(1);
  //  } while (true);

  OMS_WARN << "!!! Communicator about to quit";
  return OMS_OK;
}

int Communicator::poll()
{
  int ret = event_base_loop(_event_base, EVLOOP_NONBLOCK);
  if (ret == 1) {
    return OMS_AGAIN;
  }
  if (ret != 0) {
    OMS_ERROR << "Failed to run Communicator. event base dispatch error, ret: " << ret;
    return OMS_FAILED;
  }
  return OMS_OK;
}

int Communicator::stop()
{
  if (nullptr == _event_base) {
    OMS_INFO << "communication not started";
    return OMS_OK;
  }

  OMS_INFO << "Communicator stopping";
  event_base_loopexit(_event_base, nullptr);

  event_base_free(_event_base);
  _event_base = nullptr;
  OMS_INFO << "Communicator stopped";
  return OMS_OK;
}

int Communicator::add_channel(const PeerInfo& peer, Channel* ch /* = nullptr */)
{
  if (peer.file_desc < 0) {
    OMS_ERROR << "Invalid argument. fd=" << peer.file_desc;
    return OMS_FAILED;
  }

  std::lock_guard<std::mutex> lock_guard(_lock);
  auto iter = _channels.find(peer);
  if (iter != _channels.end()) {
    OMS_WARN << "Add channel twice: " << peer.file_desc;
    return OMS_FAILED;
  }

  int ret = set_non_block(peer.file_desc);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to set fd non block. fd=" << peer.file_desc;
    return ret;
  }

  if (nullptr == ch) {
    ch = _channel_factory.create(peer);
    if (nullptr == ch) {
      OMS_ERROR << "Failed to create new channel. fd=" << peer.file_desc;
      return OMS_FAILED;
    }
  }

  ch->set_communicator(this);

  ch->set_flag(ch->flag() | EV_READ);
  OMS_INFO << "Add channel, peer: " << ch->peer().to_string();
  ret = event_assign(&ch->_read_event, _event_base, peer.file_desc, EV_READ | EV_PERSIST /*| EV_ET*/, on_event, ch);
  if (ret < 0) {
    OMS_ERROR << "Failed to do event_assign. fd=" << peer.file_desc;
    return OMS_FAILED;
  }

  ret = event_add(&ch->_read_event, nullptr);
  if (ret < 0) {
    OMS_ERROR << "Failed to do event_add. fd=" << peer.file_desc;
    return OMS_FAILED;
  }
  _channels.emplace(peer, ch);
  OMS_INFO << "+++ Add read channel to Communicator with peer: " << peer.to_string();
  return OMS_OK;
}

int Communicator::release_channel_event(Channel& ch, bool steal)
{
  OMS_INFO << "--- Removed channel from Communicator which steal: " << steal << ", peer: " << ch.peer().to_string()
           << " flag:" << ch.flag();

  if (ch.flag() & EV_READ) {
    int ret = event_del(&(ch._read_event));
    if (ret < 0) {
      OMS_ERROR << "Failed to do event_del read. channel fd=" << ch._peer.file_desc;
      //    return OMS_FAILED;
    }
  }

  if (ch.flag() & EV_WRITE) {
    int ret = event_del(&(ch._write_event));
    if (ret < 0) {
      OMS_ERROR << "Failed to do event_del write. channel fd=" << ch._peer.file_desc;
      //    return OMS_FAILED;
    }
  }

  // close(fd) done by channel destor
  ch.release(!steal);
  return OMS_OK;
}

int Communicator::remove_channel(const PeerInfo& peer, bool steal /* = false */)
{
  if (peer.file_desc < 0) {
    OMS_WARN << "Invalid argument. fd=" << peer.file_desc;
    return OMS_FAILED;
  }

  Channel* ch = nullptr;
  {
    std::lock_guard<std::mutex> lock_guard(_lock);
    auto iter = _channels.find(peer);
    if (iter == _channels.end()) {
      OMS_WARN << "No channel found of peer: " << peer.to_string();
      return OMS_OK;
    }
    ch = iter->second;
    _channels.erase(iter);

    auto disconnection_callback = _disconnection_callback.load();
    if (disconnection_callback != nullptr) {
      (*disconnection_callback)(ch->_peer);
    }
  }

  return release_channel_event(*ch, steal);
}

int Communicator::clear_channels()
{
  std::lock_guard<std::mutex> lock_guard(_lock);
  for (auto& channel : _channels) {
    Channel* ch = channel.second;
    release_channel_event(*ch, false);
  }
  _channels.clear();
  return OMS_OK;
}

Channel* Communicator::get_channel(const PeerInfo& peer)
{
  const std::lock_guard<std::mutex> lock_guard(_lock);
  return get_channel_locked(peer);
}

Channel* Communicator::get_channel_locked(const PeerInfo& peer)
{
  auto iter = _channels.find(peer);
  if (iter == _channels.end()) {
    return nullptr;
  }
  return iter->second->get();
}

int Communicator::send_message(const PeerInfo& peer, const Message& msg, bool direct)
{
  Channel* ch = get_channel(peer);
  if (ch == nullptr) {
    OMS_ERROR << "No such channel. fd=" << peer.file_desc;
    return OMS_FAILED;
  }

  if (direct) {
    int ret = write_message(ch, msg);
    ch->put();
    return ret;
  }

  ch->set_write_msg(&msg);
  ch->set_flag(ch->flag() | EV_WRITE);
  int ret = event_assign(&ch->_write_event, _event_base, peer.file_desc, EV_WRITE /*| EV_ET*/, on_event, ch);
  if (ret < 0) {
    OMS_ERROR << "Failed to do event_assign write. fd=" << peer.file_desc;
    ch->put();
    return OMS_FAILED;
  }
  ret = event_add(&ch->_write_event, nullptr);
  if (ret < 0) {
    OMS_ERROR << "Failed to do event_add. fd=" << peer.file_desc;
    ch->put();
    return OMS_FAILED;
  }

  ch->put();
  return OMS_OK;
}

int Communicator::write_message(Channel* ch, const Message& msg)
{
  if (_s_config.verbose_packet.val()) {
    OMS_INFO << "About to write mssage: " << msg.debug_string() << ", ch: " << ch->peer().id()
             << ", msg type: " << (int)msg.type();
  }

  MsgBuf buffer;
  int ret = _encoders[(uint16_t)msg.version()]->encode(msg, buffer);
  if (ret != OMS_OK) {
    OMS_ERROR << "Encoding message failed";
    return ret;
  }

  size_t wsize = 0;
  for (const auto& chunk : buffer) {
    if (OMS_OK != ch->writen(chunk.buffer(), chunk.size())) {
      OMS_ERROR << "failed to send message. ch:" << ch->peer().id() << ", error:" << ch->last_error();
      return OMS_FAILED;
    }
    wsize += chunk.size();
  }

  Counter::instance().count_write_io(wsize);
  return OMS_OK;
}

/*
 *
 * =========== Packet Header ============
 * [7] magic number
 * [2] version
 */
PacketError Communicator::receive_message(Channel* ch, Message*& msg)
{
  uint16_t version;
  if (_s_config.packet_magic.val()) {
    char packet_buf[PACKET_MAGIC_SIZE + PACKET_VERSION_SIZE];
    if (ch->readn(packet_buf, sizeof(packet_buf)) != OMS_OK) {
      OMS_ERROR << "Failed to read packet magic+version, ch:" << ch->peer().id() << ", error:" << strerror(errno);
      return PacketError::NETWORK_ERROR;
    }
    if (strncmp(packet_buf, PACKET_MAGIC, PACKET_MAGIC_SIZE) != 0) {
      OMS_ERROR << "Invalid packet magic, ignore and close request, ch:" << ch->peer().id();
      return PacketError::PROTOCOL_ERROR;
    }
    memcpy(&version, packet_buf + PACKET_MAGIC_SIZE, PACKET_VERSION_SIZE);
  } else {
    if (ch->readn((char*)&version, PACKET_VERSION_SIZE) != OMS_OK) {
      OMS_ERROR << "Failed to read packet version, ch:" << ch->peer().id() << ", error:" << strerror(errno);
      return PacketError::NETWORK_ERROR;
    }
  }

  version = be_to_cpu<uint16_t>(version);
  if (!is_version_available(version)) {
    OMS_ERROR << "Invalid packet version:" << version << ", ch:" << ch->peer().id();
    return PacketError::PROTOCOL_ERROR;
  }

  auto ret = _decoders[version]->decode(ch, (MessageVersion)version, msg);
  if (msg != nullptr) {
    msg->set_version((MessageVersion)version);
  }
  return ret;
}

void Communicator::on_event(int fd, short event, void* arg)
{
  OMS_UNUSED(event);

  Channel* ch = (Channel*)arg;
  if (ch == nullptr) {
    OMS_ERROR << "Cannot find channel for fd=" << fd << " when receive message, will close it";
    close(fd);  // manual close here due to no channel manager it
    return;
  }
  ch->get();

  Communicator& c = *ch->get_communicator();

  OMS_INFO << "On event fd: " << fd << " got channel, peer: " << ch->peer().to_string();
  EventResult err = EventResult::ER_SUCCESS;

  if ((event & EV_FINALIZE) || (event & EV_CLOSED)) {
    OMS_WARN << "On event close, peer: " << ch->peer().to_string();
    if (event & EV_READ) {
      event_del(&ch->_read_event);
    }
    if (event & EV_WRITE) {
      event_del(&ch->_write_event);
    }
    err = EventResult::ER_CLOSE_CHANNEL;

  } else {
    if ((event & EV_WRITE) && ch->_write_msg != nullptr) {
      OMS_INFO << "On event about to write message: " << ch->peer().to_string();
      if (c.write_message(ch, *ch->_write_msg) != OMS_OK) {
        err = EventResult::ER_CLOSE_CHANNEL;
      }
      ch->set_write_msg(nullptr);
      event_del(&ch->_write_event);  // one-shot
    }

    if (event & EV_READ) {
      Message* msg = nullptr;
      PacketError result = c.receive_message(ch, msg);
      if (result == PacketError::SUCCESS) {
        auto event_callback = c._read_callback.load();
        if (event_callback != nullptr) {
          err = (*event_callback)(ch->_peer, *msg);
        }
      } else if (result == PacketError::IGNORE) {
        // do nothing
      } else {
        OMS_ERROR << "Failed to handle receive message, ret: " << (int)result;
        auto error_callback = c._error_callback.load();
        if (error_callback != nullptr) {
          err = (*error_callback)(ch->_peer, result);
        } else {
          err = EventResult::ER_CLOSE_CHANNEL;
        }
      }
      delete msg;
    }
  }
  switch (err) {
    case EventResult::ER_CLOSE_CHANNEL:
      c.remove_channel(ch->_peer);
      break;
    default:
      // do nothing
      break;
  }
  ch->put();
}

void Communicator::close_listen()
{
  if (_listener != nullptr) {
    int fd = evconnlistener_get_fd(_listener);
    evconnlistener_disable(_listener);
    close(fd);
    evconnlistener_free(_listener);
    _listener = nullptr;
    OMS_WARN << ">>> Communicator disabled listening, fd: " << fd;
  }
}

void Communicator::fork_prepare()
{
  OMS_WARN << ">>> Fork preparing, fetching lock...";
  _lock.lock();
  OMS_WARN << "--- Fork prepared, fetched lock";
}

void Communicator::fork_after()
{
  OMS_WARN << "<<< Fork after, unlock";
  fork_child_reinit();
  _lock.unlock();
}

void Communicator::fork_child_reinit()
{
  event_reinit(_event_base);
}

static int debug_events_cb(const struct event_base*, const struct event* ev, void* ctx)
{
  OMS_DEBUG << "fd: " << event_get_fd(ev) << ", flag: " << event_get_events(ev);
  return 0;
}

void Communicator::debug_events()
{
  event_base_foreach_event(_event_base, debug_events_cb, this);
}

int Communicator::set_read_callback(ReadCbFunc callback)
{
  ReadCbFunc* p = new ReadCbFunc(std::move(callback));
  auto old = _read_callback.exchange(p);
  delete old;

  return OMS_OK;
}

int Communicator::set_error_callback(ErrorCbFunc callback)
{
  ErrorCbFunc* p = new ErrorCbFunc(std::move(callback));
  auto old = _error_callback.exchange(p);
  delete old;

  return OMS_OK;
}

int Communicator::set_disconnection_callback(CloseCbFunc callback)
{
  CloseCbFunc* p = new CloseCbFunc(std::move(callback));
  auto old = _disconnection_callback.exchange(p);
  delete old;

  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
