#include "sys/socket.h"
#include "netinet/in.h"

#include "gtest/gtest.h"
#include "communication/io.h"

using namespace oceanbase::logproxy;

// ASAN_OPTIONS=alloc_dealloc_mismatch=0 ./test_base --gtest_filter=NET.fork_fd
TEST(NET, fork_fd)
{
  int port = 9111;
  int listen_fd = listen(nullptr, port, false, true);
  if (listen_fd == -1) {
    OMS_ERROR << "Failed to listen";
    return;
  }

  OMS_INFO << "Listen " << port << " with fd: " << listen_fd;

  std::thread thd1 = std::thread([&] {
    while (true) {

      struct sockaddr_in peer_addr;
      socklen_t peer_addr_size = sizeof(struct sockaddr_in);
      int connfd = accept(listen_fd, (struct sockaddr*)&peer_addr, &peer_addr_size);
      if (connfd <= 0) {
        sleep(1);
        continue;
      }

      OMS_INFO << "Accepted fd: " << connfd;

      int ret = fork();
      if (ret < 0) {
        OMS_ERROR << "Failed to fork";
        return;
      }

      if (ret == 0) {
        close(listen_fd);

        OMS_INFO << "Child: " << getpid() << " working";
        sleep(300);
        close(connfd);

      } else {
        close(connfd);
        OMS_INFO << "Parent close fd: " << connfd;
      }

      break;
    }
  });

  thd1.join();
}