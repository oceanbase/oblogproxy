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

namespace oceanbase {
namespace logproxy {

int writen(int fd, const void* buf, int size);

int readn(int fd, void* buf, int size);

/**
 * create a socket and connect to remote server
 * @param host the hostname of remote server
 * @param port the port of server
 * @param block_mode the blocking mode of socket
 * @param timeout The timeout of connect. It will worked only when `block_mode=false`
 *								-1 means infinite, 0 means no wait, others means the time to wait, in millisecond
 * @param sockfd[out] The socket file descriptor if the connection is OK.
 * @return OMS_OK if connect success
 */
int connect(const char* host, int port, bool block_mode, int timeout, int& sockfd);

/**
 * create a socket, bind the address and listen on it
 * @param host The hostname to bind. If null, then bind ANY_ADDRESS
 * @param port The port to bind
 * @param block_mode the blocking mode of socket
 * @param reuse_address Whether to set option SO_REUSEADDR
 * @return sockfd If success, return the socket descriptor
 */
int listen(const char* host, int port, bool block_mode, bool reuse_address);

int set_reuse_addr(int fd);
int set_non_block(int fd);
int set_close_on_exec(int fd);

}  // namespace logproxy
}  // namespace oceanbase
