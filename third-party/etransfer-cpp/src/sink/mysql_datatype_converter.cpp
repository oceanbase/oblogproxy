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

#include "sink/mysql_datatype_converter.h"

#include "common/define.h"
#include "common/util.h"
#include "object/type_info.h"
namespace etransfer {
namespace sink {
ConverterMapper MySQLDataTypeConverterMapper::InitMapper() {
  ConverterMapper mapper;
  mapper[GenericDataType::GENERIC_NUMERIC] =
      MySQLDataTypeConverterMapper::ConvertNumeric;
  mapper[GenericDataType::GENERIC_FLOATING] =
      MySQLDataTypeConverterMapper::ConvertFloating;
  mapper[GenericDataType::GENERIC_BOOLEAN] =
      MySQLDataTypeConverterMapper::ConvertBool;
  mapper[GenericDataType::GENERIC_TIME] =
      MySQLDataTypeConverterMapper::ConvertTime;
  mapper[GenericDataType::GENERIC_CHARACTER] =
      MySQLDataTypeConverterMapper::ConvertCharacter;
  mapper[GenericDataType::GENERIC_LOB] =
      MySQLDataTypeConverterMapper::ConvertLob;
  mapper[GenericDataType::GENERIC_BINARY] =
      MySQLDataTypeConverterMapper::ConvertBinary;
  mapper[GenericDataType::GENERIC_SET] =
      MySQLDataTypeConverterMapper::ConvertSet;
  mapper[GenericDataType::GENERIC_ENUM] =
      MySQLDataTypeConverterMapper::ConvertSet;
  mapper[GenericDataType::GENERIC_GEOMETRY] =
      MySQLDataTypeConverterMapper::ConvertGeometry;
  mapper[GenericDataType::GENERIC_LOB] =
      MySQLDataTypeConverterMapper::ConvertLob;

  return mapper;
}
Converter MySQLDataTypeConverterMapper::GetConverter(
    GenericDataType generic_data_type) {
  Converter converter;
  auto converter_iter = mysql_data_type_converter.find(generic_data_type);
  if (converter_iter != mysql_data_type_converter.end()) {
    return converter_iter->second;
  }
  return converter;
}
const ConverterMapper MySQLDataTypeConverterMapper::mysql_data_type_converter =
    MySQLDataTypeConverterMapper::InitMapper();
std::string MySQLDataTypeConverterMapper::BuildNumberTypeSuffix(
    std::shared_ptr<NumericTypeInfo> numeric_type_info,
    RealDataType real_data_type) {
  std::string builder;
  if (numeric_type_info->length == TypeInfo::ANONYMOUS_NUMBER &&
      numeric_type_info->precision == TypeInfo::ANONYMOUS_NUMBER) {
    // nothing to do
    builder.append("");
  } else if (numeric_type_info->precision == TypeInfo::ANONYMOUS_NUMBER) {
    long length;
    switch (real_data_type) {
      case TINYINT:
      case SMALLINT:
      case MEDIUMINT:
      case INTEGER:
      case BIGINT:
        if (numeric_type_info->length > 255) {
          length = 255;
          break;
        }
        length = numeric_type_info->length;
        break;
      default:
        length = numeric_type_info->length;
    }
    builder.append(" (").append(std::to_string(length)).append(")");
  } else {
    builder.append(" (")
        .append(std::to_string(numeric_type_info->length))
        .append(", ")
        .append(std::to_string(numeric_type_info->precision))
        .append(")");
  }

  if (!numeric_type_info->sign) {
    builder.append(" UNSIGNED");
  }

  if (numeric_type_info->zero_fill) {
    builder.append(" ZEROFILL");
  }

  return builder;
}

std::pair<RealDataType, std::string>
MySQLDataTypeConverterMapper::ConvertNumeric(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto numeric_type_info =
      std::dynamic_pointer_cast<NumericTypeInfo>(type_info);
  std::string suffix = BuildNumberTypeSuffix(numeric_type_info, real_data_type);
  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type) + suffix);
}

std::pair<RealDataType, std::string>
MySQLDataTypeConverterMapper::ConvertFloating(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto float_type_info = std::dynamic_pointer_cast<FloatingTypeInfo>(type_info);
  std::string suffix = BuildFloatTypeSuffix(float_type_info);
  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type) + suffix);
}

std::string MySQLDataTypeConverterMapper::BuildFloatTypeSuffix(
    std::shared_ptr<FloatingTypeInfo> float_type_info) {
  std::string builder;
  if (float_type_info->length == TypeInfo::ANONYMOUS_NUMBER &&
      float_type_info->precision == TypeInfo::ANONYMOUS_NUMBER) {
    // nothing to do
  } else if (float_type_info->precision == TypeInfo::ANONYMOUS_NUMBER) {
    builder.append(" (")
        .append(std::to_string(float_type_info->length))
        .append(")");
  } else {
    builder.append(" (")
        .append(std::to_string(float_type_info->length))
        .append(", ")
        .append(std::to_string(float_type_info->precision))
        .append(")");
  }
  return builder;
}

std::pair<RealDataType, std::string> MySQLDataTypeConverterMapper::ConvertBool(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type));
}

std::pair<RealDataType, std::string> MySQLDataTypeConverterMapper::ConvertTime(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto time_type_info = std::dynamic_pointer_cast<TimeTypeInfo>(type_info);

  std::string suffix;
  switch (real_data_type) {
    case YEAR:
      if (time_type_info->length != TypeInfo::ANONYMOUS_NUMBER) {
        suffix.append(" (")
            .append(std::to_string(time_type_info->length))
            .append(")");
      }
      break;
    case TIMESTAMP:
      if (sql_builder_context->target_db_version < MYSQL_56_VERSION &&
          time_type_info->nos_precision != TypeInfo::ANONYMOUS_NUMBER) {
        std::cerr
            << "mysql timestamp don't allow to specify precision before 5.6"
            << std::endl;
        suffix = "";
        break;
      }
    case TIME:
    case DATETIME:
      if (sql_builder_context->target_db_version < MYSQL_56_VERSION &&
          time_type_info->nos_precision != TypeInfo::ANONYMOUS_NUMBER) {
        std::cerr << "mysql don't allow to specify precision before 5.6"
                  << std::endl;
        suffix = "";
        break;
      }
    default:
      if (time_type_info->nos_precision != TypeInfo::ANONYMOUS_NUMBER) {
        suffix.append(" (")
            .append(std::to_string(time_type_info->nos_precision))
            .append(")");
      }
  }
  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type) + suffix);
}

std::string MySQLDataTypeConverterMapper::BuildCharTypeSuffix(
    std::shared_ptr<CharacterTypeInfo> type_info,
    std::shared_ptr<BuildContext> context) {
  std::string builder;
  if (type_info->length != TypeInfo::ANONYMOUS_NUMBER) {
    builder.append(" (").append(std::to_string(type_info->length)).append(")");
  }

  if (type_info->store_in_binary) {
    builder.append(" BINARY");
  }

  if (type_info->charset != "") {
    builder
        .append(IsNotSpecialCharacterSet(type_info->charset) ? " CHARSET "
                                                             : " ")
        .append(type_info->charset);
  }

  if (type_info->collation != "") {
    builder.append(" COLLATE ").append(type_info->collation);
  }

  return builder;
}

bool MySQLDataTypeConverterMapper::IsNotSpecialCharacterSet(
    std::string charset) {
  return !(Util::EqualsIgnoreCase(charset, "ascii") ||
           Util::EqualsIgnoreCase(charset, "unicode") ||
           Util::EqualsIgnoreCase(charset, "byte"));
}

std::pair<RealDataType, std::string>
MySQLDataTypeConverterMapper::ConvertCharacter(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto char_type_info = std::dynamic_pointer_cast<CharacterTypeInfo>(type_info);

  // max length of VARCHAR in MySQL is 64K (i.e. 65535 bytes),
  if (real_data_type == RealDataType::VARCHAR &&
      char_type_info->length * 4 >= mysql_varchar_max_length) {
    // ob mysql varchar(n) n [16383, 65535] --> mysql text
    // ob mysql varchar(n) n [65535, 262144] --> mysql mediumtext
    real_data_type = char_type_info->length > mysql_text_max_length
                         ? RealDataType::MEDIUMTEXT
                         : RealDataType::TEXT;
    char_type_info = std::make_shared<CharacterTypeInfo>(
        TypeInfo::ANONYMOUS_NUMBER, TypeInfo::ANONYMOUS_NUMBER,
        char_type_info->charset, char_type_info->collation,
        char_type_info->store_in_binary);
  }
  if (real_data_type == RealDataType::NCHAR) {
    real_data_type = RealDataType::CHAR;
  }
  if (real_data_type == RealDataType::NVARCHAR) {
    real_data_type = RealDataType::VARCHAR;
  }
  std::string suffix = BuildCharTypeSuffix(char_type_info, sql_builder_context);

  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type) + suffix);
}

std::pair<RealDataType, std::string> MySQLDataTypeConverterMapper::ConvertLob(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto lob_type_info = std::dynamic_pointer_cast<LOBTypeInfo>(type_info);
  std::string suffix;
  switch (real_data_type) {
    case TINYTEXT:
    case TEXT:
    case MEDIUMTEXT:
    case LONGTEXT:
      suffix = BuildCharTypeSuffix(lob_type_info, sql_builder_context);
      break;
    case TINYBLOB:
    case BLOB:
    case MEDIUMBLOB:
    case LONGBLOB:
      break;
    default:
      sql_builder_context->SetErrMsg(
          "mysql doesn't support to convert data type");
  }
  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type) + suffix);
}

std::pair<RealDataType, std::string>
MySQLDataTypeConverterMapper::ConvertBinary(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto bin_type_info = std::dynamic_pointer_cast<BinaryTypeInfo>(type_info);

  std::string length;
  if (bin_type_info->length != TypeInfo::ANONYMOUS_NUMBER) {
    length.append(" (")
        .append(std::to_string(bin_type_info->length))
        .append(")");
  }
  return std::make_pair(real_data_type,
                        DataTypeUtil::GetTypeName(real_data_type) + length);
}

std::string MySQLDataTypeConverterMapper::BuildSetTypeSuffix(
    std::shared_ptr<SetTypeInfo> type_info) {
  std::string builder("(");

  for (const auto& elem : type_info->elem_list) {
    builder.append("'").append(elem).append("',");
  }
  builder = builder.substr(0, builder.length() - 1);
  builder.append(")");
  if (type_info->is_binary) {
    builder.append(" BINARY");
  }

  if (type_info->charset != "") {
    builder.append(" CHARSET ").append(type_info->charset);
  }

  if (type_info->collation != "") {
    builder.append(" COLLATE ").append(type_info->collation);
  }

  return builder;
}

std::pair<RealDataType, std::string> MySQLDataTypeConverterMapper::ConvertSet(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  auto set_type_info = std::dynamic_pointer_cast<SetTypeInfo>(type_info);
  return std::make_pair(RealDataType::VARCHAR,
                        DataTypeUtil::GetTypeName(real_data_type) +
                            BuildSetTypeSuffix(set_type_info));
}

std::pair<RealDataType, std::string>
MySQLDataTypeConverterMapper::ConvertGeometry(
    RealDataType real_data_type,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<TypeInfo> type_info) {
  switch (real_data_type) {
    case GEOMETRY:
    case GEOMETRYCOLLECTION:
    case POINT:
    case MULTIPOINT:
    case POLYGON:
    case MULTIPOLYGON:
    case LINESTRING:
    case MULTILINESTRING:
      return std::make_pair(real_data_type,
                            DataTypeUtil::GetTypeName(real_data_type));
    default:
      return std::make_pair(RealDataType::UNSUPPORTED, "");
  }
}

}  // namespace sink

}  // namespace etransfer