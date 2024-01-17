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
#include "common/raw_constant.h"
namespace etransfer {
namespace object {
using namespace common;
class ColumnLocationIdentifier {
 public:
  enum ColPositionLocator {
    HEAD,
    TAIL,
    BEFORE,
    AFTER,
    ID,
    BETWEEN,
  };

  ColumnLocationIdentifier(ColPositionLocator col_position_locator)
      : col_position_locator_(col_position_locator) {}
  ColumnLocationIdentifier(ColPositionLocator col_position_locator,
                           const RawConstant& adjacent_column_before,
                           const RawConstant& adjacent_column_after)
      : col_position_locator_(col_position_locator),
        adjacent_column_before_(adjacent_column_before),
        adjacent_column_after_(adjacent_column_after) {}

  RawConstant GetAdjacentColumnBefore() { return adjacent_column_before_; }

  RawConstant GetAdjacentColumnAfter() { return adjacent_column_after_; }

  ColPositionLocator GetColPositionLocator() { return col_position_locator_; }

 private:
  ColPositionLocator col_position_locator_;
  RawConstant adjacent_column_before_;
  RawConstant adjacent_column_after_;
};
}  // namespace object

}  // namespace etransfer