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

#include <unistd.h>
#include <signal.h>
#ifdef linux
#include <sys/prctl.h>
#endif
#include "common/log.h"
#include "common/config.h"
#include "common/fs_util.h"
#include "oblogreader/oblogreader.h"
#include "arranger/source_invoke.h"
#include "arranger/arranger.h"

namespace oceanbase {
namespace logproxy {

static int start_oblogreader(Comm& comm, const ClientMeta& client, const OblogConfig& config)
{
  int pid = fork();
  if (pid == -1) {
    OMS_ERROR << "failed to fork: " << strerror(errno);
    return OMS_FAILED;
  }

  if (pid == 0) {  // children
    char* process_name_addr = (char*)Config::instance().process_name_address.val();
    const char* child_process_name = "oblogreader";
#ifdef linux
    ::prctl(PR_SET_NAME, (unsigned long)child_process_name);
#endif
    strncpy(process_name_addr, child_process_name, strlen(process_name_addr));

    // change process context dir path
    std::string process_path = Config::instance().oblogreader_path.val() + std::string("/") + client.id;
    FsUtil::mkdir(process_path);
    FsUtil::mkdir(process_path + "/log");
    ::chdir(process_path.c_str());

    // reload log
    init_log(child_process_name, true);

    OMS_INFO << "!!! Started oblogreader process(" << getpid() << ") with peer: " << client.peer.to_string()
             << ", client meta:" << client.to_string();

    comm.stop(client.peer.fd);

    // we create new thread for fork() acting as children process's main thread
    // child never return as exit internal if error
    ObLogReader reader;
    int ret = reader.init(client.id, client.packet_version, client, config);
    if (ret == OMS_OK) {
      reader.start();
      reader.join();
    }

    // !!!IMPORTANT!!! we don't quit current thread which work as child process's main thread
    // we IGNORE other context inheriting from parent process

    OMS_WARN << "!!! Exiting oblogreader process(" << getpid() << ") with peer: " << client.peer.to_string()
             << ", client meta:" << client.to_string();
    ::exit(-1);
  }

  OMS_INFO << "+++ Created oblogreader with pid: " << pid;
  return pid;
}

/**
 * @return pid of childern process or -1 failurs, childern process never return
 */
int SourceInvoke::invoke(Comm& comm, const ClientMeta& client, const OblogConfig& config)
{
  switch (client.type) {
    case OCEANBASE:
      return start_oblogreader(comm, client, config);

    default:
      OMS_ERROR << "Unsupported invoke logtype: " << client.type;
      return OMS_FAILED;
  }
}

}  // namespace logproxy
}  // namespace oceanbase
