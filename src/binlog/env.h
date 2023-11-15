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

#include "connection_manager.h"
#include "sys_var.h"
#include "thread_pool_executor.h"
#include "metric/sys_metric.h"
#include "binlog_state_machine.h"

namespace oceanbase {
namespace binlog {
extern ConnectionManager* g_connection_manager;
extern ThreadPoolExecutor* g_executor;
extern ThreadPoolExecutor* g_bc_executor;
extern SysVar* g_sys_var;
extern StateMachineManager* g_state_machine;

int env_init(uint32_t nof_work_threads, uint32_t bc_work_threads);

void env_deInit();

}  // namespace binlog
}  // namespace oceanbase
