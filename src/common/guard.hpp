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
#include <thread>

namespace oceanbase {
namespace logproxy {

#define DEFER_BASE(x, y) x##y
#define DEFER_FUNC(x, y) DEFER_BASE(x, y)
#define DEFER(x) DEFER_FUNC(x, __COUNTER__)
#define defer(expr) auto DEFER(_defered_option) = defer_func([&]() { expr; })

/*!
 * @brief This macro definition only calls the corresponding function when it leaves the scope.
 * However, there is no guarantee that the function has been executed, so this function only applies to releasing resources, etc.
 * @tparam
 */
template <typename F>
struct Defer {
  F f;
  Defer(F f) : f(f)
  {}
  ~Defer()
  {
    f();
  }
};

template <typename F>
Defer<F> defer_func(F f)
{
  return Defer<F>(f);
}

template <typename T>
class FreeGuard {
public:
  explicit FreeGuard(const T ptr) : _ptr(ptr)
  {}

  FreeGuard(T ptr, std::function<void(T)> free_func) : _ptr(ptr), _free_func(free_func)
  {}

  ~FreeGuard()
  {
    if (_own && _ptr != nullptr) {
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

template <typename T>
class ResourceGuard {
private:
  T& _resource;
  void (*_deleter)(T&);

public:
  ResourceGuard(T& resource, void (*deleter)(T&) = nullptr) : _resource(resource), _deleter(deleter)
  {}

  ~ResourceGuard()
  {
    if (_deleter != nullptr) {
      _deleter(_resource);
    }
  }

  ResourceGuard(const ResourceGuard&) = delete;
  ResourceGuard& operator=(const ResourceGuard&) = delete;
};

}  // namespace logproxy
}  // namespace oceanbase