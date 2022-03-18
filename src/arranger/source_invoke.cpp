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
#include <sys/wait.h>
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

class ForkThread : public Thread {
public:
  ForkThread(Communicator& comm, const ClientMeta& client, const std::string& config)
      : Thread("ForkThread"), _comm(comm), _client(client), _config(config)
  {}

  void run() override
  {
    // fork oblogreader
    int pid = fork();
    if (pid == -1) {
      OMS_ERROR << "failed to fork: " << strerror(errno);
      set_ret(OMS_FAILED);
      return;
    }

    char* process_name_addr = (char*)Config::instance().process_name_address.val();
    if (pid == 0) {  // children
#ifdef linux
      const char* child_process_name = "oblogreader";
      ::prctl(PR_SET_NAME, (unsigned long)child_process_name);
#endif
      strncpy(process_name_addr, "oblogreader", strlen(process_name_addr));

      // change process context dir path
      std::string process_path = Config::instance().oblogreader_path.val() + std::string("/") + _client.id.to_string();
      FsUtil::mkdir(process_path);
      FsUtil::mkdir(process_path + "/log");
      ::chdir(process_path.c_str());

      // reload log
      init_log("oblogreader", true);

      OMS_INFO << "!!! Started oblogreader process(" << getpid() << ") with peer: " << _client.peer.to_string()
               << ", client meta:" << _client.to_string();

      _comm.fork_after();
      _comm.close_listen();

      Channel* ch = _comm.get_channel(_client.peer);
      if (ch == nullptr) {
        OMS_ERROR << "Failed to get channel of client: " << _client.peer.to_string() << ". Exiting";
        ::exit(-1);
      }

      _comm.clear_channels();
      _comm.stop();

      ObLogReader reader;
      OblogConfig oblog_config(_config);
      if (!oblog_config.sys_user.empty()) {
        oblog_config.user.set(oblog_config.sys_user.val());
      } else {
        oblog_config.user.set(Config::instance().ob_sys_username.val());
      }
      if (!oblog_config.sys_password.empty()) {
        oblog_config.password.set(oblog_config.sys_password.val());
      } else {
        oblog_config.password.set(Config::instance().ob_sys_password.val());
      }
      int ret = reader.init(_client.id.get(), _client.packet_version, ch, oblog_config);
      if (ret == OMS_OK) {
        reader.start();
        reader.join();
      }

      // !!!IMPORTANT!!! we don't quit current thread which work as child process's main thread
      // we IGNORE other context inheriting from parent process

      OMS_WARN << "!!! Exiting oblogreader process(" << getpid() << ") with peer: " << _client.peer.to_string()
               << ", client meta:" << _client.to_string();
      ::exit(-1);

    } else {  // parent;

      OMS_INFO << "+++ create oblogreader with pid: " << pid;
      SourceWaiter::instance().add(pid, _client);
    }
  }

private:
  Communicator& _comm;

  // copy for child process
  ClientMeta _client;
  std::string _config;
};

static int start_oblogreader(Communicator& communicator, const ClientMeta& client, const std::string& configuration)
{
  communicator.fork_prepare();

  // we create new thread for fork() acting as children process's main thread
  ForkThread fork_thd(communicator, client, configuration);
  fork_thd.start();
  int ret = fork_thd.join();

  communicator.fork_after();
  communicator.remove_channel(client.peer);  // remove fd event for parent;

  return ret;
}

int SourceInvoke::invoke(Communicator& communicator, const ClientMeta& client, const std::string& configuration)
{
  switch (client.type) {
    case OCEANBASE:
      return start_oblogreader(communicator, client, configuration);

    default:
      OMS_ERROR << "Unsupported invoke logtype: " << client.type;
      return OMS_FAILED;
  }
}

void SourceWaiter::add(int pid, const ClientMeta& client)
{
  std::lock_guard<std::mutex> lg(_op_lock);

  WaitThread* thd = new WaitThread(pid, client);
  _childs.emplace(pid, thd);
  _childs.at(pid)->start();
  _childs.at(pid)->detach();
}

void SourceWaiter::remove(int pid)
{
  std::lock_guard<std::mutex> lg(_op_lock);
  auto entry = _childs.find(pid);
  if (entry != _childs.end()) {
    _childs.erase(pid);
  }
  OMS_WARN << "--- SourceWaiter remove oblogreader for pid: " << pid;
}

SourceWaiter::WaitThread::WaitThread(int pid, const ClientMeta& client)
    : Thread("WaiterThread for pid:" + std::to_string(pid)), _pid(pid), _client(client)
{}

void SourceWaiter::WaitThread::run()
{
  int retval = OMS_OK;
  waitpid(_pid, &retval, 0);
  OMS_WARN << "--- oblogreader exit with ret: " << retval << ", try to close fd: " << _client.peer.file_desc;
  if (retval != OMS_OK) {
    // TODO... response to client with _client.channel
  }

  shutdown(_client.peer.file_desc, SHUT_RDWR);

  // use a thread to remove avoid join dead lock
  Arranger::instance().close_client(_client);
  SourceWaiter::instance().remove(_pid);

  OMS_WARN << "--- oblogreader WaiterThread(" << tid() << ") exit for pid: " << _pid;

  // suicide free mem
  delete this;
}

}  // namespace logproxy
}  // namespace oceanbase
