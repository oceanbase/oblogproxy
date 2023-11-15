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

#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include <netdb.h>
#include <poll.h>

#include "communication/io.h"
#include "log.h"
#include "common.h"

namespace oceanbase {
namespace logproxy {
int writen(int fd, const void* buf, int size)
{
  const char* tmp = (const char*)buf;
  while (size > 0) {
    //    OMS_STREAM_INFO << "about to write, fd: " << fd << ", size: " << size;
    const ssize_t ret = ::write(fd, tmp, size);
    //    OMS_STREAM_INFO << "done to write, fd:" << fd << ", size: " << size << ", ret:" << ret << ", errno:" << errno;
    if (ret >= 0) {
      tmp += ret;
      size -= ret;
      continue;
    }

    const int err = errno;
    if (EAGAIN != err && EINTR != err) {
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}

int readn(int fd, void* buf, int size)
{
  char* tmp = (char*)buf;
  while (size > 0) {
    //    OMS_STREAM_INFO << "about to read, fd: " << fd << ", size: " << size;
    const ssize_t ret = ::read(fd, tmp, size);
    //    OMS_STREAM_INFO << "done to read, fd:" << fd << ", size: " << size << ", ret:" << ret << ", errno:" << errno;
    if (ret > 0) {
      tmp += ret;
      size -= ret;
      continue;
    }

    if (0 == ret) {
      return OMS_FAILED;  // end of file
    }

    const int err = errno;
    if (EAGAIN != err && EINTR != err) {
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}

int connect(const char* host, int port, bool block_mode, int timeout, int& sockfd)
{
  if (nullptr == host || 0 == host[0] || port < 0 || port >= 65536) {
    OMS_STREAM_ERROR << "Invalid host or port";
    return OMS_FAILED;
  }

  struct hostent* hostent = gethostbyname(host);
  if (hostent == nullptr) {
    OMS_STREAM_ERROR << "Failed to get host by name(" << host << "). error=" << strerror(errno);
    return OMS_FAILED;
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = PF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr = *((struct in_addr*)hostent->h_addr);

  const int sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    OMS_STREAM_ERROR << "Failed to create socket. error=" << strerror(errno);
    return OMS_FAILED;
  }

  int ret = OMS_OK;
  if (!block_mode) {
    ret = set_non_block(sock);
    if (ret != 0) {
      close(sock);
      return OMS_FAILED;
    }
  }

  ret = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (ret != 0) {
    if (!block_mode && (errno == EINPROGRESS || errno == EAGAIN)) {
      struct pollfd pollfd;
      pollfd.fd = sock;
      pollfd.events = POLLERR | POLLHUP | POLLOUT;
      pollfd.revents = 0;

      ret = poll(&pollfd, 1, timeout);
      if (ret == 1 && (pollfd.revents & POLLOUT)) {
        // connect success
        OMS_STREAM_DEBUG << "Connect to server success after poll. host=" << host << ",port=" << port;
      } else {
        OMS_STREAM_ERROR << "Failed to connect to server. host=" << host << ",port=" << port
                  << ". timeout:" << (bool)(ret == 0) << ", error=" << strerror(errno);
        close(sock);
        return OMS_CONNECT_FAILED;
      }
    } else {
      OMS_STREAM_ERROR << "Failed to connect to server. host=" << host << ",port=" << port << ". error=" << strerror(errno);
      close(sock);
      return OMS_CONNECT_FAILED;
    }
  }

  sockfd = sock;
  return OMS_OK;
}

int listen(const char* host, int port, bool block_mode, bool reuse_address)
{
  if (port < 0 || port > 65536) {
    OMS_STREAM_ERROR << "Invalid listen port: " << port;
    return OMS_FAILED;
  }

  struct in_addr bind_addr;
  bind_addr.s_addr = INADDR_ANY;
  if (host != nullptr) {
    struct hostent* hostent = gethostbyname(host);
    if (nullptr == hostent) {
      OMS_STREAM_ERROR << "gethostbyname return failed. address=" << host << ", error=" << strerror(errno);
      return OMS_FAILED;
    }
    bind_addr = *(struct in_addr*)hostent->h_addr;
  } else {
    host = "(not-set)";  // print log
  }

  struct sockaddr_in sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = PF_INET;
  sockaddr.sin_port = htons(port);
  sockaddr.sin_addr = bind_addr;

  int sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    OMS_STREAM_ERROR << "Failed to create socket. error=" << strerror(errno);
    return OMS_FAILED;
  }

  int ret = OMS_OK;
  if (reuse_address) {
    ret = set_reuse_addr(sock);
    if (ret != OMS_OK) {
      shutdown(sock, SHUT_RDWR);
      ::close(sock);
      return ret;
    }
  }

  ret = bind(sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to bind address '" << host << ":" << port << "', error: " << strerror(errno);
    shutdown(sock, SHUT_RDWR);
    ::close(sock);
    return OMS_FAILED;
  }

  ret = ::listen(sock, 10);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to listen on '" << host << ':' << port << "', error: " << strerror(errno);
    shutdown(sock, SHUT_RDWR);
    ::close(sock);
    return OMS_FAILED;
  }

  if (!block_mode) {
    ret = set_non_block(sock);
    if (ret != OMS_OK) {
      OMS_STREAM_ERROR << "Failed to set listen socket non-block mode, error: " << strerror(errno);
      shutdown(sock, SHUT_RDWR);
      ::close(sock);
      return OMS_FAILED;
    }
  }

  ret = set_close_on_exec(sock);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to set listen socket close on exec mode, error: " << strerror(errno);
    shutdown(sock, SHUT_RDWR);
    ::close(sock);
    return OMS_FAILED;
  }

  return sock;
}

int set_reuse_addr(int sock)
{
  int opt = 1;
  int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (ret != 0) {
    OMS_STREAM_ERROR << "Failed to set socket in 'REUSE_ADDR' mode. error=" << strerror(errno);
    return OMS_FAILED;
  }
  return OMS_OK;
}

int set_non_block(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    OMS_STREAM_WARN << "Failed to get flags of fd(" << fd << "). error=" << strerror(errno);
    return OMS_FAILED;
  }

  flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (flags == -1) {
    OMS_STREAM_WARN << "Failed to set non-block flags of fd(" << fd << "). error=" << strerror(errno);
    return OMS_FAILED;
  }
  return OMS_OK;
}

int set_close_on_exec(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    OMS_STREAM_WARN << "Failed to get flags of fd(" << fd << "). error=" << strerror(errno);
    return OMS_FAILED;
  }

  flags = fcntl(fd, F_SETFL, flags | O_CLOEXEC);
  if (flags == -1) {
    OMS_STREAM_WARN << "Failed to set close on exec flags of fd(" << fd << "). error=" << strerror(errno);
    return OMS_FAILED;
  }
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
