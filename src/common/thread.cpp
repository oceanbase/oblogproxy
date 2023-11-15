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

#include "common/log.h"
#include "common/common.h"
#include "common/thread.h"

namespace oceanbase {
namespace logproxy {

volatile int Thread::_s_tid_idx = 0;

Thread::Thread(const std::string& name) : _name(name)
{
  _tid = ::getpid() + OMS_ATOMIC_INC(_s_tid_idx);
}

Thread::~Thread() = default;

void Thread::stop()
{
  if (is_run()) {
    _run_flag = false;
    OMS_DEBUG << "!!! Stop OMS thread: " << _name << "(" << tid() << ")";
  }
}

int Thread::join()
{
  if (is_run()) {
    OMS_DEBUG << "<< Joining thread: " << _name << "(" << tid() << ")";
    pthread_join(_thd, nullptr);
    OMS_DEBUG << ">> Joined thread: " << _name << "(" << tid() << ")";
  }
  return _ret;
}

void* Thread::_thd_rotine(void* arg)
{
  Thread& thd = *(Thread*)arg;
  OMS_DEBUG << "+++ Create thread: " << thd._name << "(" << thd.tid() << ")";
  // cast child class poiter to base class pointer
  thd.run();
  return nullptr;
}

void Thread::start()
{
  if (!is_run()) {
    set_run(true);
    pthread_create(&_thd, nullptr, _thd_rotine, this);
  }
}

bool Thread::is_run()
{
  return _run_flag;
}

void Thread::set_run(bool run_flag)
{
  _run_flag = run_flag;
}

void Thread::detach()
{
  pthread_detach(_thd);
}

void Thread::set_ret(int ret)
{
  _ret = ret;
}

}  // namespace logproxy
}  // namespace oceanbase
