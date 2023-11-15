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

#include <future>
#include <thread>
#include <queue>
#include <functional>

namespace oceanbase {
namespace binlog {
class ThreadPoolExecutor {
public:
  explicit ThreadPoolExecutor(unsigned int nof_threads = std::thread::hardware_concurrency());

  ~ThreadPoolExecutor();

  template <class Fn, class... Args>
  auto submit(Fn&& fn, Args&&... args) -> std::future<std::invoke_result_t<Fn, Args...>>
  {
    using FnResT = std::invoke_result_t<Fn, Args...>;
    using PackagedTask = std::packaged_task<FnResT()>;
    auto p_packaged_task = std::make_shared<PackagedTask>(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
    std::future<FnResT> fut = p_packaged_task->get_future();
    Task task{[p_packaged_task]() { (*p_packaged_task)(); }};

    std::lock_guard<std::mutex> lock(mutex_);
    pending_tasks_.push(task);
    cv_.notify_one();

    return fut;
  }

private:
  using Task = std::function<void()>;

private:
  void process_task_loop();

private:
  std::vector<std::thread> work_threads_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<Task> pending_tasks_;
  unsigned int nof_running_tasks_;
  bool shutdown_;  // when ~ThreadPoolExecutor() is called, set it to true to make work threads exit.
};

}  // namespace binlog
}  // namespace oceanbase
