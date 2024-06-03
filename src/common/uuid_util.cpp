/**
 * Copyright (c) 2024 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "uuid_util.h"

namespace oceanbase {
namespace logproxy {
std::string UUIDGenerator::generate()
{
  std::random_device rd;
  std::mt19937_64 rng(rd());
  std::uniform_int_distribution<uint64_t> dist(0, (uint64_t)(-1));

  auto hex4 = [&rng, &dist]() {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(4) << (dist(rng) & 0xFFFF);
    return ss.str();
  };

  auto hex8 = [&hex4]() { return hex4() + hex4(); };

  auto hex12 = [&hex4]() { return hex4() + hex4() + hex4(); };

  std::string uuid = hex8() + "-" + hex4() + "-4" + hex4().substr(1) + "-a" + hex4().substr(1) + "-" + hex12();

  std::transform(uuid.begin(), uuid.end(), uuid.begin(), ::tolower);

  return uuid;
}
}  // namespace logproxy
}  // namespace oceanbase