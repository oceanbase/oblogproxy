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

namespace oceanbase {
namespace logproxy {

class Thread {
public:
  explicit Thread(const std::string& name = "");

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

protected:
  virtual void run() = 0;

  void set_ret(int ret);

private:
  static void* _thd_rotine(void* arg);

  std::string _name;
  uint32_t _tid;
  pthread_t _thd;
  volatile bool _run_flag = false;
  int _ret = 0;

  static volatile int _s_tid_idx;
};

}  // namespace logproxy
}  // namespace oceanbase
