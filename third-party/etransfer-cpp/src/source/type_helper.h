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

#pragma once
#include <utility>

#include "OBParser.h"
#include "common/data_type.h"
#include "object/type_info.h"
namespace etransfer {
namespace source {
using namespace oceanbase;
using namespace common;
using namespace object;
// TypeHelper used for parsing data type
class TypeHelper {
 public:
  static std::pair<RealDataType, std::shared_ptr<TypeInfo>> ParseDataType(
      OBParser::Data_typeContext* ctx);

  static int GetDefaultLobLength(RealDataType real_data_type);
};
}  // namespace source

}  // namespace etransfer
