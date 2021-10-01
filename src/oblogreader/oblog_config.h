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
#include <map>
#include "common/model.h"

namespace oceanbase {
namespace logproxy {

struct OblogConfig : public Model {
public:
  OMS_MF(std::string, id);
  OMS_MF_DFT(uint64_t, start_timestamp, 0);

public:
  explicit OblogConfig(const std::string& str);

  const std::map<std::string, std::string>& configs() const
  {
    return _configs;
  }

  void set_auth(const std::string& user, const std::string& password);

private:
  using ConfigMap = std::map<std::string, std::string>;
  OMS_MF_PRI(ConfigMap, _configs);
};

}  // namespace logproxy
}  // namespace oceanbase
