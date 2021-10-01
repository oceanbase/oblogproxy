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

#include "event.h"
#include "common/log.h"
#include "common/model.h"
#include "common/config.h"

namespace oceanbase {
namespace logproxy {

struct PeerInfo : public Model {
  OMS_MF_ENABLE_COPY(PeerInfo);

  OMS_MF_DFT(int, file_desc, -1);

public:
  explicit PeerInfo(int fd = -1) : file_desc(fd)
  {}

  inline int id() const
  {
    return file_desc;
  }

  inline bool operator==(const PeerInfo& other) const
  {
    return this->file_desc == other.file_desc;
  }

  inline bool operator!=(const PeerInfo& other) const
  {
    return this->file_desc != other.file_desc;
  }

  inline bool operator<(const PeerInfo& rhs) const
  {
    return file_desc < rhs.file_desc;
  }

  inline std::size_t operator()(const PeerInfo& p) const
  {
    return p.file_desc;
  }

  inline friend LogStream& operator<<(LogStream& ss, const PeerInfo& o)
  {
    ss << "fd:" << o.file_desc;
    return ss;
  }
};

class Communicator;
class Message;

class Channel {
public:
  explicit Channel(const PeerInfo& peer);

  virtual ~Channel();

  Channel* get();

  void put();

  /**
   * release the connection
   * @param owned true means the Channel who hold the connection will close it, otherwise will just leave it when
   * destructing
   */
  void release(bool owned);

  Communicator* get_communicator() const
  {
    return _communicator;
  }
  void set_communicator(Communicator* c)
  {
    _communicator = c;
  }
  inline const PeerInfo& peer() const
  {
    return _peer;
  }

  int flag() const
  {
    return _flag;
  }

  void set_flag(int flag)
  {
    _flag = flag;
  }

  inline void set_write_msg(const Message* write_msg)
  {
    _write_msg = write_msg;
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

  virtual const char* last_error() = 0;

protected:
  friend Communicator;
  PeerInfo _peer;
  Communicator* _communicator = nullptr;

private:
  int _flag = 0;
  struct event _read_event;
  struct event _write_event;
  const Message* _write_msg = nullptr;

  bool _owned = true;
  std::atomic<int> _refcount;
};

class PlainChannel : public Channel {
public:
  explicit PlainChannel(const PeerInfo& peer);

  int after_accept() override;
  int after_connect() override;
  int read(char* buf, int size) override;
  int readn(char* buf, int size) override;
  int write(const char* buf, int size) override;
  int writen(const char* buf, int size) override;

  /**
   * Get the last error message.
   * It will use the errno internal.
   */
  const char* last_error() override;

private:
};

class TlsChannel : public Channel {
public:
  explicit TlsChannel(const PeerInfo& peer);
  ~TlsChannel() override;

  int after_accept() override;
  int after_connect() override;
  int read(char* buf, int size) override;
  int readn(char* buf, int size) override;
  int write(const char* buf, int size) override;
  int writen(const char* buf, int size) override;

  /**
   * Get the last error message.
   * The return buffer cannot be freed
   */
  const char* last_error() override;

public:
  int init_tls(SSL_CTX* ssl_context);
  // int verify(int preverify_ok, X509_STORE_CTX* ctx);

private:
  int handle_error(int ret);

private:
  SSL* _ssl = nullptr;

  int _last_error = 0;
  char _error_string[256];
};

class ChannelFactory {
public:
  ChannelFactory();
  ~ChannelFactory();

  int init(const Config& config);

  Channel* create(const PeerInfo& peer);

public:
  /**
   * log error information of TLS
   * used by ChannelFactory and TlsChannel
   */
  static void log_tls_errors();

private:
  Channel* create_plain(const PeerInfo& peer);
  Channel* create_tls(const PeerInfo& peer);

private:
  int init_tls_context(const char* ca_cert_file, const char* cert_file, const char* key_file, bool verify_peer);

private:
  Channel* (ChannelFactory::*_channel_creator)(const PeerInfo& peer);

  SSL_CTX* _ssl_context = nullptr;
  bool _is_server_mode = true;
};

}  // namespace logproxy
}  // namespace oceanbase
