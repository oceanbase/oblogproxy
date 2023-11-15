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

#include <vector>
#include <functional>
#include "common.h"
#include "log.h"
#include "config.h"

namespace oceanbase {
namespace logproxy {

class Model {
public:
  Model() = default;
  // copy ctor do nothing
  Model(const Model& rhs)
  {}

  Model(Model&& rhs) noexcept
  {}

  Model& operator=(const Model& rhs)
  {
    _to_str_flag = false;
    _to_str_buf.clear();
    return *this;
  }

  Model& operator=(Model&& rhs) noexcept
  {
    _to_str_flag = false;
    _to_str_buf.clear();
    return *this;
  }

  virtual const std::string& to_string() const
  {
    Model* p = (Model*)this;
    if (OMS_ATOMIC_CAS(p->_to_str_flag, false, true)) {
      LogStream ls{0, "", 0, nullptr};
      for (auto& fn : p->_to_str_fns) {
        fn(ls);
      }
      p->_to_str_buf = ls.str();
    }
    return _to_str_buf;
  }

protected:
  template <typename T>
  struct Reg {
  public:
    Reg(Model* model, const std::string& k, T& v)
    {
      model->_to_str_fns.push_back([k, &v](LogStream& ls) { ls << k << ":" << v << ", "; });
    }
  };

  volatile bool _to_str_flag = false;
  std::vector<std::function<void(LogStream&)>> _to_str_fns;
  std::string _to_str_buf;
};

#define OMS_MF(T, obj) OMS_MF_SCOPE(T, obj, public)
#define OMS_MF_PRI(T, obj) OMS_MF_SCOPE(T, obj, private)

#define OMS_MF_SCOPE(T, obj, scope) \
  scope:                            \
  T obj;                            \
                                    \
private:                            \
  Reg<T> _reg_##obj                 \
  {                                 \
    this, #obj, obj                 \
  }

#define OMS_MF_DFT(T, obj, dft) OMS_MF_DFT_SCOPE(T, obj, dft, public)
#define OMS_MF_DFT_PRI(T, obj, dft) OMS_MF_DFT_SCOPE(T, obj, dft, private)

#define OMS_MF_DFT_SCOPE(T, obj, dft, scope) \
  scope:                                     \
  T obj = dft;                               \
                                             \
private:                                     \
  Reg<T> _reg_##obj                          \
  {                                          \
    this, #obj, obj                          \
  }

#define OMS_MF_ENABLE_COPY(clazz)               \
public:                                         \
  clazz(const clazz& rhs) : Model(rhs)          \
  {                                             \
    *this = rhs;                                \
  }                                             \
  clazz(clazz&& rhs) noexcept = default;        \
  clazz& operator=(const clazz& rhs) = default; \
  clazz& operator=(clazz&& rhs) = default

}  // namespace logproxy
}  // namespace oceanbase
