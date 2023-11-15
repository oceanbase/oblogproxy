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

#include "str.h"
#include "shell_executor.h"
namespace oceanbase {
namespace logproxy {
int exec_cmd(const std::string& cmd, std::string& result)
{
  FILE* fp = popen(cmd.c_str(), "r");
  if (fp == nullptr) {
    OMS_STREAM_ERROR << "Failed to exec command:" << cmd << ", errno:" << errno << ", error:" << strerror(errno);
    return -1;
  }

  char buff[1024];
  result.clear();
  while (fgets(buff, sizeof(buff), fp) != nullptr) {
    buff[1023] = '\0';
    result += buff;
  }
  int ret = pclose(fp);
  ret = WEXITSTATUS(ret);
  //  OMS_STREAM_DEBUG << "bash result code:" << ret << "(" << strerror(errno) << ")"
  //            << ", content: " << result;
  // we set SIGCHILD to SIG_IGN cause pclose got an ECHILD errno
  return (ret == 255 && errno == ECHILD) ? 0 : ret;
}

int exec_cmd(const std::string& cmd, std::vector<std::string>& lines, bool omit_empty_line)
{
  FILE* fp = popen(cmd.c_str(), "r");
  if (fp == nullptr) {
    OMS_STREAM_ERROR << "Failed to exec command:" << cmd << ", errno:" << errno << ", error:" << strerror(errno);
    return -1;
  }

  lines.clear();
  ssize_t linelen = 1024;
  size_t linecap = linelen;
  char* lineptr = (char*)malloc(linelen);
  while ((linelen = ::getline(&lineptr, &linecap, fp)) > 0) {
    std::string line(lineptr, linelen);
    if (omit_empty_line) {
      trim(line);
      if (line.empty()) {
        continue;
      }
    }
    lines.emplace_back(line);
  }

  free(lineptr);

  int ret = pclose(fp);
  ret = WEXITSTATUS(ret);
  return (ret == 255 && errno == ECHILD) ? 0 : ret;
}
}  // namespace logproxy
}  // namespace oceanbase