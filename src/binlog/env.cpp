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

#include "env.h"
#include "binlog_state_machine.h"

namespace oceanbase {
namespace binlog {
ConnectionManager* g_connection_manager = nullptr;
ThreadPoolExecutor* g_executor = nullptr;
ThreadPoolExecutor* g_bc_executor = nullptr;
SysVar* g_sys_var = nullptr;
StateMachineManager* g_state_machine = nullptr;

int env_init(uint32_t nof_work_threads, uint32_t bc_work_threads)
{
  g_connection_manager = new ConnectionManager{};
  g_executor = new ThreadPoolExecutor{nof_work_threads};
  g_bc_executor = new ThreadPoolExecutor{bc_work_threads};
  g_sys_var = new SysVar{};
  g_state_machine = new StateMachineManager{};
  return evthread_use_pthreads();
};

void env_deInit()
{
  delete g_executor;
  delete g_connection_manager;
  delete g_sys_var;
  delete g_bc_executor;
  delete g_state_machine;
}

}  // namespace binlog
}  // namespace oceanbase
