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

#include "sink/drop_table_constraint_object_builder.h"

#include "object/drop_table_constraint_object.h"
namespace etransfer {
namespace sink {
using namespace object;
Strings DropTableConstraintObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<DropTableConstraintObject> drop_table_constraint_object =
      std::dynamic_pointer_cast<DropTableConstraintObject>(db_object);
  Strings res;
  std::string line;
  if (drop_table_constraint_object->GetIndexType() == IndexType::PRIMARY) {
    line.append("DROP PRIMARY KEY ");
  } else {
    sql_builder_context->SetErrMsg("unsupported drop constraint type");
  }
  res.push_back(line);
  return res;
}
}  // namespace sink

}  // namespace etransfer
