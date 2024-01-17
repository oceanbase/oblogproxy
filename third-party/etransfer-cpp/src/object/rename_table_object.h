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
class RenameTableObject : public Object {
 public:
  struct RenamePair {
    Catalog source_catalog;
    RawConstant source_table_name;
    Catalog dest_catalog;
    RawConstant dest_table_name;

    RenamePair(const Catalog& source_catalog,
               const RawConstant& source_table_name,
               const Catalog& dest_catalog, const RawConstant& dest_table_name)
        : source_catalog(source_catalog),
          source_table_name(source_table_name),
          dest_catalog(dest_catalog),
          dest_table_name(dest_table_name) {}
  };
  RenameTableObject(const std::vector<RenamePair>& rename_pairs,
                    const Catalog& source_catalog,
                    const RawConstant& source_table_name,
                    const std::string& raw_ddl)
      : Object(source_catalog, source_table_name, raw_ddl,
               ObjectType::RENAME_TABLE_OBJECT),
        rename_pairs_(rename_pairs),
        is_parent_alter_table_(true) {}

  // constructor for RENAME TABLE statement, set isParentAlterTable to false.
  RenameTableObject(const std::vector<RenamePair>& rename_pairs,
                    const std::string& raw_ddl)
      : Object(Catalog(), RawConstant(), raw_ddl,
               ObjectType::RENAME_TABLE_OBJECT),
        rename_pairs_(rename_pairs),
        is_parent_alter_table_(false) {}
  const std::vector<RenamePair>& GetRenamePairs() { return rename_pairs_; }

  bool IsAlterTable() { return is_parent_alter_table_; }

 private:
  std::vector<RenamePair> rename_pairs_;
  bool is_parent_alter_table_;
};

}  // namespace object

}  // namespace etransfer
