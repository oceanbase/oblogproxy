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

#include "sink/alter_table_partition_object_builder.h"

#include "object/partition_object.h"
namespace etransfer {
namespace sink {
Strings AlterTablePartitionObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<AlterTablePartitionObject> alter_table_partition_object =
      std::dynamic_pointer_cast<AlterTablePartitionObject>(db_object);
  Strings res;
  std::string line;
  line.append(BuildCreateAddPartitionObject(
      alter_table_partition_object->GetModifiedTablePartitionObject(),
      parent_object, sql_builder_context));
  res.push_back(line);
  return res;
}
}  // namespace sink

}  // namespace etransfer
