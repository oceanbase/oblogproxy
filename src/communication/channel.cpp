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

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "communication/channel.h"
#include "communication/io.h"
#include "communication/communicator.h"

namespace oceanbase {
namespace logproxy {

Channel::Channel(const PeerInfo& peer) : _refcount(1), _peer(peer)
{
  memset(&_write_event, 0, sizeof(_write_event));
  memset(&_read_event, 0, sizeof(_read_event));
}

Channel::~Channel()
{
  if (_peer.file_desc >= 0) {
    if (_owned) {
      ::close(_peer.file_desc);
    }
    _peer.file_desc = -1;
  }
}

Channel* Channel::get()
{
  ++_refcount;
  return this;
}

void Channel::put()
{
  if (1 == _refcount.fetch_sub(1)) {
    OMS_DEBUG << "delete Channel";
    delete this;
  }
}

void Channel::release(bool owned)
{
  _owned = owned;
  put();
}

////////////////////////////////////////////////////////////////////////////////
PlainChannel::PlainChannel(const PeerInfo& peer) : Channel(peer)
{}

int PlainChannel::after_accept()
{
  return OMS_OK;
}
int PlainChannel::after_connect()
{
  return OMS_OK;
}

int PlainChannel::read(char* buf, int size)
{
  return ::read(_peer.file_desc, buf, size);
}

int PlainChannel::readn(char* buf, int size)
{
  return ::oceanbase::logproxy::readn(_peer.file_desc, buf, size);
}

int PlainChannel::write(const char* buf, int size)
{
  return ::write(_peer.file_desc, buf, size);
}

int PlainChannel::writen(const char* buf, int size)
{
  return ::oceanbase::logproxy::writen(_peer.file_desc, buf, size);
}

const char* PlainChannel::last_error()
{
  return strerror(errno);
}

////////////////////////////////////////////////////////////////////////////////
TlsChannel::TlsChannel(const PeerInfo& peer) : Channel(peer)
{}

TlsChannel::~TlsChannel()
{
  if (_ssl != nullptr) {
    SSL_free(_ssl);
    _ssl = nullptr;
  }
}

int TlsChannel::init_tls(SSL_CTX* ssl_context)
{
  SSL* ssl = SSL_new(ssl_context);
  if (nullptr == ssl) {
    OMS_ERROR << "Failed to create ssl";
    return OMS_FAILED;
  }

  int ret = SSL_set_fd(ssl, _peer.file_desc);
  if (ret != 1) {
    ChannelFactory::log_tls_errors();
    SSL_free(ssl);
    return OMS_FAILED;
  }
  _ssl = ssl;
  return OMS_OK;
}

int TlsChannel::after_accept()
{
  // refer https://www.openssl.org/docs/manmaster/man3/SSL_accept.html
  // if fd is nonblocking, SSL_accept may return a negative number
  while (true) {
    const int ret = SSL_accept(_ssl);
    if (ret < 0) {
      const int error = SSL_get_error(_ssl, ret);
      if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_ACCEPT) {
        usleep(1000 * 10);  // TODO instead with poll
        continue;
      } else {
        OMS_ERROR << "Failed to accept new connection. error=" << error;
        ChannelFactory::log_tls_errors();
        return OMS_FAILED;
      }
    } else if (ret == 0) {
      OMS_ERROR << "Failed to accept new connection. error is 0";
      ChannelFactory::log_tls_errors();
      return OMS_FAILED;
    } else if (ret == 1) {
      OMS_DEBUG << "accept new connection success";
      return OMS_OK;
    }
  }
  return OMS_FAILED;
}
int TlsChannel::after_connect()  // TODO simplify with after_accept
{
  // refer https://www.openssl.org/docs/manmaster/man3/SSL_connect.html
  // if fd is nonblocking, SSL_accept may return a negative number
  while (true) {
    const int ret = SSL_connect(_ssl);
    if (ret < 0) {
      const int error = SSL_get_error(_ssl, ret);
      if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_ACCEPT) {
        usleep(1000 * 10);  // TODO instead with poll
        continue;
      } else {
        OMS_ERROR << "Failed to connect. error=" << error;
        ChannelFactory::log_tls_errors();
        return OMS_FAILED;
      }
    } else if (ret == 0) {
      OMS_ERROR << "Failed to connect. error is 0";
      ChannelFactory::log_tls_errors();
      return OMS_FAILED;
    } else if (ret == 1) {
      OMS_DEBUG << "connect to server success";
      return OMS_OK;
    }
  }
  return OMS_FAILED;
}

int TlsChannel::read(char* buf, int size)
{
  ERR_clear_error();
  int ret = SSL_read(_ssl, buf, size);
  if (ret > 0) {
    return ret;
  }
  return handle_error(ret);
}

int TlsChannel::readn(char* buf, int size)
{
  ERR_clear_error();
  _last_error = 0;

  while (size > 0) {
    int ret = SSL_read(_ssl, buf, size);
    if (ret > 0) {
      size -= ret;
      buf += ret;
      continue;
    }

    _last_error = SSL_get_error(_ssl, ret);
    if (_last_error != SSL_ERROR_WANT_READ) {
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}

int TlsChannel::write(const char* buf, int size)
{
  ERR_clear_error();
  int ret = SSL_write(_ssl, buf, size);
  if (ret > 0) {
    return ret;
  }
  return handle_error(ret);
}

int TlsChannel::writen(const char* buf, int size)
{
  ERR_clear_error();
  _last_error = 0;

  while (size > 0) {
    int ret = SSL_write(_ssl, buf, size);
    if (ret > 0) {
      size -= ret;
      buf += ret;
      continue;
    }

    _last_error = SSL_get_error(_ssl, ret);
    if (_last_error != SSL_ERROR_WANT_WRITE) {
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}

int TlsChannel::handle_error(int ret)
{
  const int error = SSL_get_error(_ssl, ret);
  _last_error = error;
  if (error == SSL_ERROR_ZERO_RETURN) {
    return 0;
  }
  return -1;
}

const char* TlsChannel::last_error()
{
  switch (_last_error) {
    case SSL_ERROR_NONE:
      return "None";
    case SSL_ERROR_ZERO_RETURN:
      return "The TLS/SSL connection has been closed";
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return "The operation did not complete";
    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_WANT_ACCEPT:
      return "The operation did not complete";
    case SSL_ERROR_WANT_X509_LOOKUP:
      return "The TLS/SSL I/O function should be called again later";
    case SSL_ERROR_SYSCALL: {
      snprintf(_error_string, sizeof(_error_string), "System error:%s", strerror(errno));
      return _error_string;
    }

    case SSL_ERROR_SSL: {
      unsigned long error = ERR_get_error();
      ERR_error_string_n(error, _error_string, sizeof(_error_string));
      return _error_string;
    }
    default: {
      snprintf(_error_string, sizeof(_error_string), "Unknown error:%d", _last_error);
      return _error_string;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
ChannelFactory::ChannelFactory() : _channel_creator(nullptr)
{}

ChannelFactory::~ChannelFactory()
{
  if (_ssl_context != nullptr) {
    SSL_CTX_free(_ssl_context);
    _ssl_context = nullptr;
  }
}

int ChannelFactory::init(const Config& config)
{
  int ret = OMS_OK;
  const char* communication_mode = config.communication_mode.val().c_str();
  if (0 == strcasecmp(communication_mode, "server")) {
    _is_server_mode = true;
  } else if (0 == strcasecmp(communication_mode, "client")) {
    _is_server_mode = false;
  } else {
    OMS_ERROR << "Invalid config of communication_mode: " << communication_mode << ". can be one of server/client";
    return OMS_FAILED;
  }
  OMS_INFO << "ChannelFactory init with " << communication_mode << " mode";

  const char* channel_type = config.channel_type.val().c_str();
  if (0 == strcasecmp(channel_type, "plain")) {
    _channel_creator = &ChannelFactory::create_plain;
  } else if (0 == strcasecmp(channel_type, "tls")) {

    const char* ca_cert_file = config.tls_ca_cert_file.val().c_str();
    const char* cert_file = config.tls_cert_file.val().c_str();
    const char* key_file = config.tls_key_file.val().c_str();
    bool verify_peer = config.tls_verify_peer.val();
    ret = init_tls_context(ca_cert_file, cert_file, key_file, verify_peer);
    if (OMS_OK == ret) {
      _channel_creator = &ChannelFactory::create_tls;
    }
  } else {
    OMS_ERROR << "Unsupported channel type: " << channel_type;
    ret = OMS_FAILED;
  }
  return ret;
}

// static int index_of_channel_in_ssl = -1;

/**
 * The SSL will call this routine while verify a peer connection.
 * You can print some logs about the context
 * @param preverify_ok 1 for ok and 0 verify failed
 * @param ctx verify context
 * @return return 1 for verify ok and 0 verify failed
 */
static int verify_callback(int preverify_ok, X509_STORE_CTX* ctx)
{
  X509* cert = X509_STORE_CTX_get_current_cert(ctx);
  if (nullptr == cert) {
    return preverify_ok;
  }

  int error = X509_STORE_CTX_get_error(ctx);
  SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  if (nullptr == ssl) {
    return preverify_ok;
  }
  if (1 != preverify_ok) {
    char buf[256];
    X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
    OMS_ERROR << "verify failed. error " << error << ":" << X509_verify_cert_error_string(error)
              << ", subject name:" << buf;

    if (error == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) {
      X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
      OMS_ERROR << "issuer:" << buf;
    }
  }

  return preverify_ok;
  /**
  TlsChannel* channel = static_cast<TlsChannel*>(SSL_get_ex_data(ssl));
  if (nullptr == channel) {
    return preverify_ok;
  }

  return channel->verify(preverify_ok, ctx);
  */
}

int ChannelFactory::init_tls_context(
    const char* ca_cert_file, const char* cert_file, const char* key_file, bool verify_peer)
{
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  // const SSL_METHOD * ssl_method = TLS_server_method(); // openssl version 1.1.1 supported
  const SSL_METHOD* ssl_method = SSLv23_method();  // TLSv1.3 not supptorted

  SSL_CTX* ssl_ctx = SSL_CTX_new(ssl_method);
  if (nullptr == ssl_ctx) {
    OMS_ERROR << "Failed to create SSL_CTX";
    return OMS_FAILED;
  }

  int mode = verify_peer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE;
  SSL_CTX_set_verify(ssl_ctx, mode, verify_callback);
  int ret = SSL_CTX_load_verify_locations(ssl_ctx, ca_cert_file, nullptr);
  if (ret != 1) {
    OMS_ERROR << "SSL_CTX_load_verify_locations failed. ca cert file: '" << ca_cert_file << '\'';
    log_tls_errors();
    SSL_CTX_free(ssl_ctx);
    return OMS_FAILED;
  }
  ret = SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM);
  if (ret != 1) {
    OMS_ERROR << "SSL_CTX_use_certificate_file failed. cert file: '" << cert_file << '\'';
    log_tls_errors();
    SSL_CTX_free(ssl_ctx);
    return OMS_FAILED;
  }
  ret = SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM);
  if (ret != 1) {
    OMS_ERROR << "SSL_CTX_use_PrivateKey_file failed. key file: '" << key_file << '\'';
    log_tls_errors();
    SSL_CTX_free(ssl_ctx);
    return OMS_FAILED;
  }

  ret = SSL_CTX_check_private_key(ssl_ctx);
  if (ret != 1) {
    OMS_ERROR << "SSL_CTX_check_private_key failed. ca cert file: '" << ca_cert_file << "', cert file: '" << cert_file
              << "', key file: '" << key_file << '\'';
    log_tls_errors();
    SSL_CTX_free(ssl_ctx);
    return OMS_FAILED;
  }

  _ssl_context = ssl_ctx;
  return OMS_OK;
}

void ChannelFactory::log_tls_errors()
{
  unsigned long error = ERR_get_error();
  char buf[256];
  while (error != 0) {
    ERR_error_string_n(error, buf, sizeof(buf));
    OMS_ERROR << buf;
    error = ERR_get_error();
  }
}

Channel* ChannelFactory::create(const PeerInfo& peer)
{
  Channel* ch = (this->*_channel_creator)(peer);
  if (nullptr == ch) {
    return nullptr;
  }

  int ret = OMS_OK;
  if (_is_server_mode) {
    ret = ch->after_accept();
  } else {
    ret = ch->after_connect();
  }

  if (ret != OMS_OK) {
    ch->release(true);
    ch = nullptr;
  }
  return ch;
}

Channel* ChannelFactory::create_plain(const PeerInfo& peer)
{
  OMS_INFO << "create a plain channel of peer: " << peer.id();
  return new PlainChannel(peer);
}

Channel* ChannelFactory::create_tls(const PeerInfo& peer)
{
  TlsChannel* channel = new (std::nothrow) TlsChannel(peer);
  if (channel == nullptr) {
    return nullptr;
  }
  int ret = channel->init_tls(_ssl_context);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to init tls channel";
    channel->release(true);
    channel = nullptr;
  }
  OMS_INFO << "Create a tls channel of peer: " << peer.id();
  return channel;
}

}  // namespace logproxy
}  // namespace oceanbase
