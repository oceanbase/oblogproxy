// Copyright (c) 2021 OceanBase
// OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at:
//           http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

#include "common/shell_executor.h"
namespace oceanbase {
namespace logproxy {

int exec_cmd(const std::string& cmd, std::string& result)
{
  FILE* fd = popen(cmd.c_str(), "r");
  if (fd == nullptr) {
    OMS_ERROR << "Failed to exec command:" << cmd << ", errno:" << errno << ", error:" << strerror(errno);
    return -1;
  }

  char buff[1024];
  result.clear();
  while (fgets(buff, sizeof(buff), fd) != nullptr) {
    buff[1023] = '\0';
    result += buff;
  }
  int ret = pclose(fd);
  ret = WEXITSTATUS(ret);
  //  OMS_DEBUG << "bash result code:" << ret << "(" << strerror(errno) << ")"
  //            << ", content: " << result;
  // we set SIGCHILD to SIG_IGN cause pclose got an ECHILD errno
  return (ret == 255 && errno == ECHILD) ? 0 : ret;
}

}  // namespace logproxy
}  // namespace oceanbase