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
#include "common/data_type.h"
#include "convert/sql_builder_context.h"
#include "object/expr_token.h"
namespace etransfer {
namespace sink {
class ExprBuilder {
 public:
  ExprBuilder() = default;
  virtual std::string Build(
      std::shared_ptr<object::ExprToken> default_value,
      common::RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context) = 0;

  // std::string buildSelect(SelectObject select, BuildContext context);
};
}  // namespace sink

}  // namespace etransfer
