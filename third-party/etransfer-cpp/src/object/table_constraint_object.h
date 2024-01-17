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
#include "object/expr_token.h"
#include "object/object.h"
#include "object/option.h"
#include "object/type_info.h"
namespace etransfer {
namespace object {
class TableConstraintObject : public Object {
 public:
  enum ConstraintSortOrder { ASC, DESC, RANDOM, DEFAULT };

  class ColumnInfo {
   public:
    ConstraintSortOrder constraint_sort_order;
    int precision;
    int id;
    std::string option;

    ColumnInfo(ConstraintSortOrder constraint_sort_order, int precision)
        : ColumnInfo(constraint_sort_order, precision,
                     TypeInfo::ANONYMOUS_NUMBER, "") {}

    ColumnInfo(ConstraintSortOrder constraint_sort_order, int precision, int id,
               const std::string& option)
        : constraint_sort_order(constraint_sort_order),
          precision(precision),
          id(id),
          option(option) {}
  };

  using ColumnInfoMap = std::shared_ptr<
      std::unordered_map<std::string, TableConstraintObject::ColumnInfo>>;

  TableConstraintObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& constraint_name,
      IndexType index_type, ColumnInfoMap constraint_sort_order,
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affected_columns)
      : TableConstraintObject(catalog, object_name, raw_ddl, constraint_name,
                              index_type, ObjectType::TABLE_CONSTRAINT_OBJECT,
                              constraint_sort_order, affected_columns) {}

  TableConstraintObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& constraint_name,
      IndexType index_type, ColumnInfoMap constraint_sort_order,
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affected_columns,
      std::shared_ptr<std::vector<RawConstant>> length_of_pre_index)
      : TableConstraintObject(catalog, object_name, raw_ddl, constraint_name,
                              index_type, ObjectType::TABLE_CONSTRAINT_OBJECT,
                              constraint_sort_order, affected_columns,
                              length_of_pre_index) {}

  TableConstraintObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& constraint_name,
      IndexType index_type, ObjectType object_type,
      ColumnInfoMap constraint_sort_order,
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affected_columns)
      : TableConstraintObject(catalog, object_name, raw_ddl, constraint_name,
                              index_type, object_type, constraint_sort_order,
                              affected_columns, nullptr) {}

  TableConstraintObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& constraint_name,
      IndexType index_type, ObjectType object_type,
      ColumnInfoMap constraint_sort_order,
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affected_columns,
      std::shared_ptr<std::vector<RawConstant>> length_of_pre_index)
      : Object(catalog, object_name, raw_ddl, object_type),
        affected_columns_(affected_columns),
        index_type_(index_type),
        constraint_name_(constraint_name),
        constraint_sort_order_(constraint_sort_order),
        length_of_pre_index_(length_of_pre_index) {}

  IndexType GetIndexType() { return index_type_; }

  RawConstant GetConstraintName() { return constraint_name_; }

  Strings GetAffectedColumns() {
    Strings expr_values;
    for (const auto& affected_column : *affected_columns_) {
      expr_values.push_back(affected_column->token.value);
    }
    return expr_values;
  }

  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>>
  GetRawAffectedColumns() {
    return affected_columns_;
  }

  ColumnInfoMap GetConstraintSortOrder() { return constraint_sort_order_; }

  std::shared_ptr<std::vector<RawConstant>> GetLengthOfPreIndex() {
    return length_of_pre_index_;
  }

 private:
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affected_columns_;
  IndexType index_type_;
  RawConstant constraint_name_;
  ColumnInfoMap constraint_sort_order_;
  std::shared_ptr<std::vector<RawConstant>> length_of_pre_index_;
};

}  // namespace object

}  // namespace etransfer
