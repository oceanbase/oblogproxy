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
#include "common/catalog.h"
#include "common/raw_constant.h"
#include "convert/sql_builder_context.h"
#include "object/comment_object.h"
#include "object/object.h"
#include "object/table_column_object.h"
namespace etransfer {
using namespace common;
using namespace object;
namespace sink {
class SqlBuilderUtil {
 private:
  /* data */
 public:
  static std::string BuildRealTableName(
      const Catalog& catalog, const RawConstant& object_name,
      std::shared_ptr<BuildContext> sql_builder_context, bool with_schema);
  static std::string EscapeNormalObjectName(
      const std::string& object_name,
      std::shared_ptr<BuildContext> sql_builder_context);
  static std::string EscapeNormalObjectName(
      const RawConstant& constant,
      std::shared_ptr<BuildContext> sql_builder_context) {
    return EscapeNormalObjectName(
        Util::RawConstantValue(constant,
                               sql_builder_context->use_origin_identifier),
        sql_builder_context);
  }
  static std::string AddEscapeString(const std::string& object_name,
                                     const std::string& escape_char);
  static std::string GetRealTableName(
      ObjectPtr object, std::shared_ptr<BuildContext> sql_builder_context);
  static std::string GetRealTableName(
      const Catalog& catalog, const RawConstant& object_name,
      std::shared_ptr<BuildContext> sql_builder_context);
  static std::string GetRealColumnName(
      ObjectPtr object, std::shared_ptr<BuildContext> sql_builder_context);
  static std::string GetRealColumnName(
      const Catalog& catalog, const RawConstant& object_name,
      const RawConstant& name,
      std::shared_ptr<BuildContext> sql_builder_context);
  static std::string GetRealColumnNames(
      const Catalog& catalog, const RawConstant& object_name,
      std::shared_ptr<std::vector<RawConstant>> names,
      std::shared_ptr<BuildContext> sql_builder_context);
  static std::string BuildComment(
      std::shared_ptr<CommentObject> comment_object);
  static std::string BuildPosition(
      std::shared_ptr<TableColumnObject> table_column_object,
      std::shared_ptr<BuildContext> sql_builder_context);
};

}  // namespace sink

}  // namespace etransfer
