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

#include "sink/mysql_builder.h"

#include "sink/create_table_object_builder.h"
#include "sink/drop_table_object_builder.h"
#include "sink/object_builder_mapper.h"
#include "sink/truncate_table_object_builder.h"
namespace etransfer {
namespace sink {
Strings MySqlBuilder::RealApplySchemaObject(
    ObjectPtr db_object, std::shared_ptr<BuildContext> context) {
  Strings res;
  if (context->use_foreign_key_filter) {
    object_filter.AddIndexTypeFilter(IndexType::FOREIGN);
  }
  if (db_object->DoFilter(object_filter)) {
    return res;
  }
  auto builder =
      ObjectBuilderMapper::GetObjectBuilder(db_object->GetObjectType());
  if (builder != nullptr) {
    res = builder->BuildSql(db_object, nullptr, context);
    if (!context->succeed) {
      res.clear();
    }
  }
  return res;
}

}  // namespace sink
}  // namespace etransfer