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

#include "sink/rename_table_object_builder.h"

#include "object/rename_table_object.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
using namespace object;
Strings RenameTableObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<RenameTableObject> rename_table_object =
      std::dynamic_pointer_cast<RenameTableObject>(db_object);
  Strings res;
  std::string line;
  if (parent_object != nullptr) {
    // this is an ALTER TABLE RENAME statement
    line.append("RENAME TO ");
    const auto& rename_pairs = rename_table_object->GetRenamePairs();
    if (rename_pairs.size() != 1) {
      sql_builder_context->succeed = false;
      return res;
    }
    line.append(SqlBuilderUtil::BuildRealTableName(
        rename_pairs[0].dest_catalog, rename_pairs[0].dest_table_name,
        sql_builder_context, true));
  } else {
    // this is a RENAME TABLE statement
    line.append("RENAME TABLE \n");
    const auto& rename_pairs = rename_table_object->GetRenamePairs();
    for (int i = 0; i < rename_pairs.size(); i++) {
      if (i > 0) {
        line.append(",\n");
      }
      line.append("\t");
      line.append(SqlBuilderUtil::BuildRealTableName(
          rename_pairs[i].source_catalog, rename_pairs[i].source_table_name,
          sql_builder_context, true));
      line.append(" TO ");
      line.append(SqlBuilderUtil::BuildRealTableName(
          rename_pairs[i].dest_catalog, rename_pairs[i].dest_table_name,
          sql_builder_context, true));
    }
  }
  res.push_back(line);
  return res;
}
}  // namespace sink

}  // namespace etransfer
