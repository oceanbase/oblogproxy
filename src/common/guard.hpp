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

#include <stdlib.h>
#include <functional>

namespace oceanbase {
namespace logproxy {

template <typename T>
class FreeGuard {
public:
  explicit FreeGuard(const T ptr) : _ptr(ptr)
  {}

  FreeGuard(T ptr, std::function<void(T)> free_func) : _ptr(ptr), _free_func(free_func)
  {}

  ~FreeGuard()
  {
    if (_own) {
      if (_free_func) {
        _free_func(_ptr);
      } else {
        ::free(_ptr);
      }
      _ptr = nullptr;
    }
  }

  void release()
  {
    _own = false;
  }

private:
  bool _own = true;
  T _ptr = nullptr;

  std::function<void(T)> _free_func;
};

}  // namespace logproxy
}  // namespace oceanbase
