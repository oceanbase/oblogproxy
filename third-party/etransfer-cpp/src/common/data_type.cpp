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

#include "common/data_type.h"
namespace etransfer {
namespace common {
const char* const DataTypeUtil::data_type_names[] = {
    "TINYINT",
    "SMALLINT",
    "MEDIUMINT",
    "INTEGER",
    "BIGINT",
    "DECIMAL",
    "NUMERIC",
    "DECFLOAT",
    "NUMBER",
    "FLOATING",
    "REAL",
    "FLOAT",
    "DOUBLE",
    "BINARY_FLOAT",
    "BINARY_DOUBLE",
    "BOOLEAN",
    "CHAR",
    "VARCHAR",
    "GRAPHIC",
    "NCHAR",
    "VARGRAPHIC",
    "NVARCHAR",
    "VARCHAR2",
    "NVARCHAR2",
    "CITEXT",
    "DATE",
    "TIME",
    "DATETIME",
    "TIMESTAMP",
    "TIMESTAMP WITH TIME ZONE",
    "TIMESTAMP WITH LOCAL TIME ZONE",
    "YEAR",
    "MONTH",
    "DAY",
    "HOUR",
    "MINUTE",
    "SECOND",
    "TIMEZONE_HOUR",
    "TIMEZONE_MINUTE",
    "TIMEZONE_REGION",
    "TIMEZONE_ABBR",
    "TIMESTAMP_UNCONSTRAINED",
    "TIMESTAMP_TZ_UNCONSTRAINED",
    "TIMESTAMP_LTZ_UNCONSTRAINED",
    "YMINTERVAL_UNCONSTRAINED",
    "DSINTERVAL_UNCONSTRAINED",
    "INTERVAL_YEAR_TO_MONTH",
    "INTERVAL_DAY_TO_SECOND",
    "TINYBLOB",
    "BLOB",
    "MEDIUMBLOB",
    "LONGBLOB",
    "CLOB",
    "DBCLOB",
    "NCLOB",
    "TINYTEXT",
    "TEXT",
    "MEDIUMTEXT",
    "LONGTEXT",
    "LONG VARG",
    "LONG VARCHAR",
    "LONG",
    "BFILE",
    "BINARY",
    "VARBINARY",
    "BIT",
    "RAW",
    "ROWID",
    "UROWID",
    "XML",
    "ENUM",
    "SET",
    "",
    "SERIAL",
    "JSON",
    "GEOMETRY",
    "GEOMETRYCOLLECTION",
    "POINT",
    "MULTIPOINT",
    "LINESTRING",
    "MULTILINESTRING",
    "POLYGON",
    "MULTIPOLYGON",
    ""};

static_assert(sizeof(DataTypeUtil::data_type_names) / sizeof(char*) ==
                  RealDataType::SIZE_OF_REAL_DATA_TYPE,
              "RealDataType sizes don't match");

const char* DataTypeUtil::GetTypeName(RealDataType real_data_type) {
  return data_type_names[real_data_type];
}

GenericDataType DataTypeUtil::GetGenericDataType(RealDataType real_data_type) {
  switch (real_data_type) {
    case TINYINT:
    case SMALLINT:
    case MEDIUMINT:
    case INTEGER:
    case BIGINT:
    case DECIMAL:
    case NUMERIC:
    case DECFLOAT:
    case NUMBER:
    case FLOATING_NUMBER:
      return GenericDataType::GENERIC_NUMERIC;
    case REAL:
    case FLOAT:
    case DOUBLE:
    case BINARY_FLOAT:
    case BINARY_DOUBLE:
      return GenericDataType::GENERIC_FLOATING;
    case BOOLEAN:
      return GenericDataType::GENERIC_BOOLEAN;
    case CHAR:
    case VARCHAR:
    case GRAPHIC:
    case NCHAR:
    case VARGRAPHIC:
    case NVARCHAR:
    case VARCHAR2:
    case NVARCHAR2:
    case CITEXT:
      return GenericDataType::GENERIC_CHARACTER;
    case DATE:
    case TIME:
    case DATETIME:
    case TIMESTAMP:
    case TIMESTAMP_TZ:
    case TIMESTAMP_LTZ:
    case YEAR:
    case MONTH:
    case DAY:
    case HOUR:
    case MINUTE:
    case SECOND:
    case TIMEZONE_HOUR:
    case TIMEZONE_MINUTE:
    case TIMEZONE_REGION:
    case TIMEZONE_ABBR:
    case TIMESTAMP_UNCONSTRAINED:
    case TIMESTAMP_TZ_UNCONSTRAINED:
    case TIMESTAMP_LTZ_UNCONSTRAINED:
      return GenericDataType::GENERIC_TIME;
    case YMINTERVAL_UNCONSTRAINED:
    case DSINTERVAL_UNCONSTRAINED:
    case INTERVAL_YEAR_TO_MONTH:
    case INTERVAL_DAY_TO_SECOND:
      return GenericDataType::GENERIC_INTERVAL;
    case TINYBLOB:
    case BLOB:
    case MEDIUMBLOB:
    case LONGBLOB:
    case CLOB:
    case DBCLOB:
    case NCLOB:
    case TINYTEXT:
    case TEXT:
    case MEDIUMTEXT:
    case LONGTEXT:
      return GenericDataType::GENERIC_LOB;
    case LONGVARG:
    case LONGVARCHAR:
    case LONG_OBJ:
    case BFILE:
      return GenericDataType::GENERIC_LONG_OBJ;
    case BINRAY:
    case VARBINARY:
    case BIT:
      return GenericDataType::GENERIC_BINARY;
    case RAW:
      return GenericDataType::GENERIC_RAW;
    case ROW_ID:
    case UROWID:
      return GenericDataType::GENERIC_ROW_ID;
    case XML:
      return GenericDataType::GENERIC_XML;
    case ENUM:
      return GenericDataType::GENERIC_ENUM;
    case SET:
      return GenericDataType::GENERIC_SET;
    case USER_DEFINED_TYPE:
      return GenericDataType::GENERIC_USER_DEFINED;
    case SERIAL:
      return GenericDataType::GENERIC_NUMERIC;
    case JSON:
      return GenericDataType::GENERIC_CHARACTER;
    case GEOMETRY:
    case GEOMETRYCOLLECTION:
    case POINT:
    case MULTIPOINT:
    case LINESTRING:
    case MULTILINESTRING:
    case POLYGON:
    case MULTIPOLYGON:
      return GenericDataType::GENERIC_GEOMETRY;
    case UNSUPPORTED:
      return GenericDataType::UNKNOWN;
    default:
      return GenericDataType::UNKNOWN;
  }
}
}  // namespace common

}  // namespace etransfer
