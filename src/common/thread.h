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

#pragma once

#include <pthread.h>
#include <string>
#include <atomic>

namespace oceanbase {
namespace logproxy {
class Thread {
public:
  explicit Thread(std::string name = "");

  virtual ~Thread();

  virtual void stop();

  /**
   * @return thread-defined value, default is 0
   */
  int join();

  void start();

  bool is_run();

  void set_run(bool run_flag);

  void detach();

  inline uint32_t tid() const
  {
    return _tid;
  }

  /*!
   * @brief Preset the detach state，
   * true means that the thread is set to detach when it is created, and false means that the thread is set to joinable
   * when it is created
   */
  void set_detach_state(bool is_detach);

  /*!
   * @brief Set whether the current thread is released immediately after execution, the default is false.
   * Setting it to release immediately will release the thread object resource directly after run execution
   * @param direct_release
   */
  void set_release_state(bool direct_release);

  bool is_direct_release();

protected:
  virtual void run() = 0;

  void set_ret(int ret);

private:
  static void* _thd_rotine(void* arg);

  std::string _name;
  uint32_t _tid;
  pthread_t _thd;
  std::atomic<bool> _run_flag = false;
  int _ret = 0;
  bool _is_detach = false;
  bool _direct_release = false;

  static volatile int _s_tid_idx;
};

}  // namespace logproxy
}  // namespace oceanbase
