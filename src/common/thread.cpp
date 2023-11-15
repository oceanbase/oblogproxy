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

#include <utility>
#include <unistd.h>
#include <csignal>
#include "thread.h"
#include "log.h"
#include "common.h"

namespace oceanbase {
namespace logproxy {
volatile int Thread::_s_tid_idx = 0;

Thread::Thread(std::string name) : _name(std::move(name))
{
  _tid = ::getpid() + OMS_ATOMIC_INC(_s_tid_idx);
}

Thread::~Thread() = default;

void Thread::stop()
{
  bool expected_run_flag = true;
  if (_run_flag.compare_exchange_strong(expected_run_flag, false)) {
    OMS_STREAM_DEBUG << "!!! Stop OMS thread: " << _name << "(" << tid() << ")";
  } else {
    OMS_WARN("The current thread :{}({}) has already stopped", _name, _tid);
  }
}

int Thread::join()
{
  if (pthread_kill(_thd, 0) == 0) {
    OMS_STREAM_DEBUG << "<< Joining thread: " << _name << "(" << tid() << ")";
    pthread_join(_thd, nullptr);
    OMS_STREAM_DEBUG << ">> Joined thread: " << _name << "(" << tid() << ")";
  }
  return _ret;
}

void* Thread::_thd_rotine(void* arg)
{
  Thread* thd = (Thread*)arg;
  OMS_STREAM_DEBUG << "+++ Create thread: " << thd->_name << "(" << thd->tid() << ")";
  // cast child class poiter to base class pointer
  thd->run();
  if (thd->is_direct_release()) {
    delete thd;
  }
  return nullptr;
}

void Thread::start()
{
  bool expected_run_flag = false;
  if (_run_flag.compare_exchange_strong(expected_run_flag, true)) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (_is_detach) {
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    }
    pthread_create(&_thd, &attr, _thd_rotine, this);
    pthread_attr_destroy(&attr);
  } else {
    OMS_WARN("The current thread :{}({}) is already running", _name, _tid);
  }
}

bool Thread::is_run()
{
  return _run_flag.load();
}

void Thread::set_run(bool run_flag)
{
  _run_flag.store(run_flag);
}

void Thread::detach()
{
  if (pthread_kill(_thd, 0) == 0) {
    pthread_detach(_thd);
  }
}

void Thread::set_ret(int ret)
{
  _ret = ret;
}

void Thread::set_detach_state(bool is_detach)
{
  this->_is_detach = is_detach;
}

void Thread::set_release_state(bool direct_release)
{
  this->_direct_release = direct_release;
}

bool Thread::is_direct_release()
{
  return this->_direct_release;
}

}  // namespace logproxy
}  // namespace oceanbase
