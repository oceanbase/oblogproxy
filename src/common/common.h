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

#include <string>

namespace oceanbase {
namespace logproxy {

#define OMS_OK 0
#define OMS_AGAIN 1
#define OMS_FAILED (-1)
#define OMS_TIMEOUT (-2)
#define OMS_CONNECT_FAILED (-3)

#define OMS_UNUSED(x) (void)(x)

#define OMS_SINGLETON(__clazz__)            \
public:                                     \
  static __clazz__& instance()              \
  {                                         \
    static __clazz__ __clazz__##_singleton; \
    return __clazz__##_singleton;           \
  }                                         \
                                            \
private:                                    \
  __clazz__()                               \
  {}

#define OMS_AVOID_COPY(__clazz__)       \
private:                                \
  __clazz__(const __clazz__&) = delete; \
  __clazz__& operator=(const __clazz__&) = delete;

#define OMS_ATOMIC_CAS(_it_, _old_, _new_) __sync_bool_compare_and_swap(&_it_, _old_, _new_)

#define OMS_ATOMIC_INC(_it_) __sync_add_and_fetch(&_it_, 1)

#define OMS_ATOMIC_DEC(_it_) __sync_add_and_fetch(&_it_, -1)

bool localhostip(std::string& hostname, std::string& ip);

void dumphex(const char data[], size_t size, std::string& hex);
void hex2bin(const char data[], size_t size, std::string& bin);

}  // namespace logproxy
}  // namespace oceanbase
