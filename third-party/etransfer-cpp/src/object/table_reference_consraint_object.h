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
namespace etransfer {
namespace object {
class TableReferenceConstraintObject : public Object {
 public:
  enum FKReferenceRange { PART_OF_TABLE_COLUMNS, ALL_TABLE_COLUMNS };
  enum FKReferenceOperationCascade {
    ON_DELETE_NO_ACTION,
    ON_DELETE_RESTRICT,
    ON_DELETE_CASCADE,
    ON_DELETE_SET_NULL,
    ON_DELETE_SET_DEFAULT,
    ON_UPDATE_NO_ACTION,
    ON_UPDATE_RESTRICT,
    ON_UPDATE_DEFAULT,
    ON_UPDATE_CASCADE,
    ON_UPDATE_SET_NULL,
    NOT_DEFINED
  };

  TableReferenceConstraintObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl, const RawConstant& constraint_name,
      IndexType index_type,
      std::shared_ptr<std::vector<RawConstant>> child_table_columns,
      FKReferenceRange reference_range, const Catalog& parent_catalog,
      const RawConstant& parent_table,
      std::shared_ptr<std::vector<RawConstant>> parent_table_columns,
      std::shared_ptr<std::vector<FKReferenceOperationCascade>>
          operation_cascades)
      : Object(catalog, object_name, raw_ddl,
               ObjectType::TABLE_REFERENCE_CONSTRAINT_OBJECT),
        constraint_name_(constraint_name),
        index_type_(index_type),
        child_table_columns_(child_table_columns),
        reference_range_(reference_range),
        parent_catalog_(parent_catalog),
        parent_table_(parent_table),
        parent_table_columns_(parent_table_columns),
        operation_cascades_(operation_cascades) {}

  IndexType GetIndexType() { return index_type_; }

  std::string GetConstraintName() {
    return Util::RawConstantValue(constraint_name_);
  }

  std::shared_ptr<std::vector<FKReferenceOperationCascade>>
  GetOperationCascades() {
    return operation_cascades_;
  }

  std::shared_ptr<std::vector<RawConstant>> GetRawChildTableColumns() {
    return child_table_columns_;
  }

  Strings GetParentTableColumns() {
    Strings res;
    if (parent_table_columns_ == nullptr) {
      return res;
    }
    for (const auto& column : *parent_table_columns_) {
      res.push_back(column.value);
    }
    return res;
  }

  std::shared_ptr<std::vector<RawConstant>> GetRawParentTableColumns() {
    return parent_table_columns_;
  }

  std::string GetParentTable() { return Util::RawConstantValue(parent_table_); }

  RawConstant GetRawParentTable() { return parent_table_; }

  Catalog GetParentCatalog() { return parent_catalog_; }
  bool FilterInner(const ObjectFilter& object_filter) {
    if (object_filter.index_type_to_filter.count(index_type_) != 0) {
      return true;
    }
    return false;
  }

 private:
  RawConstant constraint_name_;
  IndexType index_type_;
  std::shared_ptr<std::vector<RawConstant>> child_table_columns_;
  FKReferenceRange reference_range_;
  Catalog parent_catalog_;
  RawConstant parent_table_;
  std::shared_ptr<std::vector<RawConstant>> parent_table_columns_;
  std::shared_ptr<std::vector<FKReferenceOperationCascade>> operation_cascades_;
};

}  // namespace object

}  // namespace etransfer
