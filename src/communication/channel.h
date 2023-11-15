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
#include <atomic>

#include <openssl/ssl.h>
#include <event2/event_struct.h>

#include "log.h"
#include "model.h"
#include "config.h"
#include "event.h"
#include "peer.h"

namespace oceanbase {
namespace logproxy {
class Comm;
class Message;

class Channel {
public:
  explicit Channel(const Peer& peer) : _peer(peer)
  {
    _read_event = static_cast<event*>(malloc(event_get_struct_event_size()));
    _read_event->ev_fd = 0;
    _write_event = static_cast<event*>(malloc(event_get_struct_event_size()));
    _write_event->ev_fd = 0;
  }

  virtual ~Channel()
  {
    if (_owned_fd && _peer.fd != 0) {
      close(_peer.fd);
      OMS_STREAM_DEBUG << "Closed fd: " << _peer.fd;
    }
    free(_read_event);
    free(_write_event);
  }

  virtual int after_accept() = 0;

  virtual int after_connect() = 0;

  /**
   * read data from the channel
   * @return >0 the bytes has been read
   *         =0 the connection has been closed or reach the end of file
   *         <0 error occurs
   */
  virtual int read(char* buf, int size) = 0;

  /**
   * read the specified size of data from the channel
   * @return OMS_OK `size` bytes has been read into the buf
   *         OMS_FAILED some errors occurs
   */
  virtual int readn(char* buf, int size) = 0;

  /**
   * write data to the channel
   * @return >0 the bytes has been read
   *         =0 the connection has been closed
   *         <0 error occurs
   */
  virtual int write(const char* buf, int size) = 0;

  /**
   * write the specified size of data from the channel
   * @return OMS_OK all `size` bytes has been write into the channel
   *         OMS_FAILED some errors occurs
   */
  virtual int writen(const char* buf, int size) = 0;

  /**
   * Get the last error message.
   * It will use the errno internal.
   */
  virtual const char* last_error() = 0;

  /**
   * determin if current channel useable
   * if we got nothing from channelFactory through peer-id, we got an DummyChannel which is not ok
   */
  virtual bool ok() const
  {
    return true;
  }

  inline const Peer& peer() const
  {
    return _peer;
  }

  inline void set_communicator(Comm* communicator)
  {
    _communicator = communicator;
  }

  inline Comm* communicator()
  {
    return _communicator;
  }

  void disable_owned_fd()
  {
    _owned_fd = false;
  }

protected:
  friend Comm;

  bool _owned_fd = true;

  // copy when construct
  Peer _peer;

  // for Event context
  Comm* _communicator = nullptr;

  struct event* _read_event = nullptr;
  struct event* _write_event = nullptr;
};

class PlainChannel : public Channel {
public:
  explicit PlainChannel(const Peer&);

  int after_accept() override;
  int after_connect() override;
  int read(char* buf, int size) override;
  int readn(char* buf, int size) override;
  int write(const char* buf, int size) override;
  int writen(const char* buf, int size) override;

  const char* last_error() override;
};

class TlsChannel : public Channel {
public:
  explicit TlsChannel(const Peer&);

  ~TlsChannel() override;

  static void close_global();

  static int init_global(const Config&);

  int init();

  int after_accept() override;
  int after_connect() override;
  int read(char* buf, int size) override;
  int readn(char* buf, int size) override;
  int write(const char* buf, int size) override;
  int writen(const char* buf, int size) override;

  const char* last_error() override;

private:
  // int verify(int preverify_ok, X509_STORE_CTX* ctx);

  static void log_tls_errors();

  int handle_error(int ret);

private:
  static SSL_CTX* _s_ssl_ctx;

  SSL* _ssl = nullptr;

  int _last_error = 0;
  char _error_string[256];
};

class DummyChannel : public Channel {
  OMS_AVOID_COPY(DummyChannel);

public:
  DummyChannel() : Channel(Peer(-1))
  {}

  explicit DummyChannel(const Peer& peer) : Channel(peer)
  {}

  int after_accept() override
  {
    return 0;
  }
  int after_connect() override
  {
    return 0;
  }
  int read(char* buf, int size) override
  {
    return 0;
  }
  int readn(char* buf, int size) override
  {
    return 0;
  }
  int write(const char* buf, int size) override
  {
    return 0;
  }
  int writen(const char* buf, int size) override
  {
    return 0;
  }
  const char* last_error() override
  {
    return nullptr;
  }
  inline bool ok() const override
  {
    return false;
  }
};

}  // namespace logproxy
}  // namespace oceanbase
