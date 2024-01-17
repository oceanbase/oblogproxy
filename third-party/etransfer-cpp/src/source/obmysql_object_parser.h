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
#include <antlr4-runtime.h>

#include "OBParser.h"
#include "OBParserBaseListener.h"
#include "convert/ddl_parser_context.h"
#include "object/alter_table_column_object.h"
#include "object/column_attributes.h"
#include "object/object.h"
#include "object/option.h"
#include "object/table_check_constraint.h"
#include "object/table_constraint_object.h"
#include "object/table_partition_object.h"
#include "object/table_reference_consraint_object.h"
#include "source/obmysql_expr_parser.h"
namespace etransfer {
namespace source {
using namespace oceanbase;
using namespace common;
using namespace object;
class OBMySQLObjectParser : public OBParserBaseListener {
 public:
  OBMySQLObjectParser(std::shared_ptr<source::ParseContext> ddl_parse_context)
      : ddl_parse_context(ddl_parse_context), expr_parser(ddl_parse_context) {}
  ObjectPtr parent_object;
  std::shared_ptr<source::ParseContext> ddl_parse_context;
  OBMySQLExprParser expr_parser;
  Catalog catalog;
  RawConstant table_name;
  ObjectPtrs actions;
  ObjectType s_action = ObjectType::NOT_INTEREST;
  std::vector<RawConstant> anonymous_primary_key_columns;
  std::vector<RawConstant> anonymous_unique_key_columns;
  std::vector<RawConstant> anonymous_normal_key_columns;
  int un_index = 1;
  void enterCreate_table_stmt(OBParser::Create_table_stmtContext*) override;
  void exitCreate_table_stmt(OBParser::Create_table_stmtContext*) override;
  void enterCreate_table_like_stmt(
      OBParser::Create_table_like_stmtContext*) override;
  void exitCreate_table_like_stmt(
      OBParser::Create_table_like_stmtContext*) override;
  void enterDrop_table_stmt(OBParser::Drop_table_stmtContext*) override;
  void exitDrop_table_stmt(OBParser::Drop_table_stmtContext*) override;
  void enterTruncate_table_stmt(OBParser::Truncate_table_stmtContext*) override;
  void exitTruncate_table_stmt(OBParser::Truncate_table_stmtContext*) override;
  std::pair<Catalog, RawConstant> RetrieveSchemaAndTableName(
      OBParser::Relation_factorContext* relation_factor_context);
  std::pair<Catalog, RawConstant> RetrieveSchemaAndTableName(
      OBParser::Normal_relation_factorContext* relation_factor_context);
  static RawConstant ProcessTableOrDbName(
      const std::string& str,
      std::shared_ptr<source::ParseContext> ddl_parse_context);
  static RawConstant ProcessMySqlEscapeTableOrDbName(
      const std::string& name, const std::string& escape_string, bool low_case);
  void exitSql_stmt(OBParser::Sql_stmtContext* context) override;
  void enterAlter_table_stmt(
      OBParser::Alter_table_stmtContext* context) override;

  void enterRename_table_stmt(
      OBParser::Rename_table_stmtContext* context) override;

  void enterCreate_index_stmt(
      OBParser::Create_index_stmtContext* context) override;

  void enterDrop_index_stmt(OBParser::Drop_index_stmtContext* context) override;

  void enterCreate_view_stmt(
      OBParser::Create_view_stmtContext* context) override;

  static RawConstant ProcessColumnName(const std::string& s) {
    return ProcessMySqlEscapeTableOrDbName(s, "`", false);
  }

 private:
  void GetTablePartitionDescribe(
      std::shared_ptr<std::vector<std::shared_ptr<Option>>> options,
      OBParser::Table_option_listContext* ctx);
  void GetTablePartitionDescribe(
      std::shared_ptr<std::vector<std::shared_ptr<Option>>> options,
      OBParser::Table_option_list_space_seperatedContext* ctx);
  void ProcessTableBodyDefinition(OBParser::Table_elementContext* ctx);
  std::shared_ptr<ObjectPtrs> ProcessOptPartitionOption(
      OBParser::Opt_partition_optionContext* ctx);
  std::vector<std::shared_ptr<Option>> ResolveTableOption(
      OBParser::Table_optionContext* ctx);
  ObjectPtrs comment_list_;
  std::unordered_map<std::string, ObjectPtrs> inline_check_constraints_;
  std::shared_ptr<TableColumnObject> ProcessColumnDefinition(
      OBParser::Column_definitionContext* ctx);
  std::shared_ptr<TableColumnObject> ProcessColumnDefinition(
      OBParser::Column_definitionContext* ctx,
      std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
          alter_column_types);
  RawConstant GetColumnName(OBParser::Column_definition_refContext* ctx);
  void ProcessAlterTableActions(OBParser::Alter_table_actionsContext* ctx);
  void ProcessAlterTableAction(OBParser::Alter_table_actionContext* ctx);
  void ProcessAlterTableAlterColumnAction(
      OBParser::Alter_column_optionContext* ctx);
  std::vector<OBParser::Column_attributeContext*>
  RetrieveColumnAttributeContext(
      OBParser::Opt_column_attribute_listContext* opt_column_attribute_list);
  void ProcessColumnAttributes(
      const std::vector<OBParser::Column_attributeContext*>&
          column_attribute_contexts,
      std::shared_ptr<ColumnAttributes> column_attributes,
      std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
          alter_column_types,
      const RawConstant& column_name);
  ////process constraint name , partition name, index name
  RawConstant HandleIdentifierName(const std::string& s);

  std::shared_ptr<TableCheckConstraint> ProcessCheckConstraint(
      OBParser::ExprContext* expr, const RawConstant& constraint_name);
  template <typename T>
  static void AnalysisTokens(
      antlr4::tree::ParseTree* c,
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> sink);
  std::vector<OBParser::Generated_column_attributeContext*>
  RetrieveGenColumnAttributeContext(
      OBParser::Opt_generated_column_attribute_listContext* ctx);

  std::vector<RawConstant> RetrieveColumnLists(
      OBParser::Column_name_listContext* ctx) {
    std::vector<RawConstant> res;
    for (const auto& column : ctx->column_name()) {
      res.push_back(ProcessColumnName(Util::GetCtxString(column)));
    }
    return res;
  }

  std::shared_ptr<
      std::unordered_map<std::string, TableConstraintObject::ColumnInfo>>
  RetrieveKeyInfo(OBParser::Sort_column_listContext* ctx);
  std::vector<std::shared_ptr<PartitionSlice>> ProcessHashPartitionList(
      OBParser::Hash_partition_listContext* c);
  std::shared_ptr<GeneralPartitionInfo> ProcessSubpartitionOption(
      OBParser::Subpartition_optionContext* c);
  std::vector<std::shared_ptr<PartitionSlice>> ProcessRangePartitionList(
      OBParser::Range_partition_listContext* c);
  std::vector<std::shared_ptr<PartitionSlice>> ProcessListPartitionList(
      OBParser::List_partition_listContext* c);
  std::vector<std::shared_ptr<PartitionSlice>> ProcessOptHashSubpartitionList(
      OBParser::Opt_hash_subpartition_listContext* c);
  std::vector<std::shared_ptr<PartitionSlice>> ProcessOptListSubpartitionList(
      OBParser::Opt_list_subpartition_listContext* c);
  std::vector<std::shared_ptr<PartitionSlice>> ProcessOptRangeSubpartitionList(
      OBParser::Opt_range_subpartition_listContext* c);

  // add index, drop index, rename index, alter index attribute
  void ProcessAlterTableIndex(OBParser::Alter_index_optionContext* ctx);

  void ProcessAlterTableOption(
      OBParser::Table_option_list_space_seperatedContext* ctx);

  // drop indexs, add check
  void ProcessAlterTableConstraint(
      OBParser::Alter_constraint_optionContext* ctx);

  void ProcessAlterPartition(OBParser::Alter_partition_optionContext* ctx);

  std::shared_ptr<TablePartitionObject> ProcessPartitionOption(
      OBParser::Partition_optionContext* c);

  void RetrieveNameList(OBParser::Name_listContext* ctx,
                        std::shared_ptr<std::vector<RawConstant>> sink);
  void ProcessOptPartitionRangeOrList(
      OBParser::Opt_partition_range_or_listContext* ctx);
  void ProcessGenColumnAttributes(
      const std::vector<OBParser::Generated_column_attributeContext*>&
          gen_column_attribute_contexts,
      std::shared_ptr<ColumnAttributes> column_attributes_builder,
      std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
          alter_column_types,
      const RawConstant& column_name);
  void ProcessAlterForeignKey(OBParser::Alter_foreign_key_actionContext* ctx);

  TableReferenceConstraintObject::FKReferenceOperationCascade
  ResolveReferenceType(OBParser::Reference_optionContext* ctx);

  void RetrieveReferenceOption(
      OBParser::Opt_reference_option_listContext* ctx,
      std::shared_ptr<std::vector<
          TableReferenceConstraintObject::FKReferenceOperationCascade>>
          sink);
  static void PutOptionIfMiss(
      std::shared_ptr<std::vector<std::shared_ptr<Option>>> option_objects,
      OptionType option_type, std::shared_ptr<Option> to_put);
  void RetrieveComment(OBParser::Opt_index_optionsContext* ctx,
                       const std::string& index_name);
};
};  // namespace source
};  // namespace etransfer