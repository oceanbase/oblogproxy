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

#include "thread_pool_executor.h"

namespace oceanbase {
namespace binlog {
ThreadPoolExecutor::ThreadPoolExecutor(unsigned int nof_threads)
    : work_threads_(nof_threads), nof_running_tasks_(0), shutdown_(false)
{
  for (auto& work_thread : work_threads_) {
    work_thread = std::thread([=]() { process_task_loop(); });
  }
}

ThreadPoolExecutor::~ThreadPoolExecutor()
{
  {
    std::unique_lock<std::mutex> lock(mutex_);
    shutdown_ = true;
  }
  cv_.notify_all();
  for (auto& work_thread : work_threads_) {
    work_thread.join();
  }
}

void ThreadPoolExecutor::process_task_loop()
{
  while (true) {
    Task task;

    // fetch the next task to run
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [=]() { return !pending_tasks_.empty() || (shutdown_ && (nof_running_tasks_ == 0)); });
      if (pending_tasks_.empty()) {  // i.e. (shutdown_ == true) && (nof_running_tasks_ == 0)
        break;
      }
      task = std::move(pending_tasks_.front());
      pending_tasks_.pop();
      ++nof_running_tasks_;
    }

    // run the fetched task
    task();

    {
      std::unique_lock<std::mutex> lock(mutex_);
      --nof_running_tasks_;
    }
  }

  // notify all other threads to exit the loop
  cv_.notify_all();
}

}  // namespace binlog
}  // namespace oceanbase
