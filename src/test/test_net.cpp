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

#include <thread>
#include "sys/socket.h"
#include "netinet/in.h"

#include "gtest/gtest.h"
#include "communication/io.h"
#include "log.h"

using namespace oceanbase::logproxy;

// ASAN_OPTIONS=alloc_dealloc_mismatch=0 ./test_base --gtest_filter=NET.fork_fd
TEST(NET, fork_fd)
{
  GTEST_SKIP();
  int port = 9111;
  int listen_fd = listen(nullptr, port, false, true);
  if (listen_fd == -1) {
    OMS_STREAM_ERROR << "Failed to listen";
    return;
  }

  OMS_STREAM_INFO << "Listen " << port << " with fd: " << listen_fd;

  std::thread thd1 = std::thread([&] {
    while (true) {
      struct sockaddr_in peer_addr;
      socklen_t peer_addr_size = sizeof(struct sockaddr_in);
      int connfd = accept(listen_fd, (struct sockaddr*)&peer_addr, &peer_addr_size);
      if (connfd <= 0) {
        sleep(1);
        continue;
      }

      OMS_STREAM_INFO << "Accepted fd: " << connfd;

      int ret = fork();
      if (ret < 0) {
        OMS_STREAM_ERROR << "Failed to fork";
        return;
      }

      if (ret == 0) {
        close(listen_fd);

        OMS_STREAM_INFO << "Child: " << getpid() << " working";
        sleep(300);
        close(connfd);
      } else {
        close(connfd);
        OMS_STREAM_INFO << "Parent close fd: " << connfd;
      }

      break;
    }
  });

  thd1.join();
}