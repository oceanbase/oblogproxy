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

#include "source/type_helper.h"

#include <iostream>

#include "common/util.h"
#include "convert/ddl_parser_context.h"
#include "object/type_info.h"
namespace etransfer {
namespace source {
std::pair<RealDataType, std::shared_ptr<TypeInfo>> TypeHelper::ParseDataType(
    OBParser::Data_typeContext* ctx) {
  if (ctx->int_type_i() != nullptr) {
    // int
    long length = ctx->INTNUM().size() == 0
                      ? TypeInfo::ANONYMOUS_NUMBER
                      : std::stol(ctx->INTNUM(0)->getText());
    NumericTypeInfo type_info(length, TypeInfo::ANONYMOUS_NUMBER,
                              ctx->UNSIGNED() == nullptr,
                              ctx->ZEROFILL() != nullptr);

    std::string int_sting = ctx->int_type_i()->getText();
    RealDataType data_type = RealDataType::UNSUPPORTED;
    if (Util::EqualsIgnoreCase(int_sting, "TINYINT")) {
      data_type = RealDataType::TINYINT;
    } else if (Util::EqualsIgnoreCase(int_sting, "SMALLINT")) {
      data_type = RealDataType::SMALLINT;
    } else if (Util::EqualsIgnoreCase(int_sting, "MEDIUMINT")) {
      data_type = RealDataType::MEDIUMINT;
    } else if (Util::EqualsIgnoreCase(int_sting, "INT") ||
               Util::EqualsIgnoreCase(int_sting, "INTEGER")) {
      data_type = RealDataType::INTEGER;
    } else if (Util::EqualsIgnoreCase(int_sting, "BIGINT")) {
      data_type = RealDataType::BIGINT;
    }
    return std::make_pair(data_type,
                          std::make_shared<NumericTypeInfo>(type_info));
  } else if (ctx->NUMBER() != nullptr || ctx->DECIMAL() != nullptr ||
             ctx->FIXED() != nullptr) {
    // number/decimal
    int length = ctx->INTNUM().size() > 0 ? std::stoi(ctx->INTNUM(0)->getText())
                                          : TypeInfo::ANONYMOUS_NUMBER;
    int precision = ctx->INTNUM().size() > 1
                        ? std::stoi(ctx->INTNUM(1)->getText())
                        : TypeInfo::ANONYMOUS_NUMBER;

    NumericTypeInfo type_info(length, precision, ctx->UNSIGNED() == nullptr,
                              ctx->ZEROFILL() != nullptr);

    if (ctx->NUMBER() != nullptr) {
      std::string type_string = ctx->NUMBER()->getText();
      if (Util::EqualsIgnoreCase(type_string, "DEC") ||
          Util::EqualsIgnoreCase(type_string, "NUMBER")) {
        return std::make_pair(RealDataType::DECIMAL,
                              std::make_shared<NumericTypeInfo>(type_info));
      } else if (Util::EqualsIgnoreCase(type_string, "NUMERIC")) {
        return std::make_pair(RealDataType::NUMERIC,
                              std::make_shared<NumericTypeInfo>(type_info));
      }
      // fixed is DECIMAL
    } else if (ctx->DECIMAL() != nullptr || nullptr != ctx->FIXED()) {
      return std::make_pair(RealDataType::DECIMAL,
                            std::make_shared<NumericTypeInfo>(type_info));
    }
  } else if (ctx->float_type_i() != nullptr) {
    int length = (ctx->INTNUM().size() > 0)
                     ? std::stoi(ctx->INTNUM(0)->getText())
                     : TypeInfo::ANONYMOUS_NUMBER;
    int precision = ctx->INTNUM().size() > 1
                        ? std::stoi(ctx->INTNUM(1)->getText())
                        : TypeInfo::ANONYMOUS_NUMBER;
    std::string type_string = ctx->float_type_i()->getText();
    if (Util::EqualsIgnoreCase(type_string, "FLOAT")) {
      // in ob mysql or mysql, The float length specification determine
      // storage size. A precision from 0 to 24 results in a 4-byte
      // single-precision FLOAT column.
      // A precision from 25 to 53 results in an 8-byte double-precision
      // DOUBLE column.
      if (length == TypeInfo::ANONYMOUS_NUMBER &&
          precision == TypeInfo::ANONYMOUS_NUMBER) {
        return std::make_pair(RealDataType::FLOAT,
                              std::make_shared<FloatingTypeInfo>(
                                  FloatingTypeInfo::default_float_info));
      } else if (precision == TypeInfo::ANONYMOUS_NUMBER) {
        if (length >= 25) {
          return std::make_pair(RealDataType::DOUBLE,
                                std::make_shared<FloatingTypeInfo>(
                                    TypeInfo::ANONYMOUS_NUMBER, true));
        } else {
          return std::make_pair(RealDataType::FLOAT,
                                std::make_shared<FloatingTypeInfo>(
                                    FloatingTypeInfo::default_float_info));
        }
      } else {
        return std::make_pair(
            RealDataType::FLOAT,
            std::make_shared<FloatingTypeInfo>(length, precision, false));
      }
    } else if (Util::ContainsIgnoreCase(type_string, "DOUBLE")) {
      // DOUBLE/DOUBLE PRECISION
      return std::make_pair(
          RealDataType::DOUBLE,
          std::make_shared<FloatingTypeInfo>(length, precision, true));
    } else if (Util::ContainsIgnoreCase(type_string, "REAL")) {
      // REAL/REAL PRECISION
      // in ob mysql , REAL is a synonym for DOUBLE PRECISION
      return std::make_pair(
          RealDataType::DOUBLE,
          std::make_shared<FloatingTypeInfo>(length, precision, true));
    }
  } else if (ctx->BOOL() != nullptr || ctx->BOOLEAN() != nullptr) {
    // bool/boolean
    return std::make_pair<RealDataType, std::shared_ptr<TypeInfo>>(
        RealDataType::BOOLEAN, std::make_shared<BooleanTypeInfo>(
                                   BooleanTypeInfo::default_boolean_info));
  } else if (ctx->datetime_type_i() != nullptr) {
    // datetime
    int precision = ctx->INTNUM().size() > 0
                        ? std::stoi(ctx->INTNUM(0)->getText())
                        : TypeInfo::ANONYMOUS_NUMBER;
    std::string type_string = ctx->datetime_type_i()->getText();
    if (Util::EqualsIgnoreCase(type_string, "DATETIME")) {
      return std::make_pair(RealDataType::DATETIME,
                            std::make_shared<TimeTypeInfo>(precision));
    } else if (Util::EqualsIgnoreCase(type_string, "TIMESTAMP")) {
      return std::make_pair(RealDataType::TIMESTAMP,
                            std::make_shared<TimeTypeInfo>(precision));
    } else if (Util::EqualsIgnoreCase(type_string, "TIME")) {
      return std::make_pair(RealDataType::TIME,
                            std::make_shared<TimeTypeInfo>(precision));
    }
  } else if (ctx->date_year_type_i() != nullptr) {
    // date year
    if (ctx->date_year_type_i()->DATE() != nullptr) {
      return std::make_pair(
          RealDataType::DATE,
          std::make_shared<TimeTypeInfo>(TimeTypeInfo::default_time_info));
    } else if (ctx->date_year_type_i()->YEAR() != nullptr) {
      int length =
          ctx->date_year_type_i()->INTNUM() == nullptr
              ? TypeInfo::ANONYMOUS_NUMBER
              : std::stoi(ctx->date_year_type_i()->INTNUM()->getText());
      return std::make_pair(
          RealDataType::YEAR,
          std::make_shared<TimeTypeInfo>(TypeInfo::ANONYMOUS_NUMBER,
                                         TypeInfo::ANONYMOUS_NUMBER, length));
    }
  } else if (ctx->CHARACTER() != nullptr || ctx->VARCHAR() != nullptr ||
             ctx->text_type_i() != nullptr) {
    // char/varchar
    int length =
        ctx->string_length_i() == nullptr
            ? TypeInfo::ANONYMOUS_NUMBER
            : std::stoi(ctx->string_length_i()->number_literal()->getText());
    std::string charset =
        ctx->charset_name() == nullptr ? "" : ctx->charset_name()->getText();
    std::string collation = ctx->collation() == nullptr
                                ? ""
                                : ctx->collation()->collation_name()->getText();
    bool is_binary = ctx->BINARY() != nullptr;
    // default utf-8 , one character contain four bytes.
    int store_unit = ParseContext::GetCharLengthSemanticsUnit(
        ParseContext::CharLengthSemantics::CHAR);

    RealDataType real_type = RealDataType::UNSUPPORTED;
    int default_length = length;
    if (ctx->CHARACTER() != nullptr) {
      real_type = RealDataType::CHAR;
    } else if (ctx->VARCHAR() != nullptr) {
      real_type = RealDataType::VARCHAR;
    } else {
      std::string type_string = ctx->text_type_i()->getText();
      if (Util::EqualsIgnoreCase(type_string, "TINYTEXT")) {
        real_type = RealDataType::TINYTEXT;
      } else if (Util::EqualsIgnoreCase(type_string, "TEXT")) {
        real_type = RealDataType::TEXT;
      } else if (Util::EqualsIgnoreCase(type_string, "MEDIUMTEXT")) {
        real_type = RealDataType::MEDIUMTEXT;
      } else if (Util::EqualsIgnoreCase(type_string, "LONGTEXT")) {
        real_type = RealDataType::LONGTEXT;
      }
      if (length == TypeInfo::ANONYMOUS_NUMBER) {
        default_length = GetDefaultLobLength(real_type);
      }
      return std::make_pair(real_type, std::make_shared<LOBTypeInfo>(
                                           length, default_length, charset,
                                           collation, is_binary, store_unit));
    }

    return std::make_pair(real_type, std::make_shared<CharacterTypeInfo>(
                                         length, default_length, charset,
                                         collation, is_binary, store_unit));

    // blob
  } else if (ctx->blob_type_i() != nullptr) {
    std::string type_string = ctx->blob_type_i()->getText();

    RealDataType type = RealDataType::BLOB;
    if (Util::EqualsIgnoreCase(type_string, "TINYBLOB")) {
      type = RealDataType::TINYBLOB;
    } else if (Util::EqualsIgnoreCase(type_string, "BLOB")) {
      type = RealDataType::BLOB;
    } else if (Util::EqualsIgnoreCase(type_string, "MEDIUMBLOB")) {
      type = RealDataType::MEDIUMBLOB;
    } else if (Util::EqualsIgnoreCase(type_string, "LONGBLOB")) {
      type = RealDataType::LONGBLOB;
    }

    int length =
        ctx->string_length_i() == nullptr
            ? TypeInfo::ANONYMOUS_NUMBER
            : std::stoi(ctx->string_length_i()->number_literal()->getText());

    return std::make_pair(
        type, std::make_shared<LOBTypeInfo>(true, length,
                                            GetDefaultLobLength(type), 1, ""));
  } else if (ctx->ENUM() != nullptr || ctx->SET() != nullptr) {
    // ENUM/SET
    std::string charset =
        ctx->charset_name() == nullptr ? "" : ctx->charset_name()->getText();
    std::string collation = ctx->collation() == nullptr
                                ? ""
                                : ctx->collation()->collation_name()->getText();
    bool is_binary = ctx->BINARY() != nullptr;
    RealDataType data_type =
        ctx->ENUM() != nullptr ? RealDataType::ENUM : RealDataType::SET;

    Strings element_list;
    for (const auto& str : ctx->string_list()->text_string()) {
      element_list.push_back(Util::GetValueString(str).value);
    }
    return std::make_pair(data_type,
                          std::make_shared<SetTypeInfo>(element_list, charset,
                                                        collation, is_binary));
  } else if (ctx->BINARY() != nullptr || ctx->VARBINARY() != nullptr) {
    // binary/var binary
    int length =
        ctx->string_length_i() == nullptr
            ? TypeInfo::ANONYMOUS_NUMBER
            : std::stoi(ctx->string_length_i()->number_literal()->getText());
    RealDataType type = ctx->BINARY() != nullptr ? RealDataType::BINRAY
                                                 : RealDataType::VARBINARY;

    return std::make_pair(type, std::make_shared<BinaryTypeInfo>(length));
  } else if (ctx->BIT() != nullptr) {
    // BIT
    int length = ctx->INTNUM().size() > 0 ? std::stoi(ctx->INTNUM(0)->getText())
                                          : TypeInfo::ANONYMOUS_NUMBER;
    return std::make_pair(RealDataType::BIT,
                          std::make_shared<BinaryTypeInfo>(length));
  } else if (nullptr != ctx->JSON()) {
    return std::make_pair(RealDataType::JSON,
                          std::make_shared<CharacterTypeInfo>(
                              CharacterTypeInfo::default_character_info));
  } else if (nullptr != ctx->GEOMETRY()) {
    return std::make_pair(
        RealDataType::GEOMETRY,
        std::make_shared<GenericTypeInfo>(RealDataType::GEOMETRY));
  } else if (nullptr != ctx->POINT()) {
    return std::make_pair(
        RealDataType::POINT,
        std::make_shared<GenericTypeInfo>(RealDataType::POINT));
  } else if (nullptr != ctx->LINESTRING()) {
    return std::make_pair(
        RealDataType::LINESTRING,
        std::make_shared<GenericTypeInfo>(RealDataType::LINESTRING));
  } else if (nullptr != ctx->POLYGON()) {
    return std::make_pair(
        RealDataType::POLYGON,
        std::make_shared<GenericTypeInfo>(RealDataType::POLYGON));
  } else if (nullptr != ctx->MULTIPOINT()) {
    return std::make_pair(
        RealDataType::MULTIPOINT,
        std::make_shared<GenericTypeInfo>(RealDataType::MULTIPOINT));
  } else if (nullptr != ctx->MULTILINESTRING()) {
    return std::make_pair(
        RealDataType::MULTILINESTRING,
        std::make_shared<GenericTypeInfo>(RealDataType::MULTILINESTRING));
  } else if (nullptr != ctx->MULTIPOLYGON()) {
    return std::make_pair(
        RealDataType::MULTIPOLYGON,
        std::make_shared<GenericTypeInfo>(RealDataType::MULTIPOLYGON));
  } else if (nullptr != ctx->GEOMETRYCOLLECTION()) {
    return std::make_pair(
        RealDataType::GEOMETRYCOLLECTION,
        std::make_shared<GenericTypeInfo>(RealDataType::GEOMETRYCOLLECTION));
  }
  return std::make_pair(RealDataType::UNSUPPORTED, nullptr);
}

int TypeHelper::GetDefaultLobLength(RealDataType real_data_type) {
  switch (real_data_type) {
    case TINYBLOB:
    case TINYTEXT:
      return 256;

    case BLOB:
    case TEXT:
      return 65536;  // 64K

    case MEDIUMBLOB:
    case MEDIUMTEXT:
      return 16777216;  // 16M

    case LONGBLOB:
    case LONGTEXT:
      return 50331648;  // 48M

    default:
      return -1;
  }
}
}  // namespace source

}  // namespace etransfer