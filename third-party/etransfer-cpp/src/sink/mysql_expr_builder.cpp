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

#include "sink/mysql_expr_builder.h"

#include "common/util.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
std::string MySQLExprBuilder::Build(
    std::shared_ptr<ExprToken> default_value, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  return BuildExpr(default_value, data_type, sql_builder_context, false);
}
std::string MySQLExprBuilder::BuildExpr(
    std::shared_ptr<ExprToken> param, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  return BuildExpr(param, data_type, sql_builder_context, true);
}
std::string MySQLExprBuilder::BuildExpr(
    std::shared_ptr<ExprToken> param, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context, bool is_param) {
  if (param == nullptr) {
    return "";
  }
  std::string parsed;
  switch (param->GetTokenType()) {
    case ExprTokenType::IDENTIFIER_NAME:
      parsed = BuildColumn(param, sql_builder_context);
      break;
    case ExprTokenType::FUNCTION:
      parsed = BuildFunction(param, data_type, sql_builder_context);
      break;
    case ExprTokenType::BINARY_EXPRESSION:
      parsed = BuildBinaryExpression(param, data_type, sql_builder_context);
      break;
    case ExprTokenType::NORMAL_CHARACTER:
      parsed =
          BuildExprLiteral(param, data_type, sql_builder_context, is_param);
      break;
    default:
      sql_builder_context->SetErrMsg("unsupported token type");
      return "";
  }
  if (param->GetIsWrappedByBrackets()) {
    parsed = "(" + parsed;
    parsed.append(")");
  }
  return parsed;
}

std::string MySQLExprBuilder::BuildColumn(
    std::shared_ptr<ExprToken> param,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  std::shared_ptr<ExprColumnRef> column =
      std::dynamic_pointer_cast<ExprColumnRef>(param);
  RawConstant table_name = column->GetTableName();
  Catalog catalog = column->GetCatalog();
  std::string raw_schema_name = catalog.GetCatalogName();
  std::string raw_catalog_name = catalog.GetCatalogName();

  std::string db_table_name = BuildDbAndTableName(column, sql_builder_context);
  std::string mapped_column;
  bool use_orig_name = sql_builder_context->use_origin_identifier;
  mapped_column = SqlBuilderUtil::EscapeNormalObjectName(
      Util::RawConstantValue(column->token, use_orig_name),
      sql_builder_context);
  if (db_table_name.empty()) {
    line.append(db_table_name).append(".");
  }
  line.append(mapped_column);
  return line;
}
std::string MySQLExprBuilder::BuildDbAndTableName(
    std::shared_ptr<ExprColumnRef> column,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string map_db_name;
  std::string map_table_name;
  Catalog catalog = column->GetCatalog();
  std::string raw_catalog_name = catalog.GetCatalogName();

  if ("" != column->GetDbName().origin_string) {
    map_db_name = SqlBuilderUtil::EscapeNormalObjectName(
        Util::RawConstantValue(column->GetDbName(),
                               sql_builder_context->use_origin_identifier),
        sql_builder_context);
  }

  if ("" != column->GetTableName().origin_string) {
    map_table_name = SqlBuilderUtil::EscapeNormalObjectName(
        Util::RawConstantValue(column->GetTableName(),
                               sql_builder_context->use_origin_identifier),
        sql_builder_context);
  }

  return map_db_name.empty() ? map_db_name + "." + map_table_name
                             : map_table_name;
}
std::string MySQLExprBuilder::BuildBinaryExpression(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<SimpleExpr> simple_expr =
      std::dynamic_pointer_cast<SimpleExpr>(token);
  OperatorType operator_type = simple_expr->GetOperatorType();
  std::string res;
  switch (operator_type) {
    case OperatorType::PERIOD_OPERATOR:
      res = PeriodBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::DOT_OPERATOR:
      res = DotBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::SPACE_OPERATOR:
      res = SpaceBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::PLUS_OPERATOR:
    case OperatorType::MINUS_OPERATOR:
    case OperatorType::MULT_OPERATOR:
    case OperatorType::DIV_OPERATOR:
    case OperatorType::DIV_SYMBOL:
    case OperatorType::MOD_OPERATOR:
    case OperatorType::MOD_SYMBOL:
    case OperatorType::EQUAL_OPERATOR:
    case OperatorType::NOT_EQUAL_OPERATOR:
    case OperatorType::GREATER_THAN_OPERATOR:
    case OperatorType::GREATER_OR_EQUAL_OPERATOR:
    case OperatorType::LESS_THAN_OPERATOR:
    case OperatorType::LESS_OR_EQUAL_OPERATOR:
    case OperatorType::AND_SYMBOL:
    case OperatorType::OR_SYMBOL:
    case OperatorType::IN_SYMBOL:
    case OperatorType::NOT_IN_SYMBOL:
    case OperatorType::NULL_SAFE_EQUAL_OPERATOR:
    case OperatorType::LIKE_SYMBOL:
    case OperatorType::NOT_LIKE_SYMBOL:
    case OperatorType::IS_SYMBOL:
    case OperatorType::IS_NOT_SYMBOL:
      res = BaseOperatorBuild(token, operator_type, data_type,
                              sql_builder_context);
      break;
    case OperatorType::BETWEEN_AND_SYMBOL:
      res = BetweenAndSymbolBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::NOT_BETWEEN_AND_SYMBOL:
      res = BetweenAndSymbolBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::NOT_SYMBOL:
      res = NotSymbolBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::JSON_UNQUOTED_SEPARATOR_SYMBOL:
      res = JsonExtractUnQuotedBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::JSON_SEPARATOR_SYMBOL:
      res = JsonSeparatorBuild(token, data_type, sql_builder_context);
      break;
    case OperatorType::CASE_WHEN:
      res = CaseWhenBuild(token, data_type, sql_builder_context);
      break;
    default:
      break;
  }
  return res;
}

std::string MySQLExprBuilder::CaseWhenBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> context) {
  std::string line;
  std::shared_ptr<CaseExpr> case_expr =
      std::dynamic_pointer_cast<CaseExpr>(token);
  line.append("CASE ");
  if (nullptr != case_expr->GetInputExpr()) {
    line.append(BuildExpr(case_expr->GetInputExpr(), RealDataType::UNSUPPORTED,
                          context));
  }
  for (std::pair<std::shared_ptr<ExprToken>, std::shared_ptr<ExprToken>>
           when_and_then : case_expr->GetWhenAndThenExpr()) {
    line.append(" WHEN ")
        .append(
            BuildExpr(when_and_then.first, RealDataType::UNSUPPORTED, context))
        .append(" THEN ")
        .append(BuildExpr(when_and_then.second, RealDataType::UNSUPPORTED,
                          context));
  }
  if (nullptr != case_expr->GetDefaultExpr()) {
    line.append(" ELSE ").append(BuildExpr(case_expr->GetDefaultExpr(),
                                           RealDataType::UNSUPPORTED, context));
  }
  line.append(" END");
  return line;
}

std::string MySQLExprBuilder::BetweenAndSymbolBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<SimpleExpr> expr =
      std::dynamic_pointer_cast<SimpleExpr>(token);
  std::string left =
      BuildExpr(expr->GetLeftExp(), data_type, sql_builder_context);
  std::string right =
      BuildExpr(expr->GetRightExp(), data_type, sql_builder_context);
  auto splits = Util::Split(right, ",");
  if (splits.size() != 2) {
    sql_builder_context->SetErrMsg("Incorrect parameter count");
    return "";
  }
  auto param1 = splits[0];
  auto param2 = splits[1];
  Util::Trim(param1);
  Util::Trim(param2);
  Util::ReplaceFirst(param1, "(", "");
  Util::ReplaceFirst(param2, ")", "");
  std::string operator_name =
      expr->GetOperatorType() == OperatorType::BETWEEN_AND_SYMBOL
          ? " BETWEEN "
          : " NOT BETWEEN ";
  return left + operator_name + param1 + " AND " + param2;
}
std::string MySQLExprBuilder::BaseOperatorBuild(
    std::shared_ptr<ExprToken> token, OperatorType type, RealDataType data_type,
    std::shared_ptr<BuildContext> context) {
  if (ExprTokenType::BINARY_EXPRESSION == token->GetTokenType()) {
    std::shared_ptr<SimpleExpr> simple_expr =
        std::dynamic_pointer_cast<SimpleExpr>(token);
    if (nullptr == simple_expr->GetLeftExp()) {
      return OperatorTypeUtil::GetTypeName(type) +
             BuildExpr(simple_expr->GetRightExp(), data_type, context);
    }
    std::string res;
    res.append(BuildExpr(simple_expr->GetLeftExp(), data_type, context));
    res.append(" ");
    res.append(OperatorTypeUtil::GetTypeName(type));
    res.append(" ");
    res.append(BuildExpr(simple_expr->GetRightExp(), data_type, context));
    return res;
  } else {
    return BuildExpr(token, data_type, context);
  }
}

std::string MySQLExprBuilder::BaseSymbolBuild(
    std::shared_ptr<ExprToken> token, OperatorType type, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<SimpleExpr> simple_expr =
      std::dynamic_pointer_cast<SimpleExpr>(token);
  std::string left = simple_expr->GetLeftExp() == nullptr
                         ? ""
                         : BuildExpr(simple_expr->GetLeftExp(), data_type,
                                     sql_builder_context) +
                               " ";
  std::string res;
  res.append(left);
  res.append(OperatorTypeUtil::GetTypeName(type));
  res.append(" ");
  res.append(
      BuildExpr(simple_expr->GetRightExp(), data_type, sql_builder_context));
  return res;
}

std::string MySQLExprBuilder::BuildFunction(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  FunctionType function_type =
      std::dynamic_pointer_cast<ExprFunction>(token)->GetFunctionType();
  std::shared_ptr<ExprFunction> expr_func =
      std::dynamic_pointer_cast<ExprFunction>(token);

  return BuildFunction(function_type, expr_func, data_type,
                       sql_builder_context);
}
std::string MySQLExprBuilder::BuildFunction(
    FunctionType function_type, std::shared_ptr<ExprFunction> token,
    RealDataType data_type, std::shared_ptr<BuildContext> sql_builder_context) {
  std::string res;
  switch (function_type) {
    case FunctionType::CONCAT_PARAMS_WITH_SPACE:
      res = ConcatParamsWithSpace(token, data_type, sql_builder_context);
      break;
    case FunctionType::CONCAT_PARAMS_WITH_DOT:
      res = ConcatParamsWithDot(token, data_type, sql_builder_context);
      break;
    case FunctionType::NOW:
      res = NowBuild(token, data_type, sql_builder_context);
      break;
    case FunctionType::CURRENT_TIMESTAMP:
      res = CurrentTimestampBuild(token, data_type, sql_builder_context);
      break;
    case FunctionType::LOCALTIME:
      res = LocaltimeBuild(token, data_type, sql_builder_context);
      break;
    case FunctionType::LOCALTIMESTAMP:
      res = LocalTimestampBuild(token, data_type, sql_builder_context);
      break;
    case FunctionType::ADDDATE:
    case FunctionType::ADDTIME:
    case FunctionType::DATE:
    case FunctionType::SUBDATE:
    case FunctionType::SUBTIME:
    case FunctionType::TIMESTAMP:
    case FunctionType::MAKEDATE:
    case FunctionType::MAKETIME:
    case FunctionType::SEC_TO_TIME:
    case FunctionType::TIME:
    case FunctionType::TIMEDIFF:
    case FunctionType::YEAR:
    case FunctionType::ABS:
    case FunctionType::CHAR_LENGTH:
    case FunctionType::FIELD:
    case FunctionType::FIND_IN_SET:
    case FunctionType::LOCATE:
    case FunctionType::STRCMP:
    case FunctionType::FORMAT:
    case FunctionType::POSITION:
    case FunctionType::DAY:
    case FunctionType::DAYOFMONTH:
    case FunctionType::DAYOFWEEK:
    case FunctionType::DAYOFYEAR:
    case FunctionType::HOUR:
    case FunctionType::MICROSECOND:
    case FunctionType::MINUTE:
    case FunctionType::MONTH:
    case FunctionType::PERIOD_DIFF:
    case FunctionType::QUARTER:
    case FunctionType::SECOND:
    case FunctionType::TIME_TO_SEC:
    case FunctionType::TO_DAYS:
    case FunctionType::WEEK:
    case FunctionType::WEEKDAY:
    case FunctionType::CONCAT:
    case FunctionType::CONCAT_WS:
    case FunctionType::INSERT:
    case FunctionType::LCASE:
    case FunctionType::LEFT:
    case FunctionType::LOWER:
    case FunctionType::LPAD:
    case FunctionType::LTRIM:
    case FunctionType::RTRIM:
    case FunctionType::REPEAT:
    case FunctionType::REPLACE:
    case FunctionType::REVERSE:
    case FunctionType::RIGHT:
    case FunctionType::SPACE:
    case FunctionType::SUBSTR:
    case FunctionType::SUBSTRING_INDEX:
    case FunctionType::TRIM:
    case FunctionType::UCASE:
    case FunctionType::UPPER:
    case FunctionType::SQRT:
    case FunctionType::JSON_ARRAY:
    case FunctionType::JSON_OBJECT:
    case FunctionType::JSON_QUOTE:
    case FunctionType::JSON_UNQUOTE:
    case FunctionType::JSON_EXTRACT:
    case FunctionType::JSON_KEYS:
    case FunctionType::JSON_SEARCH:
    case FunctionType::JSON_APPEND:
    case FunctionType::JSON_ARRAY_APPEND:
    case FunctionType::JSON_REMOVE:
    case FunctionType::JSON_DEPTH:
    case FunctionType::JSON_LENGTH:
    case FunctionType::JSON_REPLACE:
    case FunctionType::JSON_SET:
    case FunctionType::MOD:
      res = FuncWithParamBuild(FunctionTypeUtil::GetFunctionName(function_type),
                               token, data_type, sql_builder_context);
      break;
    case FunctionType::DATEDIFF:
      res = DateDiffBuild(token, data_type, sql_builder_context);
      break;

    case FunctionType::JSON_MERGE:
      res = JsonMergeBuild(token, data_type, sql_builder_context);
      break;

    case FunctionType::JSON_MERGE_PATCH:
      res = JsonMergePatchBuild(token, data_type, sql_builder_context);
      break;
    case FunctionType::JSON_MERGE_PRESERVE:
      res = JsonMergePreserveBuild(token, data_type, sql_builder_context);
      break;
    case FunctionType::UNKNOWN:
      res = UnsupportedBuild(token, data_type, sql_builder_context);
      break;
    default:
      res = UnsupportedBuild(token, data_type, sql_builder_context);
      break;
  }
  return res;
}
std::string MySQLExprBuilder::BuildExprLiteral(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context, bool is_param) {
  std::shared_ptr<ExprLiteral> literal =
      std::dynamic_pointer_cast<ExprLiteral>(token);
  ExprLiteral::LiteralType literal_type = literal->GetType();
  std::string value;
  switch (literal_type) {
    case ExprLiteral::LiteralType::TEXT:
      value = literal->GetLiteralValue().origin_string;
      break;
    case ExprLiteral::LiteralType::TIME_TEXT:
      value = literal->GetLiteralValue().origin_string;
      break;
    case ExprLiteral::LiteralType::DATE:
      value =
          ConvertDateString(token, data_type, sql_builder_context, is_param);
      break;
    case ExprLiteral::LiteralType::TIME:
      value =
          ConvertTimeString(token, data_type, sql_builder_context, is_param);
      break;
    case ExprLiteral::LiteralType::TIMESTAMP:
      value = ConvertTimestampString(token, data_type, sql_builder_context,
                                     is_param);
      break;
    case ExprLiteral::LiteralType::EMPTY_VALUE:
      value = "";
      break;
    case ExprLiteral::LiteralType::BINARY:
      value = literal->GetLiteralValue().origin_string;
      break;
    case ExprLiteral::LiteralType::NUMBER:
      value = literal->GetLiteralValue().origin_string;
      break;
    case ExprLiteral::LiteralType::INTERVAL:
    case ExprLiteral::LiteralType::KEYWORD:
      value = literal->GetLiteralValue().origin_string;
      break;
    case ExprLiteral::LiteralType::NULL_TYPE:
      value = "NULL";
      break;
    case ExprLiteral::LiteralType::BOOL:
      value = Util::EqualsIgnoreCase("TRUE", literal->GetLiteralValue().value)
                  ? "TRUE"
                  : "FALSE";
      break;
    case ExprLiteral::LiteralType::HEXADECIMAL:
      value = ConvertHexValue(token, data_type, sql_builder_context, is_param);
      break;
    default:
      sql_builder_context->SetErrMsg("invalid default value: " +
                                     literal->GetLiteralValue().value);
  }
  return value;
}

std::string MySQLExprBuilder::ConvertHexValue(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context, bool is_param) {
  std::string res;
  std::shared_ptr<HexLiteral> hex =
      std::dynamic_pointer_cast<HexLiteral>(token);
  res.append("0x");
  res.append(hex->hex_value);
  return res;
}

std::string MySQLExprBuilder::ConvertDateString(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context, bool is_param) {
  std::shared_ptr<ExprLiteral> expr_literal =
      std::dynamic_pointer_cast<ExprLiteral>(token);
  auto upper_str =
      Util::UpperCase(expr_literal->GetLiteralValue().origin_string);
  Util::ReplaceFirst(upper_str, "DATE", "");
  Util::Trim(upper_str);
  return upper_str;
}

std::string MySQLExprBuilder::ConvertTimeString(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context, bool is_param) {
  std::shared_ptr<ExprLiteral> expr_literal =
      std::dynamic_pointer_cast<ExprLiteral>(token);
  auto upper_str =
      Util::UpperCase(expr_literal->GetLiteralValue().origin_string);
  Util::ReplaceFirst(upper_str, "TIME", "");
  Util::Trim(upper_str);
  return upper_str;
}

std::string MySQLExprBuilder::ConvertTimestampString(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context, bool is_param) {
  std::shared_ptr<ExprLiteral> expr_literal =
      std::dynamic_pointer_cast<ExprLiteral>(token);
  auto upper_str =
      Util::UpperCase(expr_literal->GetLiteralValue().origin_string);
  Util::ReplaceFirst(upper_str, "TIMESTAMP", "");
  Util::Trim(upper_str);
  return upper_str;
}

std::string MySQLExprBuilder::FuncWithParamBuild(
    const std::string& function_name, std::shared_ptr<ExprToken> token,
    RealDataType data_type, std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<ExprFunction> expr_func =
      std::dynamic_pointer_cast<ExprFunction>(token);
  Strings param_strs;
  const auto& params = expr_func->GetParameters();
  for (const auto& param : params) {
    param_strs.push_back(BuildExpr(param, data_type, sql_builder_context));
  }
  std::string line(function_name);
  line.append("(");
  line.append(Util::StringJoin(param_strs, ","));
  line.append(")");
  return line;
}
std::string MySQLExprBuilder::NowBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<ExprFunction> expr_function =
      std::dynamic_pointer_cast<ExprFunction>(token);
  if (expr_function->GetParameters().empty()) {
    return "NOW()";
  }
  return FuncWithParamBuild("NOW", token, data_type, sql_builder_context);
}

std::string MySQLExprBuilder::CurrentTimestampBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<ExprFunction> expr_function =
      std::dynamic_pointer_cast<ExprFunction>(token);
  if (expr_function->GetParameters().empty()) {
    return "CURRENT_TIMESTAMP";
  }
  return FuncWithParamBuild("CURRENT_TIMESTAMP", token, data_type,
                            sql_builder_context);
}

std::string MySQLExprBuilder::LocaltimeBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<ExprFunction> expr_function =
      std::dynamic_pointer_cast<ExprFunction>(token);
  if (expr_function->GetParameters().empty()) {
    return "LOCALTIME()";
  }
  return FuncWithParamBuild("LOCALTIME", token, data_type, sql_builder_context);
}

std::string MySQLExprBuilder::LocalTimestampBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<ExprFunction> expr_function =
      std::dynamic_pointer_cast<ExprFunction>(token);
  if (expr_function->GetParameters().empty()) {
    return "LOCALTIMESTAMP";
  }
  return FuncWithParamBuild("LOCALTIMESTAMP", token, data_type,
                            sql_builder_context);
}

std::string MySQLExprBuilder::ConcatParamsWithDelimiter(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> context, const std::string& delimiter) {
  auto params = GetAllParams(token, data_type, context);
  std::string line;
  line.append(Util::StringJoin(params, delimiter));
  return line;
}

Strings MySQLExprBuilder::GetAllParams(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<ExprFunction> expr_func =
      std::dynamic_pointer_cast<ExprFunction>(token);
  Strings param_strs;
  const auto& params = expr_func->GetParameters();
  for (const auto& param : params) {
    param_strs.push_back(BuildExpr(param, data_type, sql_builder_context));
  }
  return param_strs;
}

std::string MySQLExprBuilder::DateDiffBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  auto params = GetAllParams(token, data_type, sql_builder_context);
  if (params.size() != 2) {
    sql_builder_context->SetErrMsg("Incorrect parameter count");
    return "";
  }
  std::string line;
  line.append("TO_DAYS(")
      .append(params.at(0))
      .append(") - TO_DAYS(")
      .append(params.at(1))
      .append(")");
  return line;
}

std::string MySQLExprBuilder::JsonMergeBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  // Deprecated in 5.7.22
  if (sql_builder_context->target_db_version >= MYSQL_5722_VERSION) {
    sql_builder_context->SetErrMsg("unsupported function " +
                                   token->token.origin_string);
    return "";
  }
  return FuncWithParamBuild("JSON_MERGE", token, data_type,
                            sql_builder_context);
}

std::string MySQLExprBuilder::JsonMergePatchBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  // Introduced in mysql 5.7.22
  if (sql_builder_context->target_db_version < MYSQL_5722_VERSION) {
    sql_builder_context->SetErrMsg("unsupported function " +
                                   token->token.origin_string);
    return "";
  }
  return FuncWithParamBuild("JSON_MERGE_PATCH", token, data_type,
                            sql_builder_context);
}

std::string MySQLExprBuilder::JsonMergePreserveBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  // Introduced in mysql 5.7.22
  if (sql_builder_context->target_db_version < MYSQL_5722_VERSION) {
    sql_builder_context->SetErrMsg("unsupported function " +
                                   token->token.origin_string);
    return "";
  }
  return FuncWithParamBuild("JSON_MERGE_PRESERVE", token, data_type,
                            sql_builder_context);
}

std::string MySQLExprBuilder::PeriodBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> context) {
  std::shared_ptr<SimpleExpr> expr =
      std::dynamic_pointer_cast<SimpleExpr>(token);
  return BuildExpr(expr->GetLeftExp(), data_type, context) + "." +
         BuildExpr(expr->GetRightExp(), data_type, context);
}

std::string MySQLExprBuilder::DotBuild(std::shared_ptr<ExprToken> token,
                                       RealDataType data_type,
                                       std::shared_ptr<BuildContext> context) {
  std::shared_ptr<SimpleExpr> expr =
      std::dynamic_pointer_cast<SimpleExpr>(token);
  std::string res =
      BuildExpr(expr->GetLeftExp(), data_type, context) + " , " +
      BaseOperatorBuild(expr->GetRightExp(), OperatorType::DOT_OPERATOR,
                        data_type, context);
  return "(" + res + ")";
}

std::string MySQLExprBuilder::JsonExtractUnQuotedBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  // Introduced in mysql 5.7.13
  if (sql_builder_context->target_db_version < MYSQL_5713_VERSION) {
    sql_builder_context->SetErrMsg("unsupported function " +
                                   token->token.origin_string);
    return "";
  }
  return BaseOperatorBuild(token, OperatorType::JSON_UNQUOTED_SEPARATOR_SYMBOL,
                           data_type, sql_builder_context);
}

std::string MySQLExprBuilder::JsonSeparatorBuild(
    std::shared_ptr<ExprToken> token, RealDataType data_type,
    std::shared_ptr<BuildContext> sql_builder_context) {
  // Introduced in mysql 5.7.8
  if (sql_builder_context->target_db_version < MYSQL_5708_VERSION) {
    sql_builder_context->SetErrMsg("unsupported function " +
                                   token->token.origin_string);
    return "";
  }
  return BaseOperatorBuild(token, OperatorType::JSON_SEPARATOR_SYMBOL,
                           data_type, sql_builder_context);
}

}  // namespace sink

}  // namespace etransfer
