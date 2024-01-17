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

#include "object/table_column_object.h"

namespace etransfer {
namespace object {
TableColumnObject::TableColumnObject(
    const Catalog& catalog, const RawConstant& object_name,
    const std::string& raw_ddl, const RawConstant& column_name_to_add,
    RealDataType real_data_type, std::shared_ptr<TypeInfo> type_info,
    std::shared_ptr<ColumnAttributes> column_attributes,
    std::shared_ptr<ColumnLocationIdentifier> col_position_identifier)
    : Object(catalog, object_name, raw_ddl, ObjectType::TABLE_COLUMN_OBJECT),
      column_name_to_add_(column_name_to_add),
      data_type_(real_data_type),
      type_info_(type_info),
      column_attributes_(column_attributes),
      col_position_identifier_(col_position_identifier) {}
}  // namespace object

}  // namespace etransfer