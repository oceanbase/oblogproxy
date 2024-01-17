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
#include "sink/expr_builder.h"

namespace etransfer {
namespace sink {
using namespace object;
using namespace common;
class MySQLExprBuilder : public ExprBuilder {
 public:
  MySQLExprBuilder() = default;
  std::string Build(std::shared_ptr<ExprToken> default_value,
                    RealDataType data_type,
                    std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildExpr(std::shared_ptr<ExprToken> param,
                        RealDataType data_type,
                        std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildExpr(std::shared_ptr<ExprToken> param,
                        RealDataType data_type,
                        std::shared_ptr<BuildContext> sql_builder_context,
                        bool is_param);

  std::string BuildColumn(std::shared_ptr<ExprToken> param,
                          std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildBinaryExpression(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildDbAndTableName(
      std::shared_ptr<ExprColumnRef> column,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildFunction(FunctionType function_type,
                            std::shared_ptr<ExprFunction> token,
                            RealDataType data_type,
                            std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildFunction(std::shared_ptr<ExprToken> token,
                            RealDataType data_type,
                            std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildExprLiteral(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context, bool is_param);

  std::string ConvertHexValue(std::shared_ptr<ExprToken> token,
                              RealDataType data_type,
                              std::shared_ptr<BuildContext> sql_builder_context,
                              bool is_param);

  std::string ConvertDateString(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context, bool is_param);

  std::string ConvertTimeString(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context, bool is_param);

  std::string ConvertTimestampString(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context, bool is_param);

  std::string NowBuild(std::shared_ptr<ExprToken> token, RealDataType datatype,
                       std::shared_ptr<BuildContext> sql_builder_context);

  std::string CurrentTimestampBuild(
      std::shared_ptr<ExprToken> token, RealDataType datatype,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string LocaltimeBuild(std::shared_ptr<ExprToken> token,
                             RealDataType datatype,
                             std::shared_ptr<BuildContext> sql_builder_context);

  std::string LocalTimestampBuild(
      std::shared_ptr<ExprToken> token, RealDataType datatype,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string FuncWithParamBuild(
      const std::string& function_name, std::shared_ptr<ExprToken> token,
      RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string ConcatParamsWithSpace(std::shared_ptr<ExprToken> token,
                                    RealDataType data_type,
                                    std::shared_ptr<BuildContext> context) {
    return ConcatParamsWithDelimiter(token, data_type, context, " ");
  }

  std::string ConcatParamsWithDot(std::shared_ptr<ExprToken> token,
                                  RealDataType data_type,
                                  std::shared_ptr<BuildContext> context) {
    return ConcatParamsWithDelimiter(token, data_type, context, ",");
  }
  std::string ConcatParamsWithDelimiter(std::shared_ptr<ExprToken> token,
                                        RealDataType data_type,
                                        std::shared_ptr<BuildContext> context,
                                        const std::string& delimiter);
  Strings GetAllParams(std::shared_ptr<ExprToken> token, RealDataType data_type,
                       std::shared_ptr<BuildContext> sql_builder_context);
  std::string DateDiffBuild(std::shared_ptr<ExprToken> token,
                            RealDataType data_type,
                            std::shared_ptr<BuildContext> sql_builder_context);

  std::string JsonMergeBuild(std::shared_ptr<ExprToken> token,
                             RealDataType data_type,
                             std::shared_ptr<BuildContext> sql_builder_context);

  std::string JsonMergePatchBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string JsonMergePreserveBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string UnsupportedBuild(std::shared_ptr<ExprToken> token,
                               RealDataType data_type,
                               std::shared_ptr<BuildContext> context) {
    return token->token.origin_string;
  }

  std::string SpaceBuild(std::shared_ptr<ExprToken> token,
                         RealDataType data_type,
                         std::shared_ptr<BuildContext> sql_builder_context) {
    return FuncWithParamBuild("SPACE", token, data_type, sql_builder_context);
  }

  // operator
  std::string PeriodBuild(std::shared_ptr<ExprToken> token,
                          RealDataType data_type,
                          std::shared_ptr<BuildContext> context);

  std::string DotBuild(std::shared_ptr<ExprToken> token, RealDataType data_type,
                       std::shared_ptr<BuildContext> context);

  std::string AndSymbolBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::AND_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string BetweenAndSymbolBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string InSymbolBuild(std::shared_ptr<ExprToken> token,
                            RealDataType data_type,
                            std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::IN_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string NotInSymbolBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::NOT_IN_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string LikeSymbolBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::LIKE_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string NotLikeSymbolBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::NOT_LIKE_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string IsSymbolBuild(std::shared_ptr<ExprToken> token,
                            RealDataType data_type,
                            std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::IS_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string IsNotSymbolBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context) {
    return BaseSymbolBuild(token, OperatorType::IS_NOT_SYMBOL, data_type,
                           sql_builder_context);
  }

  std::string NotSymbolBuild(std::shared_ptr<ExprToken> token,
                             RealDataType data_type,
                             std::shared_ptr<BuildContext> context) {
    std::shared_ptr<SimpleExpr> expr =
        std::dynamic_pointer_cast<SimpleExpr>(token);
    return "NOT " + BuildExpr(expr->GetRightExp(), data_type, context);
  }

  std::string JsonExtractUnQuotedBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string JsonSeparatorBuild(
      std::shared_ptr<ExprToken> token, RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string CaseWhenBuild(std::shared_ptr<ExprToken> token,
                            RealDataType data_type,
                            std::shared_ptr<BuildContext> context);

  std::string BaseOperatorBuild(std::shared_ptr<ExprToken> token,
                                OperatorType type, RealDataType data_type,
                                std::shared_ptr<BuildContext> context);

  std::string BaseSymbolBuild(
      std::shared_ptr<ExprToken> token, OperatorType type,
      RealDataType data_type,
      std::shared_ptr<BuildContext> sql_builder_context);
};
}  // namespace sink

}  // namespace etransfer
