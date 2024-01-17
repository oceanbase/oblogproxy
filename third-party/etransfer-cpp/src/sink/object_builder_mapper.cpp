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

#include "sink/object_builder_mapper.h"

#include "sink/add_table_column_object_builder.h"
#include "sink/add_table_constraint_object_builder.h"
#include "sink/add_table_partition_object_builder.h"
#include "sink/add_table_reference_constraint_object_builder.h"
#include "sink/alter_table_column_object_builder.h"
#include "sink/alter_table_object_builder.h"
#include "sink/alter_table_partition_object_builder.h"
#include "sink/check_constraint_object_builder.h"
#include "sink/comment_object_builder.h"
#include "sink/create_table_object_builder.h"
#include "sink/drop_index_object_builder.h"
#include "sink/drop_table_column_object_builder.h"
#include "sink/drop_table_constraint_object_builder.h"
#include "sink/drop_table_object_builder.h"
#include "sink/drop_table_partition_object_builder.h"
#include "sink/rename_index_object_builder.h"
#include "sink/rename_table_object_builder.h"
#include "sink/truncate_table_object_builder.h"
#include "sink/truncate_table_partition_object_builder.h"
namespace etransfer {
namespace sink {
BuilderMap ObjectBuilderMapper::InitMapper() {
  BuilderMap mapper;
  mapper[common::ObjectType::DROP_TABLE_OBJECT] =
      std::make_shared<DropTableObjectBuilder>();
  mapper[common::ObjectType::TRUNCATE_TABLE_OBJECT] =
      std::make_shared<TruncateTableObjectBuilder>();
  mapper[common::ObjectType::CREATE_TABLE_OBJECT] =
      std::make_shared<CreateTableObjectBuilder>();
  mapper[common::ObjectType::TABLE_COLUMN_OBJECT] =
      std::make_shared<AddTableColumnObjectBuilder>();
  mapper[common::ObjectType::ALTER_TABLE_OBJECT] =
      std::make_shared<AlterTableObjectBuilder>();
  mapper[common::ObjectType::TABLE_CHECK_CONSTRAINT_OBJECT] =
      std::make_shared<CheckConstraintObjectBuilder>();
  mapper[common::ObjectType::TABLE_CONSTRAINT_OBJECT] =
      std::make_shared<AddTableConstraintObjectSqlBuilder>();
  mapper[common::ObjectType::COMMENT_OBJECT] =
      std::make_shared<CommentObjectBuilder>();
  mapper[common::ObjectType::TABLE_PARTITION_OBJECT] =
      std::make_shared<AddTablePartitionObjectBuilder>();
  mapper[common::ObjectType::DROP_INDEX_OBJECT] =
      std::make_shared<DropIndexObjectBuilder>();
  mapper[common::ObjectType::DROP_TABLE_COLUMN_OBJECT] =
      std::make_shared<DropTableColumnObjectBuilder>();
  mapper[common::ObjectType::DROP_TABLE_CONSTRAINT_OBJECT] =
      std::make_shared<DropTableConstraintObjectBuilder>();
  mapper[common::ObjectType::RENAME_INDEX_OBJECT] =
      std::make_shared<RenameIndexObjectBuilder>();
  mapper[common::ObjectType::RENAME_TABLE_OBJECT] =
      std::make_shared<RenameTableObjectBuilder>();
  mapper[common::ObjectType::TRUNCATE_TABLE_PARTITION_OBJECT] =
      std::make_shared<TruncateTablePartitionObjectBuilder>();
  mapper[common::ObjectType::DROP_TABLE_PARTITION_OBJECT] =
      std::make_shared<DropTablePartitionObjectBuilder>();
  mapper[common::ObjectType::ALTER_TABLE_COLUMN_OBJECT] =
      std::make_shared<AlterTableColumnObjectBuilder>();
  mapper[common::ObjectType::ALTER_TABLE_PARTITION_OBJECT] =
      std::make_shared<AlterTablePartitionObjectBuilder>();
  mapper[common::ObjectType::TABLE_REFERENCE_CONSTRAINT_OBJECT] =
      std::make_shared<AddTableReferenceConstraintObjectBuilder>();
  return mapper;
}
const BuilderMap ObjectBuilderMapper::mysql_object_builder_mapper =
    ObjectBuilderMapper::InitMapper();
std::shared_ptr<ObjectBuilder> ObjectBuilderMapper::GetObjectBuilder(
    ObjectType object_type) {
  auto builder = mysql_object_builder_mapper.find(object_type);
  if (builder != mysql_object_builder_mapper.end()) {
    return builder->second;
  }
  return nullptr;
}
}  // namespace sink

}  // namespace etransfer
