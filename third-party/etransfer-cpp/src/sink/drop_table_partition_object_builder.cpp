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

#include "sink/drop_table_partition_object_builder.h"

#include "object/partition_object.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings DropTablePartitionObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<DropTablePartitionObject> drop_table_partition_object =
      std::dynamic_pointer_cast<DropTablePartitionObject>(db_object);
  Strings res;
  std::string line;
  line.append(" DROP ");
  if (drop_table_partition_object->GetPartitionLevel() ==
      PartitionObject::partition_level) {
    line.append(" PARTITION ");
  } else {
    sql_builder_context->SetErrMsg("mysql doesn't support drop subpartition");
    return res;
  }
  const auto partition_names =
      *drop_table_partition_object->GetRawPartitionName();
  for (int i = 0; i < partition_names.size(); i++) {
    if (i > 0) {
      line.append(",");
    }
    line.append(SqlBuilderUtil::EscapeNormalObjectName(partition_names[i],
                                                       sql_builder_context));
  }
  res.push_back(line);
  return res;
}
}  // namespace sink

}  // namespace etransfer
