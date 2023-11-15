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

#include <sys/resource.h>
#include "environmental.h"
#include "log.h"
namespace oceanbase {
namespace logproxy {

void print_env_info()
{

  /*!
   * @brief  get the maximum number of handles
   */
  struct rlimit limit;
  if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
    OMS_INFO("Max file descriptors: {}", limit.rlim_cur);
  }

  /*!
   * @brief get the maximum number of threads
   */
  if (getrlimit(RLIMIT_NPROC, &limit) == 0) {
    OMS_INFO("Max processes/threads: {}", limit.rlim_cur);
  }

  /*!
   * @brief get core dump configuration
   */
  if (getrlimit(RLIMIT_CORE, &limit) == 0) {
    OMS_INFO("Core dump size: {}", limit.rlim_cur);
  }

  /*!
   * @brief Maximum number of pending signals
   */
  if (getrlimit(RLIMIT_SIGPENDING, &limit) == 0) {
    OMS_INFO("Maximum number of pending signals: {}", limit.rlim_cur);
  }
}

}  // namespace logproxy
}  // namespace oceanbase