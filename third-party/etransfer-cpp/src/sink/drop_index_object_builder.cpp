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

#include "sink/drop_index_object_builder.h"

#include "object/drop_index_object.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
using namespace object;
Strings DropIndexObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<DropIndexObject> drop_index_object =
      std::dynamic_pointer_cast<DropIndexObject>(db_object);
  Strings res;
  std::string line;
  line.append("DROP INDEX ");
  auto index_name = drop_index_object->GetRawIndexName();
  line.append(
      SqlBuilderUtil::EscapeNormalObjectName(index_name, sql_builder_context));
  // if parentObject is not null, then this is an alter table drop index
  // statement ON TABLE_NAME part should not be generated.
  if (parent_object == nullptr) {
    line.append(" ON ").append(SqlBuilderUtil::GetRealTableName(
        drop_index_object, sql_builder_context));
  }
  res.push_back(line);

  return res;
}

}  // namespace sink

}  // namespace etransfer
