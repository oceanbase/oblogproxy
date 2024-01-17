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

#include "object/alter_table_object.h"

namespace etransfer {
namespace object {
AlterTableObject::AlterTableObject(const Catalog& catalog,
                                   const RawConstant& object_name,
                                   const std::string& raw_ddl)
    : Object(catalog, object_name, raw_ddl, ObjectType::ALTER_TABLE_OBJECT) {}
}  // namespace object

}  // namespace etransfer
