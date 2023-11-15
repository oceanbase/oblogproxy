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

#include "oblog_config.h"
#include "obcdc/obcdc_entry.h"

#ifndef OB_TIMEOUT
#define OB_TIMEOUT -4012
#endif

#ifndef OB_SUCCESS
#define OB_SUCCESS 0
#endif

namespace oceanbase {
namespace logproxy {

class ObCdcAccessFactory {
public:
  static int load(const OblogConfig& config, IObCdcAccess*& cdc_access);

  static void unload(IObCdcAccess* cdc_access);

private:
  static int locate_obcdc_library(const std::string& ob_version, std::string& so_path);
};

}  // namespace logproxy
}  // namespace oceanbase