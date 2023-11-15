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

#include <fcntl.h>
#include "binlog_server.h"
#include "config.h"
#include "log.h"
#include "file_gc.h"
#include "fork_thread.h"
#include "env.h"
#include "event_dispatch.h"
#include "binlog_state_machine.h"
#include "metric/sys_metric.h"
#include "metric/status_thread.h"

namespace oceanbase {
namespace binlog {
static oceanbase::logproxy::Config& s_config = oceanbase::logproxy::Config::instance();

int BinlogServer::run_foreground()
{
  uint16_t sys_var_listen_port = s_config.service_port.val();
  uint32_t sys_var_nof_work_threads = s_config.binlog_nof_work_threads.val();
  uint32_t sys_var_bc_work_threads = s_config.binlog_bc_work_threads.val();
  env_init(sys_var_nof_work_threads, sys_var_bc_work_threads);

  // pull up all BC processes
  start_owned_binlog_converters();

  struct event_base* ev_base = event_base_new();
  struct evconnlistener* listener = evconnlistener_new(ev_base, sys_var_listen_port, on_new_connection_cb);
  if (listener == nullptr) {
    OMS_STREAM_ERROR << "Failed to start OceanBase binlog server on port " << sys_var_listen_port;
    event_base_free(ev_base);
    return OMS_FAILED;
  }
  fcntl(evconnlistener_get_fd(listener), F_SETFD, FD_CLOEXEC);
  OMS_STREAM_INFO << "Start OceanBase binlog server on port " << sys_var_listen_port;
  logproxy::StatusThread status_thread(logproxy::g_metric, logproxy::g_proc_metric);
  if (s_config.metric_enable.val()) {
    status_thread.start();
    status_thread.detach();
  }

  event_base_dispatch(ev_base);
  OMS_STREAM_INFO << "Stop OceanBase binlog server";

  evconnlistener_free(listener);
  event_base_free(ev_base);
  env_deInit();

  return OMS_OK;
}

void BinlogServer::start_owned_binlog_converters()
{
  OMS_STREAM_INFO << "Start pull up all BC processes";
  std::vector<StateMachine*> state_machines;
  g_state_machine->fetch_state_vector(get_default_state_file_path(), state_machines);
  std::map<std::string, bool> pulled_up;

  for (auto state_machine : state_machines) {
    if (state_machine->get_converter_state() == INIT || state_machine->get_converter_state() == RUNNING) {
      // Determine whether the process exists, and if it exists, it will not be pulled repeatedly
      if (state_machine->get_pid() > 0 && 0 == kill(state_machine->get_pid(), 0)) {

        OMS_STREAM_INFO << "The current binlog converter [" << state_machine->get_cluster() << ","
                        << state_machine->get_tenant() << "]"
                        << "is alive and the pull action is terminated";
        pulled_up[state_machine->get_unique_id()] = true;
        continue;
      }

      if (pulled_up.find(state_machine->get_unique_id()) == pulled_up.end()) {
        g_bc_executor->submit(logproxy::start_binlog_converter, state_machine->get_config());
        pulled_up[state_machine->get_unique_id()] = true;
      }
    }
  }
  OMS_STREAM_INFO << "Finish to pull up " << pulled_up.size() << " BC processes";
  logproxy::release_vector(state_machines);
}

}  // namespace binlog
}  // namespace oceanbase
