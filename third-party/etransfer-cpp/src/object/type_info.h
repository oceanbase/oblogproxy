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
#include <string>
#include <vector>

#include "common/data_type.h"
#include "common/util.h"
namespace etransfer {
namespace object {
using namespace common;
class TypeInfo {
 public:
  // used for no defined part, eg number(1)，precision is anonymous
  const static int ANONYMOUS_NUMBER;
  // by default, it is any length  0x7fffffffffffffffL = long max value
  const static long ARBITRARY_LENGTH;

  const static int MAX_NVARCHAR2_LEN;

  virtual GenericDataType DataType() = 0;
};

class TypeInfoWithLength : public TypeInfo {
 public:
  long length;
  long default_length;
  TypeInfoWithLength(long length, long default_length)
      : length(length), default_length(default_length) {}
};

class NumericTypeInfo : public TypeInfoWithLength {
 public:
  int precision;
  bool sign;
  bool zero_fill;

  NumericTypeInfo(long length, int precision)
      : TypeInfoWithLength(length, length),
        precision(precision),
        sign(true),
        zero_fill(false) {}

  NumericTypeInfo(long length, int precision, bool sign, bool zero_fill)
      : TypeInfoWithLength(length, length),
        precision(precision),
        sign(sign),
        zero_fill(zero_fill) {}

  GenericDataType DataType() { return GenericDataType::GENERIC_NUMERIC; }
};

class FloatingTypeInfo : public NumericTypeInfo {
 public:
  const static FloatingTypeInfo default_float_info;
  const static FloatingTypeInfo default_double_info;

  bool is_double_precision;

  FloatingTypeInfo(long length, bool is_double_precision)
      : FloatingTypeInfo(length, ANONYMOUS_NUMBER, is_double_precision) {}

  FloatingTypeInfo(long length, int precision, bool is_double_precision)
      : NumericTypeInfo(length, precision),
        is_double_precision(is_double_precision) {}

  GenericDataType DataType() { return GenericDataType::GENERIC_FLOATING; }
};

class BooleanTypeInfo : public TypeInfo {
 public:
  const static BooleanTypeInfo default_boolean_info;

  BooleanTypeInfo() {}

  GenericDataType DataType() { return GenericDataType::GENERIC_BOOLEAN; }
};

class TimeTypeInfo : public TypeInfoWithLength {
 public:
  const static TimeTypeInfo default_time_info;
  int nos_precision;
  int time_zone_info;
  bool sign;
  bool zerofill;
  TimeTypeInfo(int nos_precision)
      : TypeInfoWithLength(ANONYMOUS_NUMBER, ANONYMOUS_NUMBER),
        nos_precision(nos_precision),
        time_zone_info(0),
        sign(false),
        zerofill(false) {}

  TimeTypeInfo(int nos_precision, int time_zone_info, long length)
      : TypeInfoWithLength(length, length),
        nos_precision(nos_precision),
        time_zone_info(time_zone_info),
        sign(false),
        zerofill(false) {}
  GenericDataType DataType() { return GenericDataType::GENERIC_TIME; }
};

class CharacterTypeInfo : public TypeInfoWithLength {
 public:
  const static CharacterTypeInfo default_character_info;
  // 1 for byte
  // 2 for graphic or unicode
  // 4 for nchar 4 bytes
  int store_unit;
  // may octs/char/byte/octunit32/octunit16... allow absent
  std::string origin_unit;

  bool store_in_binary;

  std::string charset;
  std::string collation;

  CharacterTypeInfo(long length, int store_unit, const std::string& origin_unit,
                    bool store_in_binary)
      : CharacterTypeInfo(length, length, store_unit, origin_unit,
                          store_in_binary) {}

  CharacterTypeInfo(long length, long default_length, int store_unit,
                    const std::string& origin_unit, bool store_in_binary)
      : CharacterTypeInfo(length, default_length, "", "", store_in_binary,
                          store_unit, origin_unit) {}

  CharacterTypeInfo(long length, long default_length,
                    const std::string& charset, const std::string& collation,
                    bool store_in_binary, int store_unit)
      : CharacterTypeInfo(length, default_length, charset, collation,
                          store_in_binary, store_unit, "") {}

  CharacterTypeInfo(long length, long default_length,
                    const std::string& charset, const std::string& collation,
                    bool store_in_binary, int store_unit,
                    const std::string& origin_unit)
      : TypeInfoWithLength(length, default_length),
        store_unit(store_unit),
        origin_unit(origin_unit),
        charset(charset),
        collation(collation),
        store_in_binary(store_in_binary) {}

  CharacterTypeInfo(int length, int default_length, const std::string& charset,
                    const std::string& collation, bool store_in_binary)
      : CharacterTypeInfo(length, default_length, charset, collation,
                          store_in_binary, TypeInfo::ANONYMOUS_NUMBER) {}

  GenericDataType DataType() { return GenericDataType::GENERIC_CHARACTER; }
};
class LOBTypeInfo : public CharacterTypeInfo {
 public:
  const static LOBTypeInfo default_lob_info;

  // use storeInBinary in CharacterTypeInfo to distinguish blob and clob
  // blob is true/ other is false

  // use storeUnit in CharacterTypeInfo to means :
  //  1 for byte, 2 for graphic or unicode, 4 for nchar 4 bytes

  // may be byte/ char/ K / M / G ... allow absent
  std::string origin_unit;

  LOBTypeInfo(bool store_in_binary, long length, int store_unit,
              const std::string& origin_unit)
      : LOBTypeInfo(store_in_binary, length, length, store_unit, origin_unit) {}

  LOBTypeInfo(bool store_in_binary, long length, long default_length,
              int store_unit, const std::string& origin_unit)
      : LOBTypeInfo(length, default_length, "", "", store_in_binary, store_unit,
                    origin_unit) {}

  LOBTypeInfo(long length, long default_length, const std::string& charset,
              const std::string& collation, bool is_binary, int store_unit)
      : LOBTypeInfo(length, default_length, charset, collation, is_binary,
                    store_unit, "") {}

  LOBTypeInfo(long length, long default_length, const std::string& charset,
              const std::string& collation, bool is_binary, int store_unit,
              const std::string& origin_unit)
      : CharacterTypeInfo(length, default_length, charset, collation, is_binary,
                          store_unit),
        origin_unit(origin_unit) {}

  GenericDataType DataType() { return GenericDataType::GENERIC_LOB; }
};

class BinaryTypeInfo : public TypeInfoWithLength {
 public:
  const static BinaryTypeInfo default_binary_info;

  BinaryTypeInfo(long length) : TypeInfoWithLength(length, length) {}

  GenericDataType DataType() { return GenericDataType::GENERIC_BINARY; }
};

class SetTypeInfo : public TypeInfo {
 public:
  Strings elem_list;
  std::string charset;
  std::string collation;
  bool is_binary;

  GenericDataType DataType() { return GenericDataType::GENERIC_ENUM; }

  SetTypeInfo(const Strings& elem_list, const std::string& charset,
              const std::string& collation, bool is_binary)
      : elem_list(elem_list),
        charset(charset),
        collation(collation),
        is_binary(is_binary) {}
};

class GenericTypeInfo : public TypeInfo {
 public:
  GenericDataType generic_data_type;

  GenericDataType DataType() { return generic_data_type; }

  GenericTypeInfo(RealDataType real_data_type)
      : generic_data_type(DataTypeUtil::GetGenericDataType(real_data_type)) {}
};

}  // namespace object

}  // namespace etransfer