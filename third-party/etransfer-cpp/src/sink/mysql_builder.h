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
#include "sink/sql_builder.h"
namespace etransfer {
namespace sink {
// MySqlBuilder convert intermediate object to MySQL fully compatible DDL
// statement.
class MySqlBuilder : public SqlBuilder {
  Strings RealApplySchemaObject(ObjectPtr db_object,
                                std::shared_ptr<BuildContext> context);
};
}  // namespace sink
}  // namespace etransfer