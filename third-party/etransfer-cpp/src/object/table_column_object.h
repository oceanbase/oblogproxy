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
#include <memory>

#include "common/data_type.h"
#include "object/column_attributes.h"
#include "object/column_location_identifier.h"
#include "object/expr_token.h"
#include "object/object.h"
#include "object/type_info.h"
namespace etransfer {
namespace object {
using namespace common;
class TableColumnObject : public Object {
 private:
  RawConstant column_name_to_add_;

  RealDataType data_type_;
  std::shared_ptr<TypeInfo> type_info_;
  std::shared_ptr<ColumnAttributes> column_attributes_;
  std::shared_ptr<ColumnLocationIdentifier> col_position_identifier_;

 public:
  TableColumnObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& column_name_to_add,
      RealDataType real_data_type, std::shared_ptr<TypeInfo> type_info,
      std::shared_ptr<ColumnAttributes> column_attributes,
      std::shared_ptr<ColumnLocationIdentifier> col_position_identifier);

  bool IsNullable() {
    return column_attributes_->IsNullableSet()
               ? column_attributes_->GetIsNullable()
               : true;
  }

  bool Visible() {
    return column_attributes_->IsVisibleSet() ? column_attributes_->IsVisible()
                                              : true;
  }

  std::string GetColumnNameToAdd() {
    return Util::RawConstantValue(column_name_to_add_);
  }

  RawConstant GetRawColumnNameToAdd() { return column_name_to_add_; }

  std::shared_ptr<ExprToken> GetDefaultValue() {
    return column_attributes_->IsDefaultValueSet()
               ? column_attributes_->GetDefaultValue()
               : nullptr;
  }

  std::shared_ptr<ExprToken> GetGenExpr() {
    return column_attributes_->IsGeneratedSet()
               ? column_attributes_->GetExpGen()
               : nullptr;
  }

  GenStoreType GetGenStoreType() {
    return column_attributes_->IsGeneratedSet()
               ? column_attributes_->GetGenStoreType()
               : GenStoreType::INVALID;
  }

  bool IsAutoInc() { return column_attributes_->IsAutoInc(); }

  bool HasOnUpdate() { return column_attributes_->HasOnUpdate(); }

  std::string GetOnUpdate() { return column_attributes_->GetOnUpdateFunc(); }

  std::shared_ptr<ColumnLocationIdentifier> GetColPositionIdentifier() {
    return col_position_identifier_;
  }

  RealDataType GetDataType() { return data_type_; }

  std::shared_ptr<TypeInfo> GetTypeInfo() { return type_info_; }

  std::shared_ptr<ColumnAttributes> GetColumnAttributes() {
    return column_attributes_;
  }
};
}  // namespace object

}  // namespace etransfer