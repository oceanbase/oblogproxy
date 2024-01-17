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
namespace etransfer {
namespace common {
enum class GenericDataType {
  GENERIC_BOOLEAN,
  GENERIC_NUMERIC,
  GENERIC_FLOATING,
  GENERIC_TIME,
  GENERIC_INTERVAL,
  GENERIC_CHARACTER,
  GENERIC_LOB,
  GENERIC_LONG_OBJ,
  GENERIC_BINARY,
  GENERIC_RAW,
  GENERIC_ROW_ID,
  GENERIC_XML,
  GENERIC_ENUM,
  GENERIC_SET,
  GENERIC_USER_DEFINED,
  GENERIC_GEOMETRY,
  UNKNOWN
};
enum RealDataType {
  // generic number data type
  TINYINT = 0,
  SMALLINT,
  MEDIUMINT,
  INTEGER,
  BIGINT,
  DECIMAL,
  NUMERIC,
  DECFLOAT,
  NUMBER,
  FLOATING_NUMBER,

  // generic floating data type
  REAL,
  FLOAT,
  DOUBLE,
  BINARY_FLOAT,
  BINARY_DOUBLE,

  // generic boolean type
  BOOLEAN,

  // generic character data type
  CHAR,
  VARCHAR,
  GRAPHIC,
  NCHAR,
  VARGRAPHIC,
  NVARCHAR,
  VARCHAR2,
  NVARCHAR2,
  CITEXT,

  // generic time data type
  DATE,
  TIME,
  DATETIME,
  TIMESTAMP,
  TIMESTAMP_TZ,
  TIMESTAMP_LTZ,
  YEAR,
  MONTH,
  DAY,
  HOUR,
  MINUTE,
  SECOND,
  TIMEZONE_HOUR,
  TIMEZONE_MINUTE,
  TIMEZONE_REGION,
  TIMEZONE_ABBR,
  TIMESTAMP_UNCONSTRAINED,
  TIMESTAMP_TZ_UNCONSTRAINED,
  TIMESTAMP_LTZ_UNCONSTRAINED,

  // generic interval data type
  YMINTERVAL_UNCONSTRAINED,
  DSINTERVAL_UNCONSTRAINED,
  INTERVAL_YEAR_TO_MONTH,
  INTERVAL_DAY_TO_SECOND,

  // generic lob data type
  TINYBLOB,
  BLOB,
  MEDIUMBLOB,
  LONGBLOB,
  CLOB,
  DBCLOB,
  NCLOB,
  TINYTEXT,
  TEXT,
  MEDIUMTEXT,
  LONGTEXT,

  //  generic long obj
  LONGVARG,
  LONGVARCHAR,
  LONG_OBJ,
  BFILE,

  // generic binary
  BINRAY,
  VARBINARY,
  BIT,

  // generic raw
  RAW,

  // generic row id
  ROW_ID,
  UROWID,

  // xml
  XML,

  ENUM,
  SET,

  USER_DEFINED_TYPE,

  SERIAL,

  JSON,

  GEOMETRY,

  GEOMETRYCOLLECTION,

  POINT,

  MULTIPOINT,

  LINESTRING,

  MULTILINESTRING,

  POLYGON,

  MULTIPOLYGON,

  // .... future more type
  UNSUPPORTED,

  SIZE_OF_REAL_DATA_TYPE
};

class DataTypeUtil {
 public:
  static const char* const data_type_names[];
  static const char* GetTypeName(RealDataType real_data_type);
  static GenericDataType GetGenericDataType(RealDataType real_data_type);
};

};  // namespace common
};  // namespace etransfer