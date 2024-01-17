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
#include "OBParser.h"
#include "convert/ddl_parser_context.h"
#include "object/expr_token.h"
namespace etransfer {
namespace source {
using namespace oceanbase;
using namespace object;
class OBMySQLExprParser {
 private:
  std::shared_ptr<ParseContext> parse_context_;

 public:
  OBMySQLExprParser(std::shared_ptr<ParseContext> context)
      : parse_context_(context) {}
  std::shared_ptr<ExprToken> AnalysisNowOrSignedLiteral(
      OBParser::Now_or_signed_literalContext* context);
  // Signed_literal
  std::shared_ptr<ExprToken> AnalysisSignedLiteral(
      OBParser::Signed_literalContext* context);

  // cur_timestamp_func
  std::shared_ptr<ExprToken> AnalysisCurTimestampFunc(
      OBParser::Cur_timestamp_funcContext* context);

  // literal
  std::shared_ptr<ExprToken> AnalysisLiteral(OBParser::LiteralContext* context);

  // Complex_string_literal
  std::shared_ptr<ExprToken> AnalysisComplexStringLiteral(
      OBParser::Complex_string_literalContext* context);

  std::shared_ptr<ExprToken> ParseExpr(OBParser::ExprContext* context);

  std::shared_ptr<ExprToken> AnalysisBoolPri(
      OBParser::Bool_priContext* context);

  std::shared_ptr<ExprToken> AnalysisPredicate(
      OBParser::PredicateContext* context);

  std::shared_ptr<ExprToken> AnalysisBitExpr(
      OBParser::Bit_exprContext* context);

  std::shared_ptr<ExprToken> AnalysisSimpleExpr(
      OBParser::Simple_exprContext* context);

  std::shared_ptr<ExprToken> AnalysisInExpr(OBParser::In_exprContext* context);
  std::vector<std::shared_ptr<ExprToken>> AnalysisStringValList(
      OBParser::String_val_listContext* ctx);

  std::shared_ptr<ExprToken> AnalysisCaseExpr(
      OBParser::Case_exprContext* context);

  std::shared_ptr<ExprToken> AnalysisColumnRef(
      OBParser::Column_refContext* ctx);

  std::shared_ptr<ExprToken> AnalysisExprConst(
      OBParser::Expr_constContext* ctx);

  std::shared_ptr<ExprToken> AnalysisExprList(
      OBParser::Expr_listContext* context);

  std::shared_ptr<ExprToken> AnalysisFuncExpr(
      OBParser::Func_exprContext* context);

  std::shared_ptr<ExprToken> ParseColumnDefinitionRef(
      OBParser::Column_definition_refContext* ctx);

  std::pair<std::shared_ptr<ExprToken>, std::shared_ptr<ExprToken>>
  AnalysisWhenClause(OBParser::When_clauseContext* when_clause_context);

  std::shared_ptr<ExprToken> AnalysisCastDataType(
      OBParser::Cast_data_typeContext* context);

  std::shared_ptr<ExprToken> AnalysisCaseDefault(
      OBParser::Case_defaultContext* case_default_context);

  std::shared_ptr<ExprToken> AnalysisExprs(
      const std::vector<OBParser::ExprContext*>& context);

  std::vector<std::shared_ptr<ExprToken>> AnalysisSubstrParams(
      OBParser::Substr_paramsContext* context);

  std::shared_ptr<ExprToken> AnalysisParameterizedTrim(
      OBParser::Parameterized_trimContext* context);

  std::vector<std::shared_ptr<ExprToken>> AnalysisDateParams(
      OBParser::Date_paramsContext* context);

  std::vector<std::shared_ptr<ExprToken>> AnalysisTimestampParams(
      OBParser::Timestamp_paramsContext* context);

  std::shared_ptr<ExprToken> AnalysisSysIntervalFunc(
      OBParser::Sys_interval_funcContext* context);

  std::vector<std::shared_ptr<ExprToken>> AnalysisExprAsList(
      OBParser::Expr_as_listContext* context);

  std::shared_ptr<ExprToken> AnalysisExprWithOptAlias(
      OBParser::Expr_with_opt_aliasContext* ctx);
};
}  // namespace source

}  // namespace etransfer
