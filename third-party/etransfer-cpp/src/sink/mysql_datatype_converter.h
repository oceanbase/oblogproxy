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
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "common/data_type.h"
#include "convert/sql_builder_context.h"
#include "object/type_info.h"
namespace etransfer {
namespace sink {
using namespace common;
using namespace object;
typedef std::function<std::pair<RealDataType, std::string>(
    RealDataType, std::shared_ptr<BuildContext>, std::shared_ptr<TypeInfo>)>
    Converter;
using ConverterMapper = std::unordered_map<GenericDataType, Converter>;
class MySQLDataTypeConverterMapper {
 public:
  static const ConverterMapper mysql_data_type_converter;
  static const ConverterMapper& DatatypeConverterMapper() {
    return mysql_data_type_converter;
  }
  static ConverterMapper InitMapper();
  // 65535 bytes
  const static int mysql_varchar_max_length = 65535;
  // 65535 characters
  const static int mysql_text_max_length = 65535;

  static std::string BuildNumberTypeSuffix(
      std::shared_ptr<NumericTypeInfo> numeric_type_info,
      RealDataType real_data_type);
  static std::pair<RealDataType, std::string> ConvertNumeric(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);
  static std::pair<RealDataType, std::string> ConvertFloating(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);
  static std::string BuildFloatTypeSuffix(
      std::shared_ptr<FloatingTypeInfo> float_type_info);
  static std::pair<RealDataType, std::string> ConvertBool(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);
  static std::pair<RealDataType, std::string> ConvertTime(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);
  static std::string BuildCharTypeSuffix(
      std::shared_ptr<CharacterTypeInfo> type_info,
      std::shared_ptr<BuildContext> context);
  static std::pair<RealDataType, std::string> ConvertCharacter(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);
  static Converter GetConverter(GenericDataType generic_data_type);
  static bool IsNotSpecialCharacterSet(std::string charset);

  static std::pair<RealDataType, std::string> ConvertLob(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);

  static std::pair<RealDataType, std::string> ConvertBinary(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);

  static std::string BuildSetTypeSuffix(std::shared_ptr<SetTypeInfo> type_info);

  static std::pair<RealDataType, std::string> ConvertSet(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);

  static std::pair<RealDataType, std::string> ConvertGeometry(
      RealDataType real_data_type,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<TypeInfo> type_info);
};
}  // namespace sink

}  // namespace etransfer