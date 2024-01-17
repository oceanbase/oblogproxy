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
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/catalog.h"
#include "common/define.h"
#include "common/function_type.h"
#include "common/operator_type.h"
#include "common/raw_constant.h"
namespace etransfer {
namespace object {
using namespace common;
class ExprToken {
 public:
  RawConstant token;
  bool is_wrapped_by_brackets = false;
  ExprTokenType token_type;

  ExprToken(const RawConstant& token, ExprTokenType token_type)
      : token(token), token_type(token_type) {}
  ExprToken(const RawConstant& token, ExprTokenType token_type,
            bool is_wrapped_by_brackets)
      : token(token),
        token_type(token_type),
        is_wrapped_by_brackets(is_wrapped_by_brackets) {}

  ExprTokenType GetTokenType() { return token_type; }

  bool GetIsWrappedByBrackets() { return is_wrapped_by_brackets; }

  virtual ~ExprToken() {}
};
class ExprLiteral : public ExprToken {
 public:
  enum LiteralType {
    TEXT,
    // '2020-1-1' , '19:00:00' , '2020-1-1 19:00:00'
    TIME_TEXT,
    // b'100'
    BINARY,
    // mysql : X'A', db2 : BX'01fff'
    HEXADECIMAL,
    NUMBER,
    // DATE '2020-1-1'
    DATE,
    // TIME '20:00:00'
    TIME,
    // TIMESTAMP '2020-1-1 20:00:00'
    TIMESTAMP,
    INTERVAL,
    BOOL,
    NULL_TYPE,
    // DB2 default ,
    EMPTY_VALUE,
    UNKNOWN,
    // such as : day, IN, FROM, interval etc.
    KEYWORD
  };
  LiteralType literal_type;

  ExprLiteral(const RawConstant& literal, LiteralType literal_type)
      : ExprToken(literal, ExprTokenType::NORMAL_CHARACTER),
        literal_type(literal_type) {}

  ExprLiteral(const RawConstant& literal)
      : ExprLiteral(literal, LiteralType::TEXT) {}

  RawConstant GetLiteralValue() { return token; }

  ExprLiteral::LiteralType GetType() { return literal_type; }
  virtual ~ExprLiteral() = default;
};

class TimestampLiteral : public ExprLiteral {
 private:
  bool is_contain_at_time_zone_;

 public:
  TimestampLiteral(const RawConstant& literal, LiteralType literal_type,
                   bool is_contain_at_time_zone)
      : ExprLiteral(literal, literal_type),
        is_contain_at_time_zone_(is_contain_at_time_zone) {}

  TimestampLiteral(const RawConstant& literal)
      : TimestampLiteral(literal, LiteralType::TIMESTAMP, false) {}

  TimestampLiteral(const RawConstant& literal, bool is_contain_at_time_zone)
      : TimestampLiteral(literal, LiteralType::TIMESTAMP,
                         is_contain_at_time_zone) {}

  bool GetIsContainAtTimeZone() { return is_contain_at_time_zone_; }
};

class HexLiteral : public ExprLiteral {
 public:
  // BX'00FF' -> prefix : BX , hexValue : 00FF
  std::string prefix;
  std::string hex_value;

  HexLiteral(const RawConstant& literal, const std::string& prefix,
             const std::string& hex_value)
      : ExprLiteral(literal, LiteralType::HEXADECIMAL),
        prefix(prefix),
        hex_value(hex_value) {}
};

class ExprFunction : public ExprToken {
 private:
  FunctionType function_type_;
  std::vector<std::shared_ptr<ExprToken>> parameters_;

 public:
  ExprFunction(const RawConstant& token, FunctionType function_type)
      : ExprToken(token, ExprTokenType::FUNCTION),
        function_type_(function_type) {}

  ExprFunction(const RawConstant& token, FunctionType function_type,
               const std::vector<std::shared_ptr<ExprToken>>& parameters)
      : ExprToken(token, ExprTokenType::FUNCTION),
        function_type_(function_type),
        parameters_(parameters) {}

  FunctionType GetFunctionType() { return function_type_; }

  const std::vector<std::shared_ptr<ExprToken>>& GetParameters() const {
    return parameters_;
  }
};

class ExprColumnRef : public ExprToken {
 private:
  Catalog catalog_;
  RawConstant table_name_;

 public:
  ExprColumnRef(const RawConstant& column_name, const Catalog& catalog,
                const RawConstant& table_name)
      : ExprToken(column_name, ExprTokenType::IDENTIFIER_NAME),
        catalog_(catalog),
        table_name_(table_name) {}

  ExprColumnRef(const RawConstant& column_name)
      : ExprToken(column_name, ExprTokenType::IDENTIFIER_NAME) {}

  ExprColumnRef(const RawConstant& db_name, const RawConstant& table_name,
                const RawConstant& column_name)
      : ExprToken(column_name, ExprTokenType::IDENTIFIER_NAME),
        catalog_(db_name),
        table_name_(table_name) {}

  RawConstant GetColumnName() { return token; }

  RawConstant GetDbName() { return catalog_.GetRawCatalogName(); }

  Catalog GetCatalog() { return catalog_; }

  RawConstant GetTableName() { return table_name_; }
};

class SimpleExpr : public ExprToken {
 private:
  OperatorType op_;
  std::shared_ptr<ExprToken> left_exp_;
  std::shared_ptr<ExprToken> right_exp_;

 public:
  SimpleExpr(const RawConstant& token, OperatorType op,
             std::shared_ptr<ExprToken> left_exp,
             std::shared_ptr<ExprToken> right_exp)
      : SimpleExpr(token, op, left_exp, right_exp, false) {}

  SimpleExpr(const RawConstant& token, OperatorType op,
             std::shared_ptr<ExprToken> left_exp,
             std::shared_ptr<ExprToken> right_exp, bool is_wrapped_by_brackets)
      : ExprToken(token, ExprTokenType::BINARY_EXPRESSION,
                  is_wrapped_by_brackets),
        op_(op),
        left_exp_(left_exp),
        right_exp_(right_exp) {}

  OperatorType GetOperatorType() { return op_; }

  std::shared_ptr<ExprToken> GetLeftExp() { return left_exp_; }

  std::shared_ptr<ExprToken> GetRightExp() { return right_exp_; }
};

class CaseExpr : public SimpleExpr {
 public:
  using WhenAndThenVec = std::vector<
      std::pair<std::shared_ptr<ExprToken>, std::shared_ptr<ExprToken>>>;

 private:
  std::shared_ptr<ExprToken> input_expr_;
  WhenAndThenVec when_and_then_expr_;
  std::shared_ptr<ExprToken> default_expr_;

 public:
  CaseExpr(const RawConstant& token, std::shared_ptr<ExprToken> input_expr,
           const WhenAndThenVec& when_and_then_expr,
           std::shared_ptr<ExprToken> default_expr)
      : SimpleExpr(token, OperatorType::CASE_WHEN, nullptr, nullptr),
        input_expr_(input_expr),
        when_and_then_expr_(when_and_then_expr),
        default_expr_(default_expr) {}

  std::shared_ptr<ExprToken> GetInputExpr() { return input_expr_; }

  const std::vector<
      std::pair<std::shared_ptr<ExprToken>, std::shared_ptr<ExprToken>>>&
  GetWhenAndThenExpr() const {
    return when_and_then_expr_;
  }

  std::shared_ptr<ExprToken> GetDefaultExpr() { return default_expr_; }
};

};  // namespace object
};  // namespace etransfer