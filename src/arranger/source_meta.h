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
#include "client_meta.h"

namespace oceanbase {
namespace logproxy {
struct SourceMeta {
public:
  const LogType type;
  const int pid;
  const std::string client_id;

  /**
   * last checkd alive time
   */
  time_t _last_time;

public:
  SourceMeta(LogType t, int p, const std::string& cid) : type(t), pid(p), client_id(cid)
  {
    id_str = std::to_string(pid);
  }

  std::string to_string() const
  {
    return "";
  }

  const std::string& id() const
  {
    return id_str;
  }

private:
  std::string id_str;
};

}  // namespace logproxy
}  // namespace oceanbase
