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

#include "option.h"

namespace oceanbase {
namespace logproxy {

OmsOption::OmsOption(char opt, const std::string& opt_long, bool is_arg, const std::string& desc,
    const std::function<void(const std::string&)>& func)
    : opt(opt), opt_long(opt_long), is_arg(is_arg), desc(desc), func(func)
{}

OmsOptions::OmsOptions(const std::string& title) : _title(title)
{}

OmsOptions::~OmsOptions()
{
  delete[] _sys_opts;
  _sys_opts = nullptr;
}

void OmsOptions::add(const OmsOption& option)
{
  _options.emplace(option.opt, option);
}

void OmsOptions::usage() const
{
  printf("%s usage: \n", _title.c_str());
  for (auto& opt : _options) {
    printf("-%c, --%s\t\t\t%s\n", opt.first, opt.second.opt_long.c_str(), opt.second.desc.c_str());
  }
}

struct option* OmsOptions::long_pattern()
{
  if (_sys_opts == nullptr) {
    _sys_opts = new option[_options.size() + 1];
    _sys_opts[_options.size()] = {nullptr, 0, nullptr, 0};
  }
  size_t i = 0;
  for (auto& entry : _options) {
    _sys_opts[i].name = entry.second.opt_long.c_str();
    _sys_opts[i].has_arg = entry.second.is_arg ? required_argument : no_argument;
    _sys_opts[i].flag = nullptr;
    _sys_opts[i].val = entry.second.opt;
    ++i;
  }
  return _sys_opts;
}

std::string OmsOptions::pattern()
{
  std::string pt;
  for (auto& entry : _options) {
    pt.append(1, entry.second.opt);
    if (entry.second.is_arg) {
      pt.append(":");
    }
  }
  return pt;
}

OmsOption* OmsOptions::get(char opt)
{
  const auto& entry = _options.find(opt);
  if (entry == _options.end()) {
    return nullptr;
  }
  return &entry->second;
}

}  // namespace logproxy
}  // namespace oceanbase
