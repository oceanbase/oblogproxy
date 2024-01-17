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
#include "object/object.h"
#include "object/table_column_object.h"
namespace etransfer {
namespace object {
class AlterTableColumnObject : public Object {
 public:
  enum AlterColumnType {
    // reset the column's data type definition
    CHANGE_DATA_TYPE,
    // set visible or invisible
    CHANGE_VISIBILITY,
    // reset generated column
    CHANGE_GENERATION,
    // reset default value
    CHANGE_DEFAULT,
    // set null or set not null
    CHANGE_NULLABLE,
    // drop default value
    DROP_DEFAULT,
    // reclaim the existed column definition or to new column definition, this
    // is the union operation of rename and change data type
    REDEFINE_COLUMN
  };
  RawConstant GetRawColumnNameToAlter() const { return column_name_to_alter_; }

  AlterTableColumnObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& column_name_to_alter,
      std::shared_ptr<TableColumnObject> modified_column_definition,
      std::shared_ptr<std::vector<AlterColumnType>> alter_column_type_list)
      : Object(catalog, object_name, raw_ddl,
               ObjectType::ALTER_TABLE_COLUMN_OBJECT),
        column_name_to_alter_(column_name_to_alter),
        modified_column_definition_(modified_column_definition),
        alter_column_type_list_(alter_column_type_list) {}

  std::shared_ptr<TableColumnObject> GetModifiedColumnDefinition() {
    return modified_column_definition_;
  }

  bool GetNullable() { return modified_column_definition_->IsNullable(); }

  bool GetVisible() { return modified_column_definition_->Visible(); }

  std::shared_ptr<std::vector<AlterColumnType>> GetAlterColumnType() {
    return alter_column_type_list_;
  }

  std::string GetColumnNameToAlter() {
    return Util::RawConstantValue(column_name_to_alter_);
  }

  std::shared_ptr<ExprToken> GetDefaultValue() {
    return modified_column_definition_->GetDefaultValue();
  }

  std::shared_ptr<ExprToken> GetGenExpr() {
    return modified_column_definition_->GetGenExpr();
  }

  std::pair<RealDataType, std::shared_ptr<TypeInfo>> GetDataType() {
    return std::make_pair(modified_column_definition_->GetDataType(),
                          modified_column_definition_->GetTypeInfo());
  }

 private:
  RawConstant column_name_to_alter_;
  std::shared_ptr<TableColumnObject> modified_column_definition_;
  std::shared_ptr<std::vector<AlterColumnType>> alter_column_type_list_;
};
}  // namespace object

}  // namespace etransfer