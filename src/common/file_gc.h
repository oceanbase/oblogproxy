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
#include <set>
#include "common/thread.h"

namespace oceanbase {
namespace logproxy {

class FileGcRoutine : public Thread {

public:
  FileGcRoutine(std::string path, std::set<std::string> prefixs);

protected:
  void run() override;

private:
  std::string _path;
  std::set<std::string> _prefixs;
};

}  // namespace logproxy
}  // namespace oceanbase
