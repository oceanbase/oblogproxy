/**
 * Copyright (c) 2023 OceanBase
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

#include <cassert>
#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>

namespace oceanbase {
namespace binlog {
class SocketUtil {
public:
  static int get_socket_local_ip_port(
      int sock, std::string& local_ip, uint16_t& local_port, const struct sockaddr* local_addr_hint = nullptr)
  {
    return get_sockaddr_ip_port_helper(sock, local_ip, local_port, getsockname, local_addr_hint);
  }

  static int get_socket_peer_ip_port(
      int sock, std::string& peer_ip, uint16_t& peer_port, const struct sockaddr* peer_addr_hint = nullptr)
  {
    return get_sockaddr_ip_port_helper(sock, peer_ip, peer_port, getpeername, peer_addr_hint);
  }

private:
  static int get_sockaddr_ip_port_helper(int sock, std::string& ip, uint16_t& port,
      decltype(getsockname) get_socket_addr_func, const struct sockaddr* addr_hint = nullptr)
  {
    if (nullptr != addr_hint) {
      return sockaddr_to_ip_port(addr_hint, ip, port);
    }
    struct sockaddr_storage addr_storage;
    memset(&addr_storage, 0, sizeof(addr_storage));
    auto* addr = reinterpret_cast<sockaddr*>(&addr_storage);
    socklen_t addr_len = sizeof(addr_storage);
    if (get_socket_addr_func(sock, addr, &addr_len)) {
      return -1;
    }
    return sockaddr_to_ip_port(addr, ip, port);
  }

  static int sockaddr_to_ip_port(const struct sockaddr* addr, std::string& ip, uint16_t& port)
  {
    assert(AF_INET == addr->sa_family || AF_INET6 == addr->sa_family);
    if (AF_INET == addr->sa_family) {
      return sockaddr4_to_ip_port(reinterpret_cast<const struct sockaddr_in*>(addr), ip, port);
    }
    return sockaddr6_to_ip_port(reinterpret_cast<const struct sockaddr_in6*>(addr), ip, port);
  }

  static int sockaddr4_to_ip_port(const struct sockaddr_in* addr, std::string& ip, uint16_t& port)
  {
    char ip_buff[INET_ADDRSTRLEN];
    if (nullptr == inet_ntop(AF_INET, &addr->sin_addr, ip_buff, sizeof(ip_buff))) {
      return -1;
    }
    ip = ip_buff;
    port = ntohs(addr->sin_port);
    return 0;
  }

  static int sockaddr6_to_ip_port(const struct sockaddr_in6* addr, std::string& ip, uint16_t& port)
  {
    char ip_buff[INET6_ADDRSTRLEN];
    if (nullptr == inet_ntop(AF_INET6, &addr->sin6_addr, ip_buff, sizeof(ip_buff))) {
      return -1;
    }
    ip = ip_buff;
    port = ntohs(addr->sin6_port);
    return 0;
  }
};

}  // namespace binlog
}  // namespace oceanbase
