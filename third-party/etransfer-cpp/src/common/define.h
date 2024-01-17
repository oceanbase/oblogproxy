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
#include <stdint.h>
namespace etransfer {
namespace common {

enum ObjectType {
  CREATE_TABLE_OBJECT,
  INDEX_OBJECT,
  COMMENT_OBJECT,
  TABLE_COLUMN_OBJECT,
  TABLE_PARTITION_OBJECT,
  TABLE_CONSTRAINT_OBJECT,
  TABLE_REFERENCE_CONSTRAINT_OBJECT,
  TABLE_CHECK_CONSTRAINT_OBJECT,
  SELECT_OBJECT,
  VIEW_OBJECT,
  ALTER_TABLE_OBJECT,
  RENAME_TABLE_OBJECT,
  RENAME_TABLE_COLUMN_OBJECT,
  RENAME_INDEX_OBJECT,
  DROP_TABLE_OBJECT,
  DROP_INDEX_OBJECT,
  DROP_TABLE_CONSTRAINT_OBJECT,
  DROP_TABLE_COLUMN_OBJECT,
  DROP_TABLE_PARTITION_OBJECT,
  TRUNCATE_TABLE_OBJECT,
  TRUNCATE_TABLE_PARTITION_OBJECT,
  ALTER_TABLE_COLUMN_OBJECT,
  ALTER_TABLE_CONSTRAINT_OBJECT,
  ALTER_TABLE_PARTITION_OBJECT,
  ALTER_TABLE_OPTION_OBJECT,
  MODIFY_TABLE_CONSTRAINT_OBJECT,
  NOT_INTEREST
};

// IndexType includes index and constraint type
enum IndexType {
  PRIMARY,
  UNIQUE,
  NORMAL,
  FOREIGN,
  CHECK,
  FULLTEXT,
  SPATIAL,
  UNKNOWN
};

enum class OptionType {
  INDEX_SCOPE,
  INDEX_SCAN_ORDER,
  INDEX_HINT_COLUMN,
  INDEX_IMPL_ALGORITHM,
  INDEX_VISIBILITY,
  COMMENT_OPTION,
  OBJECT_OWNER,
  AUTO_STEP,
  DROP_TABLE_CASCADE_CONSTRAINTS,
  SELECT_SPACE_OPTION,
  TABLE_CHARSET,
  TABLE_COLLATION,
  UNINTERESTED
};

enum class ExprTokenType {
  NORMAL_CHARACTER,
  IDENTIFIER_NAME,
  FUNCTION,
  BINARY_EXPRESSION,
  SELECT
};

// database version define
#define MAJOR_SHIFT 16
#define MINOR_SHIFT 8

#define CALC_VERSION(major, minor, patch) \
  (((major) << MAJOR_SHIFT) + ((minor) << MINOR_SHIFT) + ((patch)))
constexpr static inline uint32_t CalVersion(const uint32_t major,
                                            const uint32_t minor,
                                            const uint32_t patch) {
  return CALC_VERSION(major, minor, patch);
}

#define MYSQL_55_VERSION (etransfer::common::CalVersion(5, 5, 0))
#define MYSQL_56_VERSION (etransfer::common::CalVersion(5, 6, 0))
#define MYSQL_57_VERSION (etransfer::common::CalVersion(5, 7, 0))
#define MYSQL_80_VERSION (etransfer::common::CalVersion(8, 0, 0))
#define MYSQL_5722_VERSION (etransfer::common::CalVersion(5, 7, 22))
#define MYSQL_5713_VERSION (etransfer::common::CalVersion(5, 7, 13))
#define MYSQL_5708_VERSION (etransfer::common::CalVersion(5, 7, 8))

};  // namespace common
};  // namespace etransfer