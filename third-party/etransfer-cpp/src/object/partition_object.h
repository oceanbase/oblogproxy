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
#include "object/table_partition_object.h"
namespace etransfer {
namespace object {
class PartitionObject : public Object {
 public:
  const static int partition_level = 1;
  const static int sub_partition_level = 2;

 public:
  PartitionObject(const Catalog catalog, const RawConstant& object_name,
                  ObjectType object_type,
                  std::shared_ptr<std::vector<RawConstant>> partition_names,
                  int level)
      : Object(catalog, object_name, object_type),
        partition_names_(partition_names),
        level_(level) {}

  int GetPartitionLevel() { return level_; }
  std::shared_ptr<std::vector<RawConstant>> GetRawPartitionName() {
    return partition_names_;
  }

 private:
  std::shared_ptr<std::vector<RawConstant>> partition_names_;
  int level_;
};

class DropTablePartitionObject : public PartitionObject {
 public:
  DropTablePartitionObject(
      const Catalog& catalog, const RawConstant& object_name,
      std::shared_ptr<std::vector<RawConstant>> partition_name_to_drop,
      int level)
      : PartitionObject(catalog, object_name,
                        ObjectType::DROP_TABLE_PARTITION_OBJECT,
                        partition_name_to_drop, level) {}
};

class TruncateTablePartitionObject : public PartitionObject {
 public:
  // mysql supports alter table xxx truncate partition all
  bool truncate_all_partition;

  TruncateTablePartitionObject(
      const Catalog& catalog, const RawConstant& object_name,
      std::shared_ptr<std::vector<RawConstant>> partition_name_to_truncate,
      int level, bool truncate_all_partition)
      : PartitionObject(catalog, object_name,
                        ObjectType::TRUNCATE_TABLE_PARTITION_OBJECT,
                        partition_name_to_truncate, level),
        truncate_all_partition(truncate_all_partition) {}
};

class AlterTablePartitionObject : public Object {
 public:
  enum AlterPartitionType {
    // reclaim the existed partition definition or to new partition definition,
    // this is the union operation of delete and create partition
    // mysql、obmysql allows non-partitioned tables to be modified to
    // partitioned tables, and partitioned tables to redefine partitions
    REDEFINE_PARTITION,
    REDEFINE_SUB_PARTITION,
    NOT_INTEREST,
  };

 private:
  std::shared_ptr<TablePartitionObject> table_partition_object_;
  AlterPartitionType alter_partition_type_;

 public:
  AlterTablePartitionObject(
      const Catalog& catalog, const RawConstant& object_name,
      const std::string& raw_ddl,
      std::shared_ptr<TablePartitionObject> table_partition_object,
      AlterPartitionType alter_partition_type)
      : Object(catalog, object_name, raw_ddl,
               ObjectType::ALTER_TABLE_PARTITION_OBJECT),
        table_partition_object_(table_partition_object),
        alter_partition_type_(alter_partition_type) {}

  std::shared_ptr<TablePartitionObject> GetModifiedTablePartitionObject() {
    return table_partition_object_;
  }

  AlterPartitionType GetAlterPartitionType() { return alter_partition_type_; }
};

}  // namespace object

}  // namespace etransfer
