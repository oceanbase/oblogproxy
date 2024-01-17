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

#include "source/obmysql_expr_parser.h"

#include "common/function_type.h"
#include "common/util.h"
#include "source/obmysql_object_parser.h"
namespace etransfer {
namespace source {
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisNowOrSignedLiteral(
    OBParser::Now_or_signed_literalContext* context) {
  if (context->signed_literal() != nullptr) {
    return AnalysisSignedLiteral(context->signed_literal());
  } else if (context->cur_timestamp_func() != nullptr) {
    return AnalysisCurTimestampFunc(context->cur_timestamp_func());
  }
  return nullptr;
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisSignedLiteral(
    OBParser::Signed_literalContext* context) {
  if (context->Plus() != nullptr || context->Minus() != nullptr) {
    std::string symbol = context->Plus() != nullptr ? "+" : "-";
    return std::make_shared<ExprLiteral>(
        RawConstant(symbol + context->number_literal()->getText()),
        ExprLiteral::LiteralType::NUMBER);
  }
  return AnalysisLiteral(context->literal());
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisLiteral(
    OBParser::LiteralContext* context) {
  if (nullptr != context->complex_string_literal()) {
    return AnalysisComplexStringLiteral(context->complex_string_literal());
  } else if (context->DATE_VALUE() != nullptr) {
    std::string value = context->DATE_VALUE()->getText();
    if (Util::ContainsIgnoreCase(value, "TIMESTAMP")) {
      return std::make_shared<TimestampLiteral>(RawConstant(value));
    } else {
      ExprLiteral::LiteralType type = Util::ContainsIgnoreCase(value, "DATE")
                                          ? ExprLiteral::LiteralType::DATE
                                          : ExprLiteral::LiteralType::TIME;
      return std::make_shared<ExprLiteral>(RawConstant(value), type);
    }
  } else if (context->INTNUM() != nullptr ||
             context->DECIMAL_VAL() != nullptr) {
    return std::make_shared<ExprLiteral>(
        RawConstant(Util::GetCtxString(context)),
        ExprLiteral::LiteralType::NUMBER);
  } else if (context->BOOL_VALUE() != nullptr) {
    return std::make_shared<ExprLiteral>(
        RawConstant(Util::GetCtxString(context)),
        ExprLiteral::LiteralType::BOOL);
  } else if (context->NULLX() != nullptr) {
    return std::make_shared<ExprLiteral>(
        RawConstant(Util::GetCtxString(context)),
        ExprLiteral::LiteralType::NULL_TYPE);
  } else if (context->HEX_STRING_VALUE() != nullptr) {
    std::string value = context->HEX_STRING_VALUE()->getText();
    if (Util::StartsWithIgnoreCase(value, "X")) {
      std::string hex = value.substr(2, value.length() - 3);
      return std::make_shared<HexLiteral>(RawConstant(value), "X", hex);
    } else if (Util::StartsWithIgnoreCase(value, "0X")) {
      std::string hex = value.substr(2, value.length() - 2);
      return std::make_shared<HexLiteral>(RawConstant(value), "0x", hex);
    } else {
      return std::make_shared<ExprLiteral>(
          RawConstant(Util::GetCtxString(context)),
          ExprLiteral::LiteralType::BINARY);
    }
  } else {
    parse_context_->SetErrMsg("ob mysql parser can't parse literal " +
                              Util::GetCtxString(context));
  }
  return nullptr;
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisComplexStringLiteral(
    OBParser::Complex_string_literalContext* context) {
  if (nullptr != context->charset_introducer()) {
    parse_context_->SetErrMsg("ob mysql parser can't parse literal " +
                              Util::GetCtxString(context));
    return nullptr;
  }
  return std::make_shared<ExprLiteral>(
      RawConstant(context->STRING_VALUE()->getText()),
      ExprLiteral::LiteralType::TEXT);
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisCurTimestampFunc(
    OBParser::Cur_timestamp_funcContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  FunctionType function_type;
  if (context->NOW() != nullptr) {
    function_type = FunctionType::NOW;
  } else {
    OBParser::Now_synonyms_funcContext* func_context =
        context->now_synonyms_func();
    if (func_context->CURRENT_TIMESTAMP() != nullptr) {
      function_type = FunctionType::CURRENT_TIMESTAMP;
    } else if (func_context->LOCALTIME() != nullptr) {
      function_type = FunctionType::LOCALTIME;
    } else {
      function_type = FunctionType::LOCALTIMESTAMP;
    }
  }
  if (nullptr != context->INTNUM()) {
    params.push_back(
        std::make_shared<ExprLiteral>(RawConstant(context->INTNUM()->getText()),
                                      ExprLiteral::LiteralType::NUMBER));
    return std::make_shared<ExprFunction>(
        RawConstant(Util::GetCtxString(context)), function_type, params);
  }
  return std::make_shared<ExprFunction>(
      RawConstant(Util::GetCtxString(context)), function_type);
}

std::shared_ptr<ExprToken> OBMySQLExprParser::ParseExpr(
    OBParser::ExprContext* context) {
  if (nullptr != context->NOT()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::NOT_SYMBOL,
        nullptr, ParseExpr(context->expr(0)));
  } else if (nullptr != context->bool_pri()) {
    if (nullptr != context->IS()) {
      OperatorType type = context->not_() != nullptr
                              ? OperatorType::IS_NOT_SYMBOL
                              : OperatorType::IS_SYMBOL;
      std::string right = context->BOOL_VALUE() != nullptr
                              ? context->BOOL_VALUE()->getText()
                              : context->UNKNOWN()->getText();
      return std::make_shared<SimpleExpr>(
          RawConstant(Util::GetCtxString(context)), type,
          AnalysisBoolPri(context->bool_pri()),
          std::make_shared<ExprLiteral>(RawConstant(right),
                                        ExprLiteral::LiteralType::KEYWORD));
    } else {
      return AnalysisBoolPri(context->bool_pri());
    }
  } else {
    OperatorType type = OperatorType::UNKNOWN;
    if (nullptr != context->AND()) {
      type = OperatorType::AND_SYMBOL;
    } else if (nullptr != context->AND_OP()) {
      type = OperatorType::LOGICAL_AND_OPERATOR;
    } else if (nullptr != context->CNNOP()) {
      type = OperatorType::LOGICAL_OR_OPERATOR;
    } else if (nullptr != context->OR()) {
      type = OperatorType::OR_SYMBOL;
    } else {
      type = OperatorType::XOR_SYMBOL;
    }
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), type,
        ParseExpr(context->expr(0)), ParseExpr(context->expr(1)));
  }
  return nullptr;
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisBoolPri(
    OBParser::Bool_priContext* context) {
  if (nullptr != context->IS()) {
    OperatorType type = context->not_() != nullptr ? OperatorType::IS_NOT_SYMBOL
                                                   : OperatorType::IS_SYMBOL;
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), type,
        AnalysisBoolPri(context->bool_pri()),
        std::make_shared<ExprLiteral>(RawConstant(context->NULLX()->getText()),
                                      ExprLiteral::LiteralType::KEYWORD));
  } else if (nullptr != context->bool_pri()) {
    OperatorType type;
    if (nullptr != context->COMP_EQ()) {
      type = OperatorType::EQUAL_OPERATOR;
    } else if (nullptr != context->COMP_GE()) {
      type = OperatorType::GREATER_OR_EQUAL_OPERATOR;
    } else if (nullptr != context->COMP_GT()) {
      type = OperatorType::GREATER_THAN_OPERATOR;
    } else if (nullptr != context->COMP_LE()) {
      type = OperatorType::LESS_OR_EQUAL_OPERATOR;
    } else if (nullptr != context->COMP_LT()) {
      type = OperatorType::LESS_THAN_OPERATOR;
    } else if (nullptr != context->COMP_NE()) {
      type = OperatorType::NOT_EQUAL_OPERATOR;
    } else {
      type = OperatorType::NULL_SAFE_EQUAL_OPERATOR;
    }
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), type,
        AnalysisBoolPri(context->bool_pri()),
        AnalysisPredicate(context->predicate()));
  } else if (nullptr != context->predicate()) {
    return AnalysisPredicate(context->predicate());
  }
  return nullptr;
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisPredicate(
    OBParser::PredicateContext* context) {
  if (nullptr != context->IN()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        context->not_() == nullptr ? OperatorType::IN_SYMBOL
                                   : OperatorType::NOT_IN_SYMBOL,
        AnalysisBitExpr(context->bit_expr(0)),
        AnalysisInExpr(context->in_expr()));
  } else if (nullptr != context->BETWEEN()) {
    auto right = std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::DOT_OPERATOR,
        AnalysisBitExpr(context->bit_expr(1)),
        AnalysisPredicate(context->predicate()));
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        context->not_() == nullptr ? OperatorType::BETWEEN_AND_SYMBOL
                                   : OperatorType::NOT_BETWEEN_AND_SYMBOL,
        AnalysisBitExpr(context->bit_expr(0)), right);
  } else if (nullptr != context->LIKE()) {
    std::vector<std::shared_ptr<ExprToken>> right_token;
    if (nullptr != context->simple_expr() && nullptr != context->bit_expr(0)) {
      right_token.push_back(AnalysisSimpleExpr(context->simple_expr()));
    } else {
      if (nullptr != context->like_right_param_option1()) {
        OBParser::Like_right_param_option1Context* ctxt1 =
            context->like_right_param_option1();
        if (nullptr != ctxt1->STRING_VALUE()) {
          right_token.push_back(std::make_shared<ExprLiteral>(
              RawConstant(ctxt1->STRING_VALUE()->getText())));
          auto strs = AnalysisStringValList(ctxt1->string_val_list());
          right_token.insert(right_token.end(), strs.begin(), strs.end());
        } else {
          right_token.push_back(AnalysisSimpleExpr(ctxt1->simple_expr(0)));
        }
        if (nullptr != ctxt1->ESCAPE()) {
          right_token.push_back(std::make_shared<ExprLiteral>(
              RawConstant(ctxt1->ESCAPE()->getText()),
              ExprLiteral::LiteralType::KEYWORD));
          right_token.push_back(AnalysisSimpleExpr(ctxt1->simple_expr(1)));
        }
      } else {
        OBParser::Like_right_param_option2Context* ctx2 =
            context->like_right_param_option2();
        if (nullptr != ctx2->simple_expr()) {
          right_token.push_back(AnalysisSimpleExpr(ctx2->simple_expr()));
        } else {
          right_token.push_back(std::make_shared<ExprLiteral>(
              RawConstant(ctx2->STRING_VALUE(0)->getText())));
          auto strs = AnalysisStringValList(ctx2->string_val_list(0));
          right_token.insert(right_token.end(), strs.begin(), strs.end());
        }
        right_token.push_back(std::make_shared<ExprLiteral>(
            RawConstant(ctx2->ESCAPE()->getText()),
            ExprLiteral::LiteralType::KEYWORD));
        right_token.push_back(std::make_shared<ExprLiteral>(
            RawConstant(ctx2->STRING_VALUE(1)->getText())));
        auto strs = AnalysisStringValList(ctx2->string_val_list(1));
        right_token.insert(right_token.end(), strs.begin(), strs.end());
      }
    }
    auto right = std::make_shared<ExprFunction>(
        RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, right_token);
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        context->not_() == nullptr ? OperatorType::LIKE_SYMBOL
                                   : OperatorType::NOT_LIKE_SYMBOL,
        AnalysisBitExpr(context->bit_expr(0)), right);
  } else if (nullptr != context->REGEXP()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        context->not_() == nullptr ? OperatorType::REGEXP_SYMBOL
                                   : OperatorType::NOT_REGEXP_SYMBOL,
        AnalysisBitExpr(context->bit_expr(0)),
        AnalysisBitExpr(context->bit_expr(1)));
  } else {
    return AnalysisBitExpr(context->bit_expr(0));
  }
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisBitExpr(
    OBParser::Bit_exprContext* context) {
  if (nullptr != context->INTERVAL()) {
    OperatorType type = context->Minus() != nullptr
                            ? OperatorType::MINUS_INTERVAL_OPERATOR
                            : OperatorType::PLUS_INTERVAL_OPERATOR;
    auto left = AnalysisBitExpr(context->bit_expr(0));
    auto right = std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::SPACE_OPERATOR,
        ParseExpr(context->expr()),
        std::make_shared<ExprLiteral>(
            RawConstant(context->date_unit()->getText()),
            ExprLiteral::LiteralType::KEYWORD));
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), type, left, right);
  } else if (nullptr != context->simple_expr()) {
    return AnalysisSimpleExpr(context->simple_expr());
  } else {
    OperatorType type;
    if (nullptr != context->And()) {
      type = OperatorType::AND_SYMBOL;
    } else if (nullptr != context->Caret()) {
      type = OperatorType::BITWISE_XOR_OPERATOR;
    } else if (nullptr != context->DIV()) {
      type = OperatorType::DIV_SYMBOL;
    } else if (nullptr != context->Div()) {
      type = OperatorType::DIV_OPERATOR;
    } else if (nullptr != context->MOD()) {
      type = OperatorType::MOD_SYMBOL;
    } else if (nullptr != context->Minus()) {
      type = OperatorType::MINUS_OPERATOR;
    } else if (nullptr != context->Mod()) {
      type = OperatorType::MOD_OPERATOR;
    } else if (nullptr != context->Or()) {
      type = OperatorType::BITWISE_OR_OPERATOR;
    } else if (nullptr != context->Plus()) {
      type = OperatorType::PLUS_OPERATOR;
    } else if (nullptr != context->SHIFT_LEFT()) {
      type = OperatorType::SHIFT_LEFT_OPERATOR;
    } else if (nullptr != context->SHIFT_RIGHT()) {
      type = OperatorType::SHIFT_RIGHT_OPERATOR;
    } else {
      type = OperatorType::MULT_OPERATOR;
    }
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), type,
        AnalysisBitExpr(context->bit_expr(0)),
        AnalysisBitExpr(context->bit_expr(1)));
  }
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisSimpleExpr(
    OBParser::Simple_exprContext* context) {
  if (nullptr != context->select_with_parens() || nullptr != context->ROW() ||
      nullptr != context->EXISTS() || nullptr != context->MATCH() ||
      nullptr != context->window_function() ||
      nullptr != context->USER_VARIABLE()) {
    return nullptr;
  }
  if (nullptr != context->case_expr()) {
    return AnalysisCaseExpr(context->case_expr());
  } else if (nullptr != context->collation()) {
    auto right = std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::SPACE_OPERATOR,
        std::make_shared<ExprLiteral>(
            RawConstant(context->collation()->COLLATE()->getText()),
            ExprLiteral::LiteralType::KEYWORD),
        std::make_shared<ExprLiteral>(
            RawConstant(context->collation()->collation_name()->getText())));
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::SPACE_OPERATOR,
        AnalysisSimpleExpr(context->simple_expr(0)), right);
  } else if (nullptr != context->BINARY()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::SPACE_OPERATOR,
        std::make_shared<ExprLiteral>(RawConstant(context->BINARY()->getText()),
                                      ExprLiteral::LiteralType::KEYWORD),
        AnalysisSimpleExpr(context->simple_expr(0)));
  } else if (nullptr != context->column_ref()) {
    return AnalysisColumnRef(context->column_ref());
  } else if (nullptr != context->expr_const()) {
    return AnalysisExprConst(context->expr_const());
  } else if (nullptr != context->CNNOP()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        OperatorType::LOGICAL_OR_OPERATOR,
        AnalysisSimpleExpr(context->simple_expr(0)),
        AnalysisSimpleExpr(context->simple_expr(1)));
  } else if (nullptr != context->Plus()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::PLUS_OPERATOR,
        nullptr, AnalysisSimpleExpr(context->simple_expr(0)));
  } else if (nullptr != context->Minus()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::MINUS_OPERATOR,
        nullptr, AnalysisSimpleExpr(context->simple_expr(0)));
  } else if (nullptr != context->Tilde()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::TILDE_OPERATOR,
        nullptr, AnalysisSimpleExpr(context->simple_expr(0)));
  } else if (nullptr != context->not2()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::NOT_SYMBOL,
        nullptr, AnalysisSimpleExpr(context->simple_expr(0)));
  } else if (nullptr != context->expr_list()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)), OperatorType::DOT_OPERATOR,
        AnalysisExprList(context->expr_list()), ParseExpr(context->expr()),
        true);
  } else if (nullptr != context->func_expr()) {
    return AnalysisFuncExpr(context->func_expr());
  } else if (nullptr != context->JSON_EXTRACT()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        OperatorType::JSON_SEPARATOR_SYMBOL,
        ParseColumnDefinitionRef(context->column_definition_ref()),
        AnalysisComplexStringLiteral(context->complex_string_literal()));
  } else if (nullptr != context->JSON_EXTRACT_UNQUOTED()) {
    return std::make_shared<SimpleExpr>(
        RawConstant(Util::GetCtxString(context)),
        OperatorType::JSON_UNQUOTED_SEPARATOR_SYMBOL,
        ParseColumnDefinitionRef(context->column_definition_ref()),
        AnalysisComplexStringLiteral(context->complex_string_literal()));
  } else {
    auto expr = ParseExpr(context->expr());
    expr->is_wrapped_by_brackets = true;
    return expr;
  }
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisInExpr(
    OBParser::In_exprContext* context) {
  if (nullptr != context->select_with_parens()) {
    return nullptr;
  }
  auto expr_token = AnalysisExprList(context->expr_list());
  expr_token->is_wrapped_by_brackets = true;
  return expr_token;
}
std::vector<std::shared_ptr<ExprToken>>
OBMySQLExprParser::AnalysisStringValList(
    OBParser::String_val_listContext* ctx) {
  std::vector<std::shared_ptr<ExprToken>> res;
  for (const auto& str : ctx->STRING_VALUE()) {
    res.push_back(std::make_shared<ExprLiteral>(RawConstant(str->getText())));
  }
  return res;
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisCaseExpr(
    OBParser::Case_exprContext* context) {
  std::shared_ptr<ExprToken> input = nullptr;
  if (nullptr != context->case_arg() &&
      nullptr != context->case_arg()->expr()) {
    input = ParseExpr(context->case_arg()->expr());
  }
  OBParser::When_clause_listContext* when_clause_list_context =
      context->when_clause_list();
  CaseExpr::WhenAndThenVec when_and_thens;
  for (const auto& when : when_clause_list_context->when_clause()) {
    when_and_thens.push_back(AnalysisWhenClause(when));
  }
  std::shared_ptr<ExprToken> default_value =
      AnalysisCaseDefault(context->case_default());
  return std::make_shared<CaseExpr>(RawConstant(Util::GetCtxString(context)),
                                    input, when_and_thens, default_value);
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisColumnRef(
    OBParser::Column_refContext* ctx) {
  antlr4::ParserRuleContext* table_name = nullptr;
  antlr4::ParserRuleContext* database_name = nullptr;
  std::string column_name;
  if (ctx->Dot().empty()) {
    if (nullptr != ctx->Star()) {
      table_name = ctx->relation_name().size() > 1 ? ctx->relation_name(1)
                                                   : ctx->relation_name(0);
      database_name =
          ctx->relation_name().size() > 1 ? ctx->relation_name(0) : nullptr;
      column_name = ctx->Star()->getText();
    } else if (ctx->mysql_reserved_keyword().size() > 1) {
      table_name = ctx->mysql_reserved_keyword(0);
      database_name =
          !ctx->relation_name().empty() ? ctx->relation_name(0) : nullptr;
      column_name = ctx->mysql_reserved_keyword(1)->getText();
    } else {
      table_name = ctx->relation_name().size() > 1 ? ctx->relation_name(1)
                                                   : ctx->relation_name(0);
      database_name =
          ctx->relation_name().size() > 1 ? ctx->relation_name(0) : nullptr;
      column_name = ctx->column_name() != nullptr
                        ? ctx->column_name()->getText()
                        : ctx->mysql_reserved_keyword(0)->getText();
    }
  } else {
    column_name = ctx->column_name()->getText();
  }

  return std::make_shared<ExprColumnRef>(
      database_name != nullptr ? OBMySQLObjectParser::ProcessTableOrDbName(
                                     database_name->getText(), parse_context_)
                               : RawConstant(),
      table_name != nullptr ? OBMySQLObjectParser::ProcessTableOrDbName(
                                  table_name->getText(), parse_context_)
                            : RawConstant(),
      OBMySQLObjectParser::ProcessColumnName(column_name));
}

std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisExprConst(
    OBParser::Expr_constContext* ctx) {
  if (nullptr != ctx->literal()) {
    return AnalysisLiteral(ctx->literal());
  } else if (nullptr != ctx->Dot()) {
    return nullptr;
  } else {
    std::string value = ctx->SYSTEM_VARIABLE() == nullptr
                            ? ctx->QUESTIONMARK()->getText()
                            : ctx->SYSTEM_VARIABLE()->getText();
    return std::make_shared<ExprLiteral>(RawConstant(value));
  }
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisExprList(
    OBParser::Expr_listContext* context) {
  return context->expr().size() == 1 ? ParseExpr(context->expr(0))
                                     : AnalysisExprs(context->expr());
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisFuncExpr(
    OBParser::Func_exprContext* context) {
  FunctionType type = FunctionType::UNKNOWN;
  std::vector<std::shared_ptr<ExprToken>> params;
  if (nullptr != context->MOD()) {
    type = FunctionType::MOD;
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  } else if (nullptr != context->CAST()) {
    type = FunctionType::CAST;
    params.push_back(std::make_shared<SimpleExpr>(
        RawConstant(), OperatorType::SPACE_OPERATOR,
        ParseExpr(context->expr(0)),
        std::make_shared<SimpleExpr>(
            RawConstant(), OperatorType::SPACE_OPERATOR,
            std::make_shared<ExprLiteral>(RawConstant("AS"),
                                          ExprLiteral::LiteralType::KEYWORD),
            AnalysisCastDataType(context->cast_data_type()))));
  } else if (nullptr != context->INSERT()) {
    type = FunctionType::INSERT;
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  } else if (nullptr != context->LEFT()) {
    type = FunctionType::LEFT;
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  } else if (nullptr != context->POSITION()) {
    type = FunctionType::POSITION;
    std::vector<std::shared_ptr<ExprToken>> right_token;
    right_token.push_back(AnalysisBitExpr(context->bit_expr(0)));
    right_token.push_back(
        std::make_shared<ExprLiteral>(RawConstant(context->IN()->getText()),
                                      ExprLiteral::LiteralType::KEYWORD));
    right_token.push_back(ParseExpr(context->expr(0)));
    params.push_back(std::make_shared<ExprFunction>(
        RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, right_token));
  } else if (nullptr != context->substr_or_substring()) {
    type = FunctionType::SUBSTR;
    const auto& sub_strs = AnalysisSubstrParams(context->substr_params());
    params.insert(params.end(), sub_strs.begin(), sub_strs.end());
  } else if (nullptr != context->TRIM()) {
    type = FunctionType::TRIM;
    params.push_back(AnalysisParameterizedTrim(context->parameterized_trim()));
  } else if (nullptr != context->DATE()) {
    type = FunctionType::DATE;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->YEAR()) {
    type = FunctionType::YEAR;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->TIME()) {
    type = FunctionType::TIME;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->WEEK()) {
    type = FunctionType::WEEK;
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  } else if (nullptr != context->SECOND()) {
    type = FunctionType::SECOND;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->MINUTE()) {
    type = FunctionType::MINUTE;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->MICROSECOND()) {
    type = FunctionType::MICROSECOND;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->HOUR()) {
    type = FunctionType::HOUR;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->DATE_ADD() || nullptr != context->DATE_SUB()) {
    type = context->DATE_ADD() != nullptr ? FunctionType::ADDDATE
                                          : FunctionType::SUBDATE;
    const auto& date_params = AnalysisDateParams(context->date_params());
    params.insert(params.end(), date_params.begin(), date_params.end());
  } else if (nullptr != context->ADDDATE() || nullptr != context->SUBDATE()) {
    type = context->ADDDATE() != nullptr ? FunctionType::ADDDATE
                                         : FunctionType::SUBDATE;
    if (nullptr != context->date_params()) {
      const auto& date_params = AnalysisDateParams(context->date_params());
      params.insert(params.end(), date_params.begin(), date_params.end());
    } else {
      for (const auto& e : context->expr()) {
        params.push_back(ParseExpr(e));
      }
    }
  } else if (nullptr != context->HOUR()) {
    type = FunctionType::HOUR;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->TIMESTAMPDIFF() ||
             nullptr != context->TIMESTAMPADD()) {
    type = nullptr != context->TIMESTAMPDIFF() ? FunctionType::TIMESTAMPDIFF
                                               : FunctionType::TIMESTAMPADD;
    const auto& timestamp_params =
        AnalysisTimestampParams(context->timestamp_params());
    params.insert(params.end(), params.begin(), params.end());
  } else if (nullptr != context->EXTRACT()) {
    type = FunctionType::EXTRACT;
    std::vector<std::shared_ptr<ExprToken>> inner_params;
    inner_params.push_back(std::make_shared<ExprLiteral>(
        RawConstant(context->date_unit()->getText()),
        ExprLiteral::LiteralType::KEYWORD));
    inner_params.push_back(
        std::make_shared<ExprLiteral>(RawConstant(context->FROM()->getText()),
                                      ExprLiteral::LiteralType::KEYWORD));
    inner_params.push_back(ParseExpr(context->expr(0)));
    params.push_back(std::make_shared<ExprFunction>(
        RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, inner_params));
  } else if (nullptr != context->HOUR()) {
    type = FunctionType::ASCII;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->HOUR()) {
    type = FunctionType::CHARACTER;
    if (nullptr != context->USING()) {
      std::vector<std::shared_ptr<ExprToken>> inner_params;
      inner_params.push_back(AnalysisExprList(context->expr_list()));
      inner_params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->USING()->getText()),
          ExprLiteral::LiteralType::KEYWORD));
      inner_params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->charset_name()->getText()),
          ExprLiteral::LiteralType::KEYWORD));
      params.push_back(std::make_shared<ExprFunction>(
          RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, inner_params));
    } else {
      params.push_back(AnalysisExprList(context->expr_list()));
    }
  } else if (nullptr != context->cur_timestamp_func()) {
    return AnalysisCurTimestampFunc(context->cur_timestamp_func());
  } else if (nullptr != context->sysdate_func()) {
    type = FunctionType::SYSDATE;
    if (nullptr != context->cur_time_func()->INTNUM()) {
      params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->sysdate_func()->INTNUM()->getText()),
          ExprLiteral::LiteralType::NUMBER));
    }
  } else if (nullptr != context->cur_time_func()) {
    type = FunctionType::CURRENT_TIME;
    if (nullptr != context->cur_time_func()->INTNUM()) {
      params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->cur_time_func()->INTNUM()->getText()),
          ExprLiteral::LiteralType::NUMBER));
    }
  } else if (nullptr != context->cur_date_func()) {
    type = FunctionType::CURRENT_DATE;
  } else if (nullptr != context->utc_timestamp_func()) {
    type = FunctionType::UTC_TIMESTAMP;
    if (nullptr != context->utc_timestamp_func()->INTNUM()) {
      params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->utc_timestamp_func()->INTNUM()->getText()),
          ExprLiteral::LiteralType::NUMBER));
    }
  } else if (nullptr != context->MONTH()) {
    type = FunctionType::MONTH;
    params.push_back(ParseExpr(context->expr(0)));
  } else if (nullptr != context->function_name()) {
    if (nullptr != context->relation_name()) {
      return nullptr;
    }
    type =
        FunctionTypeUtil::GetFunctionType(context->function_name()->getText());
    auto exprs = AnalysisExprAsList(context->expr_as_list());
    params.insert(params.end(), exprs.begin(), exprs.end());
  } else if (nullptr != context->sys_interval_func()) {
    return AnalysisSysIntervalFunc(context->sys_interval_func());
  } else if (nullptr != context->TIMESTAMP()) {
    type = FunctionType::TIMESTAMP;
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  } else if (nullptr != context->QUARTER()) {
    type = FunctionType::QUARTER;
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  } else if (nullptr != context->COUNT()) {
    type = FunctionType::COUNT;
    if (nullptr != context->Star()) {
      if (nullptr != context->ALL()) {
        params.push_back(std::make_shared<ExprLiteral>(
            RawConstant(context->ALL()->getText()),
            ExprLiteral::LiteralType::KEYWORD));
      }
      params.push_back(std::make_shared<ExprColumnRef>(
          RawConstant(context->Star()->getText())));
    } else if (nullptr != context->expr_list()) {
      if (nullptr != context->DISTINCT()) {
        params.push_back(std::make_shared<ExprLiteral>(
            RawConstant(context->DISTINCT()->getText()),
            ExprLiteral::LiteralType::KEYWORD));
      } else {
        params.push_back(std::make_shared<ExprLiteral>(
            RawConstant(context->UNIQUE()->getText()),
            ExprLiteral::LiteralType::KEYWORD));
      }
      for (const auto& e : context->expr_list()->expr()) {
        params.push_back(ParseExpr(e));
      }
    } else {
      if (nullptr != context->ALL()) {
        params.push_back(std::make_shared<ExprLiteral>(
            RawConstant(context->ALL()->getText()),
            ExprLiteral::LiteralType::KEYWORD));
      }
      params.push_back(ParseExpr(context->expr(0)));
    }
  } else if (nullptr != context->SUM() || nullptr != context->MAX() ||
             nullptr != context->MIN() || nullptr != context->AVG()) {
    if (nullptr != context->ALL()) {
      params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->ALL()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
    }
    if (nullptr != context->DISTINCT()) {
      params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->DISTINCT()->getText()),
          ExprLiteral::LiteralType::KEYWORD));
    }
    if (nullptr != context->UNIQUE()) {
      params.push_back(std::make_shared<ExprLiteral>(
          RawConstant(context->UNIQUE()->getText()),
          ExprLiteral::LiteralType::KEYWORD));
    }
    params.push_back(ParseExpr(context->expr(0)));
    if (nullptr != context->SUM()) {
      type = FunctionType::SUM;
    } else if (nullptr != context->MAX()) {
      type = FunctionType::MAX;
    } else if (nullptr != context->MIN()) {
      type = FunctionType::MIN;
    } else {
      type = FunctionType::AVG;
    }
  } else {
    return nullptr;
  }
  return std::make_shared<ExprFunction>(
      RawConstant(Util::GetCtxString(context)), type, params);
}
std::shared_ptr<ExprToken> OBMySQLExprParser::ParseColumnDefinitionRef(
    OBParser::Column_definition_refContext* ctx) {
  OBParser::Relation_nameContext* table_name = nullptr;
  OBParser::Relation_nameContext* db_name = nullptr;

  if (!ctx->relation_name().empty()) {
    table_name = ctx->relation_name().size() > 1 ? ctx->relation_name(1)
                                                 : ctx->relation_name(0);
    db_name = ctx->relation_name().size() > 1 ? ctx->relation_name(0) : nullptr;
  }
  return std::make_shared<ExprColumnRef>(
      db_name != nullptr ? OBMySQLObjectParser::ProcessTableOrDbName(
                               db_name->getText(), parse_context_)
                         : RawConstant(),
      table_name != nullptr ? OBMySQLObjectParser::ProcessTableOrDbName(
                                  table_name->getText(), parse_context_)
                            : RawConstant(),
      OBMySQLObjectParser::ProcessColumnName(ctx->column_name()->getText()));
}
std::pair<std::shared_ptr<ExprToken>, std::shared_ptr<ExprToken>>
OBMySQLExprParser::AnalysisWhenClause(
    OBParser::When_clauseContext* when_clause_context) {
  return std::make_pair(ParseExpr(when_clause_context->expr(0)),
                        ParseExpr(when_clause_context->expr(1)));
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisCastDataType(
    OBParser::Cast_data_typeContext* context) {
  return std::make_shared<ExprLiteral>(RawConstant(Util::GetCtxString(context)),
                                       ExprLiteral::LiteralType::KEYWORD);
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisCaseDefault(
    OBParser::Case_defaultContext* case_default_context) {
  if (nullptr != case_default_context->empty()) {
    return nullptr;
  }
  return ParseExpr(case_default_context->expr());
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisExprs(
    const std::vector<OBParser::ExprContext*>& context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  for (auto e : context) {
    params.push_back(ParseExpr(e));
  }
  return std::make_shared<ExprFunction>(
      RawConstant(), FunctionType::CONCAT_PARAMS_WITH_DOT, params);
}
std::vector<std::shared_ptr<ExprToken>> OBMySQLExprParser::AnalysisSubstrParams(
    OBParser::Substr_paramsContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  if (nullptr != context->FROM()) {
    std::vector<std::shared_ptr<ExprToken>> inner_params;
    inner_params.push_back(ParseExpr(context->expr(0)));
    inner_params.push_back(
        std::make_shared<ExprLiteral>(RawConstant(context->FROM()->getText()),
                                      ExprLiteral::LiteralType::KEYWORD));
    inner_params.push_back(ParseExpr(context->expr(1)));
    if (nullptr != context->FOR()) {
      inner_params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->FOR()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
      inner_params.push_back(ParseExpr(context->expr(2)));
    }
    params.push_back(std::make_shared<ExprFunction>(
        RawConstant(Util::GetCtxString(context)),
        FunctionType::CONCAT_PARAMS_WITH_SPACE, inner_params));
  } else {
    for (const auto& e : context->expr()) {
      params.push_back(ParseExpr(e));
    }
  }
  return params;
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisParameterizedTrim(
    OBParser::Parameterized_trimContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  if (nullptr != context->LEADING() || nullptr != context->TRAILING()) {
    params.push_back(std::make_shared<ExprLiteral>(
        RawConstant(context->LEADING() != nullptr
                        ? context->LEADING()->getText()
                        : context->TRAILING()->getText()),
        ExprLiteral::LiteralType::KEYWORD));
    if (context->expr().size() > 1) {
      params.push_back(ParseExpr(context->expr(0)));
      params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->FROM()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
      params.push_back(ParseExpr(context->expr(1)));
    } else {
      params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->FROM()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
      params.push_back(ParseExpr(context->expr(0)));
    }
  } else if (context->expr().size() == 2) {
    if (nullptr != context->BOTH()) {
      params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->BOTH()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
    }
    params.push_back(ParseExpr(context->expr(0)));
    params.push_back(
        std::make_shared<ExprLiteral>(RawConstant(context->FROM()->getText()),
                                      ExprLiteral::LiteralType::KEYWORD));
    params.push_back(ParseExpr(context->expr(1)));
  } else {
    if (nullptr != context->BOTH()) {
      params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->BOTH()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
      params.push_back(
          std::make_shared<ExprLiteral>(RawConstant(context->FROM()->getText()),
                                        ExprLiteral::LiteralType::KEYWORD));
    }
    params.push_back(ParseExpr(context->expr(0)));
  }
  return std::make_shared<ExprFunction>(
      RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, params);
}
std::vector<std::shared_ptr<ExprToken>> OBMySQLExprParser::AnalysisDateParams(
    OBParser::Date_paramsContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  std::vector<std::shared_ptr<ExprToken>> inner_params;
  params.push_back(ParseExpr(context->expr(0)));
  inner_params.push_back(
      std::make_shared<ExprLiteral>(RawConstant(context->INTERVAL()->getText()),
                                    ExprLiteral::LiteralType::KEYWORD));
  inner_params.push_back(ParseExpr(context->expr(1)));
  inner_params.push_back(std::make_shared<ExprLiteral>(
      RawConstant(context->date_unit()->getText()),
      ExprLiteral::LiteralType::KEYWORD));
  params.push_back(std::make_shared<ExprFunction>(
      RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, inner_params));
  return params;
}
std::vector<std::shared_ptr<ExprToken>>
OBMySQLExprParser::AnalysisTimestampParams(
    OBParser::Timestamp_paramsContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  params.push_back(std::make_shared<ExprLiteral>(
      RawConstant(context->date_unit()->getText()),
      ExprLiteral::LiteralType::KEYWORD));
  for (const auto& e : context->expr()) {
    params.push_back(ParseExpr(e));
  }
  return params;
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisSysIntervalFunc(
    OBParser::Sys_interval_funcContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  for (const auto& e : context->expr()) {
    params.push_back(ParseExpr(e));
  }
  if (nullptr != context->expr_list()) {
    params.push_back(AnalysisExprList(context->expr_list()));
  }
  return std::make_shared<ExprFunction>(
      RawConstant(Util::GetCtxString(context)), FunctionType::INTERVAL, params);
}
std::vector<std::shared_ptr<ExprToken>> OBMySQLExprParser::AnalysisExprAsList(
    OBParser::Expr_as_listContext* context) {
  std::vector<std::shared_ptr<ExprToken>> params;
  for (const auto& e : context->expr_with_opt_alias()) {
    params.push_back(AnalysisExprWithOptAlias(e));
  }
  return params;
}
std::shared_ptr<ExprToken> OBMySQLExprParser::AnalysisExprWithOptAlias(
    OBParser::Expr_with_opt_aliasContext* ctx) {
  std::vector<std::shared_ptr<ExprToken>> params;
  params.push_back(ParseExpr(ctx->expr()));
  if (nullptr != ctx->AS()) {
    params.push_back(std::make_shared<ExprLiteral>(
        RawConstant(ctx->AS()->getText()), ExprLiteral::LiteralType::KEYWORD));
  }
  if (nullptr != ctx->column_label()) {
    params.push_back(
        std::make_shared<ExprColumnRef>(OBMySQLObjectParser::ProcessColumnName(
            ctx->column_label()->getText())));
  } else if (nullptr != ctx->STRING_VALUE()) {
    params.push_back(std::make_shared<ExprLiteral>(
        RawConstant(ctx->STRING_VALUE()->getText()),
        ExprLiteral::LiteralType::KEYWORD));
  }
  return std::make_shared<ExprFunction>(
      RawConstant(), FunctionType::CONCAT_PARAMS_WITH_SPACE, params);
}
}  // namespace source

}  // namespace etransfer
