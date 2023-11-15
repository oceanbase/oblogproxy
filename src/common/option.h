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
#include <functional>
#include <unordered_map>
#include <getopt.h>

#include "common.h"

namespace oceanbase {
namespace logproxy {
struct OmsOption {
  char opt;
  std::string opt_long;
  bool is_arg;
  std::string desc;

  std::function<void(const std::string&)> func;

  OmsOption(char opt, const std::string& opt_long, bool is_arg, const std::string& desc,
      const std::function<void(const std::string&)>& func);
};

class OmsOptions {
  OMS_AVOID_COPY(OmsOptions);

public:
  explicit OmsOptions(const std::string& title = "");

  virtual ~OmsOptions();

  void add(const OmsOption& option);

  void usage() const;

  struct option* long_pattern();

  std::string pattern();

  OmsOption* get(char opt);

  inline std::unordered_map<char, OmsOption>& options()
  {
    return _options;
  }

private:
  std::string _title;
  std::unordered_map<char, OmsOption> _options;
  struct option* _sys_opts = nullptr;
};

}  // namespace logproxy
}  // namespace oceanbase
