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
#include "object/create_table_object.h"
#include "sink/object_builder.h"
namespace etransfer {
namespace sink {
class CreateTableObjectBuilder : public ObjectBuilder {
 public:
  Strings RealBuildSql(ObjectPtr db_object, ObjectPtr parent_object,
                       std::shared_ptr<BuildContext> sql_builder_context);
  std::string BuildCreateTableHeadPart(
      std::shared_ptr<object::CreateTableObject>,
      std::shared_ptr<BuildContext>);
  std::string BuildCreateLikeBody(std::shared_ptr<object::CreateTableObject>,
                                  std::shared_ptr<BuildContext>);
  bool BuildCreateNormalTableBody(
      std::shared_ptr<CreateTableObject> table_object,
      const ObjectPtrs& create_table_sub_actions,
      std::shared_ptr<BuildContext> sql_builder_context, std::string& line);
  std::string BuildTableAttributes(
      std::shared_ptr<CreateTableObject> db_object,
      std::shared_ptr<BuildContext> sql_builder_context);
  std::string BuildOptions(
      std::shared_ptr<std::vector<std::shared_ptr<Option>>> options,
      const std::string& delim);

  std::string BuildExtraInfo(std::shared_ptr<CreateTableObject> db_object,
                             std::shared_ptr<BuildContext> sql_builder_context);
  static std::string BuildTableCharset(std::shared_ptr<Option> option_object);

  static std::string BuildTableCollation(std::shared_ptr<Option> option_object);
};
}  // namespace sink

}  // namespace etransfer
