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

#include "source/obmysql_object_parser.h"

#include <iostream>

#include "common/catalog.h"
#include "common/raw_constant.h"
#include "common/util.h"
#include "object/alter_table_column_object.h"
#include "object/alter_table_object.h"
#include "object/comment_object.h"
#include "object/create_table_object.h"
#include "object/drop_index_object.h"
#include "object/drop_table_column_object.h"
#include "object/drop_table_constraint_object.h"
#include "object/drop_table_object.h"
#include "object/partition_object.h"
#include "object/rename_index_object.h"
#include "object/rename_table_object.h"
#include "object/table_column_object.h"
#include "object/table_constraint_object.h"
#include "object/table_partition_object.h"
#include "object/table_reference_consraint_object.h"
#include "object/truncate_table_object.h"
#include "source/type_helper.h"
namespace etransfer {
namespace source {
using namespace oceanbase;
using namespace object;
using namespace common;
// create table like
void OBMySQLObjectParser::enterCreate_table_like_stmt(
    OBParser::Create_table_like_stmtContext* ctx) {
  auto db_and_table_name =
      RetrieveSchemaAndTableName(ctx->relation_factor().at(0));

  std::shared_ptr<CreateTableDDLTricks> create_table_ddl_tricks =
      std::make_shared<CreateTableDDLTricks>();
  if (ctx->temporary_option() != nullptr &&
      ctx->temporary_option()->TEMPORARY() != nullptr) {
    create_table_ddl_tricks->is_temporary_table = true;
  }
  if (ctx->IF() != nullptr) {
    create_table_ddl_tricks->create_if_not_exists = true;
  }

  auto like_db_and_table_name =
      RetrieveSchemaAndTableName(ctx->relation_factor().at(1));

  Catalog like_catalog(like_db_and_table_name.first);
  ObjectPtr like_table_object = std::make_shared<CreateTableObject>(
      like_catalog, like_db_and_table_name.second, Util::GetCtxString(ctx),
      nullptr, nullptr);
  create_table_ddl_tricks->is_create_like = true;
  create_table_ddl_tricks->create_like_syntax = like_table_object;
  parent_object = std::make_shared<CreateTableObject>(
      db_and_table_name.first, db_and_table_name.second,
      ddl_parse_context->raw_ddl, nullptr, create_table_ddl_tricks);
}
void OBMySQLObjectParser::exitCreate_table_like_stmt(
    OBParser::Create_table_like_stmtContext*) {}
// create table
void OBMySQLObjectParser::enterCreate_table_stmt(
    OBParser::Create_table_stmtContext* ctx) {
  auto db_and_table_name = RetrieveSchemaAndTableName(ctx->relation_factor());
  catalog = db_and_table_name.first;
  table_name = db_and_table_name.second;
  auto create_table_ddl_tricks = std::make_shared<CreateTableDDLTricks>();
  if (ctx->temporary_option() != nullptr &&
      ctx->temporary_option()->TEMPORARY() != nullptr) {
    create_table_ddl_tricks->is_temporary_table = true;
  }
  if (ctx->IF() != nullptr) {
    create_table_ddl_tricks->create_if_not_exists = true;
  }
  if (ctx->AS() != nullptr || ctx->select_stmt() != nullptr) {
    ddl_parse_context->SetErrMsg("not support create table select as option");
    return;
  }
  // process table options
  auto options = std::make_shared<std::vector<std::shared_ptr<Option>>>();
  GetTablePartitionDescribe(options, ctx->table_option_list());
  // set default charset to given charset if provided
  if (!ddl_parse_context->default_charset.empty()) {
    PutOptionIfMiss(options, OptionType::TABLE_CHARSET,
                    std::make_shared<TableCharsetOption>(
                        ddl_parse_context->default_charset));
  }
  // set default collation to given collation if provided
  if (!ddl_parse_context->default_collection.empty()) {
    PutOptionIfMiss(options, OptionType::TABLE_COLLATION,
                    std::make_shared<TableCollationOption>(
                        ddl_parse_context->default_collection));
  }
  // process table body definition
  for (const auto& table_element : ctx->table_element_list()->table_element()) {
    ProcessTableBodyDefinition(table_element);
  }
  // process partition info
  auto table_partition_objects =
      ProcessOptPartitionOption(ctx->opt_partition_option());
  parent_object = std::make_shared<CreateTableObject>(
      db_and_table_name.first, db_and_table_name.second,
      ddl_parse_context->raw_ddl, table_partition_objects,
      create_table_ddl_tricks, options);
}
void OBMySQLObjectParser::GetTablePartitionDescribe(
    std::shared_ptr<std::vector<std::shared_ptr<Option>>> options,
    OBParser::Table_option_listContext* ctx) {
  if (ctx == nullptr) {
    return;
  }
  if (ctx->Comma() != nullptr) {
    std::vector<std::shared_ptr<Option>> table_option_describe =
        ResolveTableOption(ctx->table_option());
    options->insert(options->end(), table_option_describe.begin(),
                    table_option_describe.end());
    GetTablePartitionDescribe(options, ctx->table_option_list());
  } else {
    GetTablePartitionDescribe(options,
                              ctx->table_option_list_space_seperated());
  }
}

void OBMySQLObjectParser::PutOptionIfMiss(
    std::shared_ptr<std::vector<std::shared_ptr<Option>>> option_objects,
    OptionType option_type, std::shared_ptr<Option> to_put) {
  if (nullptr == to_put || nullptr == option_objects) {
    return;
  }
  bool find = false;
  for (auto option_object : *option_objects) {
    if (option_object->GetOptionType() == option_type) {
      find = true;
      break;
    }
  }
  if (!find) {
    option_objects->push_back(to_put);
  }
}

void OBMySQLObjectParser::GetTablePartitionDescribe(
    std::shared_ptr<std::vector<std::shared_ptr<Option>>> options,
    OBParser::Table_option_list_space_seperatedContext* ctx) {
  if (ctx == nullptr) {
    return;
  }
  std::vector<std::shared_ptr<Option>> table_option_describe =
      ResolveTableOption(ctx->table_option());
  options->insert(options->end(), table_option_describe.begin(),
                  table_option_describe.end());
  if (ctx->table_option_list_space_seperated() != nullptr) {
    GetTablePartitionDescribe(options,
                              ctx->table_option_list_space_seperated());
  }
}
std::vector<std::shared_ptr<Option>> OBMySQLObjectParser::ResolveTableOption(
    OBParser::Table_optionContext* ctx) {
  std::vector<std::shared_ptr<Option>> options;
  if (ctx->COMMENT() != nullptr) {
    comment_list_.push_back(std::make_shared<CommentObject>(
        catalog, table_name, Util::GetCtxString(ctx),
        CommentObject::CommentTargetType::TABLE,
        CommentObject::CommentPair(table_name,
                                   ctx->STRING_VALUE()->getText())));
    options.push_back(
        std::make_shared<CommentOption>(ctx->STRING_VALUE()->getText()));
  } else if (ctx->charset_key() != nullptr) {
    options.push_back(std::make_shared<TableCharsetOption>(
        Util::GetCtxString(ctx->charset_name())));
  } else if (ctx->collation_name() != nullptr) {
    options.push_back(std::make_shared<TableCollationOption>(
        Util::GetCtxString(ctx->collation_name())));
  } else {
    // do nothing
  }
  return options;
}

void OBMySQLObjectParser::ProcessTableBodyDefinition(
    OBParser::Table_elementContext* ctx) {
  if (ctx->column_definition() != nullptr) {
    // process table column definition
    actions.push_back(ProcessColumnDefinition(ctx->column_definition()));
  } else if (ctx->PRIMARY() != nullptr) {
    // process pk define
    RawConstant constraint_name;
    if (nullptr != ctx->constraint_name() &&
        nullptr != ctx->constraint_name()) {
      constraint_name =
          HandleIdentifierName(Util::GetCtxString(ctx->constraint_name()));
    }
    std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> columns =
        std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();

    auto retrieves = RetrieveColumnLists(ctx->column_name_list().at(0));
    for (const auto& retrieve : retrieves) {
      columns->push_back(std::make_shared<ExprColumnRef>(retrieve));
    }
    actions.push_back(std::make_shared<TableConstraintObject>(
        catalog, table_name, Util::GetCtxString(ctx), constraint_name,
        IndexType::PRIMARY, nullptr, columns));
  } else if (ctx->sort_column_list() != nullptr) {
    // handle normal key and unique key and full text key
    RawConstant index_name;
    if (ctx->index_name() != nullptr) {
      index_name = HandleIdentifierName(Util::GetCtxString(ctx->index_name()));
    }
    if (ctx->constraint_name() != nullptr) {
      index_name =
          HandleIdentifierName(Util::GetCtxString(ctx->constraint_name()));
    }
    IndexType index_type = IndexType::NORMAL;
    if (ctx->UNIQUE() != nullptr) {
      index_type = IndexType::UNIQUE;
    } else if (ctx->FULLTEXT() != nullptr) {
      index_type = IndexType::FULLTEXT;
    }
    auto column_info_map = RetrieveKeyInfo(ctx->sort_column_list());
    std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affected_columns =
        std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
    std::shared_ptr<std::vector<RawConstant>> pre_index_lengths =
        std::make_shared<std::vector<RawConstant>>();
    for (const auto column_key : ctx->sort_column_list()->sort_column_key()) {
      affected_columns->push_back(std::make_shared<ExprColumnRef>(
          ProcessColumnName(Util::GetCtxString(column_key->column_name()))));
      pre_index_lengths->push_back(
          nullptr != column_key->LeftParen()
              ? RawConstant(column_key->LeftParen()->getText() +
                            column_key->INTNUM(0)->getText() +
                            column_key->RightParen()->getText())
              : RawConstant());
    }
    actions.push_back(std::make_shared<TableConstraintObject>(
        catalog, table_name, Util::GetCtxString(ctx), index_name, index_type,
        column_info_map, affected_columns, pre_index_lengths));
  } else if (ctx->constraint_definition() != nullptr &&
             ctx->constraint_definition()->CHECK() != nullptr) {
    // handle check
    RawConstant constraint_name =
        nullptr != ctx->constraint_definition()->constraint_name()
            ? HandleIdentifierName(Util::GetCtxString(
                  ctx->constraint_definition()->constraint_name()))
            : RawConstant();
    actions.push_back(ProcessCheckConstraint(
        ctx->constraint_definition()->expr(), constraint_name));
  } else if (ctx->FOREIGN() != nullptr) {
    // handle foreign key
    RawConstant index_name;
    if (ctx->index_name() != nullptr) {
      index_name = HandleIdentifierName(Util::GetCtxString(ctx->index_name()));
    }
    if (ctx->constraint_name() != nullptr) {
      index_name = HandleIdentifierName(
          Util::GetCtxString(ctx->constraint_name()->relation_name()));
    }
    auto columns = RetrieveColumnLists(ctx->column_name_list().at(0));
    std::pair<Catalog, RawConstant> parent_table_info =
        RetrieveSchemaAndTableName(ctx->relation_factor());
    auto parents_columns = RetrieveColumnLists(ctx->column_name_list().at(1));
    auto cascade_options = std::make_shared<std::vector<
        TableReferenceConstraintObject::FKReferenceOperationCascade>>();
    if (nullptr != ctx->reference_option()) {
      cascade_options->push_back(ResolveReferenceType(ctx->reference_option()));
      RetrieveReferenceOption(ctx->opt_reference_option_list(),
                              cascade_options);
    }
    actions.push_back(std::make_shared<TableReferenceConstraintObject>(
        catalog, table_name, Util::GetCtxString(ctx), index_name,
        IndexType::FOREIGN, std::make_shared<std::vector<RawConstant>>(columns),
        TableReferenceConstraintObject::FKReferenceRange::PART_OF_TABLE_COLUMNS,
        parent_table_info.first, parent_table_info.second,
        std::make_shared<std::vector<RawConstant>>(parents_columns),
        cascade_options));
  }
}

std::shared_ptr<
    std::unordered_map<std::string, TableConstraintObject::ColumnInfo>>
OBMySQLObjectParser::RetrieveKeyInfo(OBParser::Sort_column_listContext* ctx) {
  std::shared_ptr<
      std::unordered_map<std::string, TableConstraintObject::ColumnInfo>>
      ret = std::make_shared<
          std::unordered_map<std::string, TableConstraintObject::ColumnInfo>>();
  for (const auto& column_key : ctx->sort_column_key()) {
    std::string column_name =
        ProcessColumnName(Util::GetCtxString(column_key->column_name()))
            .origin_string;
    int precision = TypeInfo::ANONYMOUS_NUMBER;
    TableConstraintObject::ConstraintSortOrder order =
        TableConstraintObject::ConstraintSortOrder::DEFAULT;
    if (!column_key->INTNUM().empty() && column_key->LeftParen() != nullptr) {
      precision = std::stoi(column_key->INTNUM().at(0)->getText());
    }
    if (column_key->DESC() != nullptr) {
      order = TableConstraintObject::ConstraintSortOrder::DESC;
    } else if (column_key->ASC() != nullptr) {
      order = TableConstraintObject::ConstraintSortOrder::ASC;
    }
    ret->insert(std::make_pair(
        column_name, TableConstraintObject::ColumnInfo(order, precision)));
  }
  return ret;
}
std::shared_ptr<TableColumnObject> OBMySQLObjectParser::ProcessColumnDefinition(
    OBParser::Column_definitionContext* ctx) {
  std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
      alter_column_types = std::make_shared<
          std::vector<AlterTableColumnObject::AlterColumnType>>();
  return ProcessColumnDefinition(ctx, alter_column_types);
}
std::shared_ptr<TableColumnObject> OBMySQLObjectParser::ProcessColumnDefinition(
    OBParser::Column_definitionContext* ctx,
    std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
        alter_column_types) {
  RawConstant column_name = GetColumnName(ctx->column_definition_ref());
  std::shared_ptr<ColumnAttributes> column_attributes =
      std::make_shared<ColumnAttributes>();
  std::pair<RealDataType, std::shared_ptr<TypeInfo>> data_type_and_info =
      source::TypeHelper::ParseDataType(ctx->data_type());
  if (data_type_and_info.first == RealDataType::UNSUPPORTED) {
    ddl_parse_context->SetErrMsg("parse data type failed");
    return nullptr;
  }
  if (data_type_and_info.first == RealDataType::VARBINARY ||
      data_type_and_info.first == RealDataType::BINRAY ||
      data_type_and_info.first == RealDataType::BIT) {
    column_attributes->SetBinary(true);
  }
  std::vector<OBParser::Column_attributeContext*> column_attribute_contexts;
  if (ctx->opt_column_attribute_list() != nullptr) {
    auto retrieve_contexts =
        RetrieveColumnAttributeContext(ctx->opt_column_attribute_list());
    column_attribute_contexts.insert(column_attribute_contexts.end(),
                                     retrieve_contexts.begin(),
                                     retrieve_contexts.end());
    if (!column_attribute_contexts.empty()) {
      ProcessColumnAttributes(column_attribute_contexts, column_attributes,
                              alter_column_types, column_name);
    }
  }
  if (nullptr != ctx->opt_generated_column_attribute_list()) {
    std::vector<OBParser::Generated_column_attributeContext*>
        gen_column_attribute_contexts = RetrieveGenColumnAttributeContext(
            ctx->opt_generated_column_attribute_list());
    if (!gen_column_attribute_contexts.empty()) {
      ProcessGenColumnAttributes(gen_column_attribute_contexts,
                                 column_attributes, alter_column_types,
                                 column_name);
    }
  }

  // Generate column : default store type : VIRTUAL
  if (ctx->AS() != nullptr) {
    GenStoreType store_type = GenStoreType::INVALID;
    if (nullptr != ctx->STORED()) {
      store_type = GenStoreType::STORED;
    }
    if (nullptr != ctx->VIRTUAL()) {
      store_type = GenStoreType::VIRTUAL;
    }
    column_attributes->SetGenerated(expr_parser.ParseExpr(ctx->expr()),
                                    store_type);
  }

  // position: FIRST/BEFORE xx/AFTER xxx
  std::shared_ptr<ColumnLocationIdentifier> position = nullptr;
  if (ctx->FIRST() != nullptr) {
    position = std::make_shared<ColumnLocationIdentifier>(
        ColumnLocationIdentifier::HEAD);
  } else if (ctx->BEFORE() != nullptr) {
    RawConstant col_name = ProcessColumnName(ctx->column_name()->getText());
    position = std::make_shared<ColumnLocationIdentifier>(
        ColumnLocationIdentifier::BEFORE, col_name, RawConstant());
  } else if (ctx->AFTER() != nullptr) {
    RawConstant col_name = ProcessColumnName(ctx->column_name()->getText());
    position = std::make_shared<ColumnLocationIdentifier>(
        ColumnLocationIdentifier::AFTER, RawConstant(), col_name);
  } else {
    position = std::make_shared<ColumnLocationIdentifier>(
        ColumnLocationIdentifier::TAIL);
  }

  std::shared_ptr<TableColumnObject> table_column_object =
      std::make_shared<TableColumnObject>(
          catalog, table_name, Util::GetCtxString(ctx), column_name,
          data_type_and_info.first, data_type_and_info.second,
          column_attributes, position);

  if (0 != inline_check_constraints_.count(column_name.value)) {
    table_column_object->SetSubObjects(
        inline_check_constraints_.at(column_name.value));
  }
  return table_column_object;
}
////process constraint name , partition name, index name
RawConstant OBMySQLObjectParser::HandleIdentifierName(const std::string& s) {
  return Util::ProcessEscapeString(s, "`");
}

void OBMySQLObjectParser::ProcessGenColumnAttributes(
    const std::vector<OBParser::Generated_column_attributeContext*>&
        gen_column_attribute_contexts,
    std::shared_ptr<ColumnAttributes> column_attributes,
    std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
        alter_column_types,
    const RawConstant& column_name) {
  bool nullable = true;
  for (const auto& attr_ctx : gen_column_attribute_contexts) {
    // NULL/NOT NULL
    if (attr_ctx->NULLX() != nullptr) {
      nullable = attr_ctx->NOT() == nullptr;
      alter_column_types->push_back(
          AlterTableColumnObject::AlterColumnType::CHANGE_NULLABLE);
    }
    column_attributes->SetIsNullable(nullable);

    // PK UK KEY
    if (attr_ctx->UNIQUE() != nullptr) {
      anonymous_unique_key_columns.push_back(column_name);
      column_attributes->SetAnonymousKeyType(IndexType::UNIQUE);
    } else if (attr_ctx->PRIMARY() != nullptr) {
      anonymous_primary_key_columns.push_back(column_name);
      column_attributes->SetAnonymousKeyType(IndexType::PRIMARY);
    } else if (attr_ctx->KEY() != nullptr) {
      if (!column_attributes->IsAnonymousKey()) {
        anonymous_normal_key_columns.push_back(column_name);
        column_attributes->SetAnonymousKeyType(IndexType::NORMAL);
      }
    }

    // COMMENT
    if (attr_ctx->COMMENT() != nullptr) {
      column_attributes->SetComment(attr_ctx->STRING_VALUE()->getText());
      comment_list_.push_back(std::make_shared<CommentObject>(
          catalog, table_name, Util::GetCtxString(attr_ctx),
          CommentObject::CommentTargetType::COLUMN,
          CommentObject::CommentPair(column_name,
                                     attr_ctx->STRING_VALUE()->getText())));
    }
    OBParser::Constraint_definitionContext* constraint_definition =
        attr_ctx->constraint_definition();
    // CHECK
    if (nullptr != constraint_definition &&
        nullptr != constraint_definition->CHECK()) {
      auto constraint =
          ProcessCheckConstraint(constraint_definition->expr(), RawConstant());
      inline_check_constraints_[column_name.value].push_back(constraint);
    }
  }
}

std::shared_ptr<TableCheckConstraint>
OBMySQLObjectParser::ProcessCheckConstraint(
    OBParser::ExprContext* expr, const RawConstant& constraint_name) {
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> check_tokens =
      std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
  AnalysisTokens<OBParser::Column_nameContext>(expr, check_tokens);
  return std::make_shared<TableCheckConstraint>(
      catalog, table_name, constraint_name, Util::GetCtxString(expr),
      check_tokens);
}
template <typename T>
void OBMySQLObjectParser::AnalysisTokens(
    antlr4::tree::ParseTree* c,
    std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> sink) {
  for (int i = 0; i < c->children.size(); ++i) {
    auto p = c->children[i];
    if (T* v = dynamic_cast<T*>(p)) {
      sink->push_back(std::make_shared<ExprColumnRef>(
          Util::ProcessEscapeString(Util::GetCtxString(v), "`")));
    } else if (antlr4::tree::TerminalNode* v =
                   dynamic_cast<antlr4::tree::TerminalNode*>(p)) {
      RawConstant raw_constant(v->getText());
      sink->push_back(std::make_shared<ExprLiteral>(raw_constant));
    } else {
      AnalysisTokens<T>(p, sink);
    }
  }
}

std::vector<OBParser::Generated_column_attributeContext*>
OBMySQLObjectParser::RetrieveGenColumnAttributeContext(
    OBParser::Opt_generated_column_attribute_listContext* ctx) {
  std::vector<OBParser::Generated_column_attributeContext*> ret;
  if (ctx->generated_column_attribute() != nullptr) {
    ret.push_back(ctx->generated_column_attribute());
  }
  if (ctx->opt_generated_column_attribute_list() != nullptr) {
    auto column_attrs = RetrieveGenColumnAttributeContext(
        ctx->opt_generated_column_attribute_list());
    ret.insert(ret.end(), column_attrs.begin(), column_attrs.end());
  }
  return ret;
}

void OBMySQLObjectParser::ProcessColumnAttributes(
    const std::vector<OBParser::Column_attributeContext*>&
        column_attribute_contexts,
    std::shared_ptr<ColumnAttributes> column_attributes,
    std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
        alter_column_types,
    const RawConstant& column_name) {
  bool nullable = true;
  for (auto attr_ctx : column_attribute_contexts) {
    // NULL/NOT NULL
    if (attr_ctx->NULLX() != nullptr) {
      nullable = attr_ctx->not_() == nullptr;
      alter_column_types->push_back(
          AlterTableColumnObject::AlterColumnType::CHANGE_NULLABLE);
    }
    column_attributes->SetIsNullable(nullable);

    // DEFAULT
    if (attr_ctx->DEFAULT() != nullptr || attr_ctx->ORIG_DEFAULT() != nullptr) {
      column_attributes->SetDefaultValue(expr_parser.AnalysisNowOrSignedLiteral(
          attr_ctx->now_or_signed_literal()));
      alter_column_types->push_back(
          AlterTableColumnObject::AlterColumnType::CHANGE_DEFAULT);
    }

    // AUTO_INCREMENT
    if (attr_ctx->AUTO_INCREMENT() != nullptr) {
      column_attributes->SetAutoInc();
    }

    if (attr_ctx->UNIQUE() != nullptr) {
      anonymous_unique_key_columns.push_back(column_name);
      column_attributes->SetAnonymousKeyType(IndexType::UNIQUE);
    } else if (attr_ctx->PRIMARY() != nullptr) {
      anonymous_primary_key_columns.push_back(column_name);
      column_attributes->SetAnonymousKeyType(IndexType::PRIMARY);
    } else if (attr_ctx->KEY() != nullptr) {
      // such as unique key : unique in opt_column_attribute_list and key in
      // column_attribute, so unique and key exist in columnAttributeContexts at
      // the same time
      if (!column_attributes->IsAnonymousKey()) {
        anonymous_normal_key_columns.push_back(column_name);
        column_attributes->SetAnonymousKeyType(IndexType::NORMAL);
      }
    }

    // COMMENT
    if (attr_ctx->COMMENT() != nullptr) {
      column_attributes->SetComment(attr_ctx->STRING_VALUE()->getText());
      std::string comment = attr_ctx->STRING_VALUE()->getText();
      comment_list_.push_back(std::make_shared<CommentObject>(
          catalog, table_name, Util::GetCtxString(attr_ctx),
          CommentObject::CommentTargetType::COLUMN,
          CommentObject::CommentPair(column_name, comment)));
    }
    OBParser::Constraint_definitionContext* constraint_definition =
        attr_ctx->constraint_definition();
    // CHECK
    if (nullptr != constraint_definition &&
        nullptr != constraint_definition->CHECK()) {
      auto constraint = ProcessCheckConstraint(
          constraint_definition->expr(),
          nullptr != constraint_definition->constraint_name()
              ? HandleIdentifierName(Util::GetCtxString(
                    constraint_definition->constraint_name()))
              : RawConstant());
      inline_check_constraints_[column_name.value].push_back(constraint);
    }

    // ON UPDATE
    if (attr_ctx->ON() != nullptr) {
      column_attributes->SetOnUpdate(
          Util::GetCtxString(attr_ctx->cur_timestamp_func()));
    }

    if (attr_ctx->ORIG_DEFAULT() != nullptr || attr_ctx->ID() != nullptr) {
      ddl_parse_context->SetErrMsg("unsupported: " + attr_ctx->getText());
    }

    if (nullptr != attr_ctx->SRID()) {
      column_attributes->SetSpatialRefId(attr_ctx->INTNUM()->getText());
    }
  }
}

std::vector<OBParser::Column_attributeContext*>
OBMySQLObjectParser::RetrieveColumnAttributeContext(
    OBParser::Opt_column_attribute_listContext* opt_column_attribute_list) {
  std::vector<OBParser::Column_attributeContext*> ret;
  if (opt_column_attribute_list->column_attribute() != nullptr) {
    ret.push_back(opt_column_attribute_list->column_attribute());
  }
  if (opt_column_attribute_list->opt_column_attribute_list() != nullptr) {
    auto column_attrs = RetrieveColumnAttributeContext(
        opt_column_attribute_list->opt_column_attribute_list());
    ret.insert(ret.end(), column_attrs.begin(), column_attrs.end());
  }
  return ret;
}

RawConstant OBMySQLObjectParser::GetColumnName(
    OBParser::Column_definition_refContext* ctx) {
  return ProcessMySqlEscapeTableOrDbName(Util::GetCtxString(ctx->column_name()),
                                         ddl_parse_context->escape_char, false);
}

std::shared_ptr<ObjectPtrs> OBMySQLObjectParser::ProcessOptPartitionOption(
    OBParser::Opt_partition_optionContext* ctx) {
  auto res = std::make_shared<ObjectPtrs>();
  if (ctx->partition_option() != nullptr) {
    res->push_back(ProcessPartitionOption(ctx->partition_option()));
  } else if ((ctx->opt_column_partition_option() != nullptr &&
              ctx->opt_column_partition_option()->column_partition_option() !=
                  nullptr) ||
             ctx->auto_partition_option() != nullptr) {
    ddl_parse_context->SetErrMsg(
        "unsupported to parse other partition grammar");
  }
  return res;
}

std::shared_ptr<TablePartitionObject>
OBMySQLObjectParser::ProcessPartitionOption(
    OBParser::Partition_optionContext* partition_ctx) {
  PartitionType partition_type = PartitionType::NONE;
  std::vector<RawConstant> affect_columns;
  std::shared_ptr<Strings> options = std::make_shared<Strings>();
  int partition_num = -1;
  std::vector<std::shared_ptr<PartitionSlice>> partition_slices;
  std::shared_ptr<GeneralPartitionInfo> sub_partition_info;
  std::string expr;
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> expr_tokens =
      std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
  OBParser::ExprContext* expr_context = nullptr;
  bool contain_columns_keyword = false;
  if (partition_ctx->hash_partition_option() != nullptr) {
    OBParser::Hash_partition_optionContext* hash_partition_ctx =
        partition_ctx->hash_partition_option();
    partition_type = PartitionType::HASH;

    if (hash_partition_ctx->INTNUM() != nullptr) {
      partition_num = std::stoi(hash_partition_ctx->INTNUM()->getText());
    }
    if (nullptr != hash_partition_ctx->opt_hash_partition_list()) {
      partition_slices = ProcessHashPartitionList(
          hash_partition_ctx->opt_hash_partition_list()->hash_partition_list());
    }
    expr_context = hash_partition_ctx->expr();
    sub_partition_info =
        ProcessSubpartitionOption(hash_partition_ctx->subpartition_option());
  } else if (partition_ctx->range_partition_option() != nullptr) {
    partition_type = PartitionType::RANGE;
    OBParser::Range_partition_optionContext* range_ctx =
        partition_ctx->range_partition_option();
    if (nullptr != range_ctx->COLUMNS()) {
      contain_columns_keyword = true;
    }
    if (range_ctx->column_name_list() != nullptr) {
      affect_columns = RetrieveColumnLists(range_ctx->column_name_list());
    } else {
      expr_context = range_ctx->expr();
    }
    if (range_ctx->INTNUM() != nullptr) {
      partition_num = std::stoi(range_ctx->INTNUM()->getText());
    }
    partition_slices = ProcessRangePartitionList(
        range_ctx->opt_range_partition_list()->range_partition_list());
    sub_partition_info =
        ProcessSubpartitionOption(range_ctx->subpartition_option());
  } else if (partition_ctx->list_partition_option() != nullptr) {
    OBParser::List_partition_optionContext* list_ctx =
        partition_ctx->list_partition_option();
    partition_type = PartitionType::LIST;

    if (nullptr != list_ctx->COLUMNS()) {
      contain_columns_keyword = true;
    }
    if (list_ctx->column_name_list() != nullptr) {
      affect_columns = RetrieveColumnLists(list_ctx->column_name_list());
    } else {
      expr_context = list_ctx->expr();
    }
    if (list_ctx->INTNUM() != nullptr) {
      partition_num = std::stoi(list_ctx->INTNUM()->getText());
    }
    partition_slices = ProcessListPartitionList(
        list_ctx->opt_list_partition_list()->list_partition_list());
    sub_partition_info =
        ProcessSubpartitionOption(list_ctx->subpartition_option());
  } else {
    partition_type = PartitionType::KEY;
    OBParser::Key_partition_optionContext* key_ctx =
        partition_ctx->key_partition_option();
    if (nullptr != key_ctx->column_name_list()) {
      affect_columns = RetrieveColumnLists(key_ctx->column_name_list());
    }
    if (key_ctx->INTNUM() != nullptr) {
      partition_num = std::stoi(key_ctx->INTNUM()->getText());
    }
    if (nullptr != key_ctx->opt_hash_partition_list()) {
      partition_slices = ProcessHashPartitionList(
          key_ctx->opt_hash_partition_list()->hash_partition_list());
    }
    sub_partition_info =
        ProcessSubpartitionOption(key_ctx->subpartition_option());
  }
  if (nullptr != expr_context) {
    expr = Util::GetCtxString(expr_context);
    AnalysisTokens<OBParser::Column_nameContext>(expr_context, expr_tokens);
  }
  if (expr_tokens->size() == 1 &&
      expr_tokens->at(0)->GetTokenType() == ExprTokenType::IDENTIFIER_NAME) {
    affect_columns.clear();
    affect_columns.push_back(ProcessColumnName(expr));
  }
  auto general_partition_info = std::make_shared<GeneralPartitionInfo>(
      affect_columns.empty()
          ? std::make_shared<PartitionValueSourceInfo>(expr, expr_tokens)
          : std::make_shared<PartitionValueSourceInfo>(affect_columns),
      partition_type, partition_num, options, nullptr, contain_columns_keyword);
  std::shared_ptr<TablePartitionObject> table_partition_object =
      std::make_shared<TablePartitionObject>(catalog, table_name,
                                             Util::GetCtxString(partition_ctx),
                                             general_partition_info);
  for (const auto& slice : partition_slices) {
    table_partition_object->AddPartitionSlice(slice);
  }
  table_partition_object->SetSubPartitionInfo(sub_partition_info);
  return table_partition_object;
}

std::vector<std::shared_ptr<PartitionSlice>>
OBMySQLObjectParser::ProcessHashPartitionList(
    OBParser::Hash_partition_listContext* c) {
  std::vector<std::shared_ptr<PartitionSlice>> res;
  for (const auto& ctx : c->hash_partition_element()) {
    RawConstant partition_name =
        ctx->relation_factor() != nullptr
            ? HandleIdentifierName(Util::GetCtxString(ctx->relation_factor()))
            : RawConstant();
    int seq = -1;
    Strings options;
    if (ctx->INTNUM() != nullptr) {
      seq = std::stoi(ctx->INTNUM()->getText());
    }

    std::vector<std::shared_ptr<PartitionSlice>> partition_slices;
    if (ctx->opt_hash_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptHashSubpartitionList(ctx->opt_hash_subpartition_list());
    } else if (ctx->opt_list_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptListSubpartitionList(ctx->opt_list_subpartition_list());
    } else if (ctx->opt_range_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptRangeSubpartitionList(ctx->opt_range_subpartition_list());
    }
    res.push_back(std::make_shared<HashPartitionSlice>(
        partition_name,
        std::make_shared<std::vector<std::shared_ptr<PartitionSlice>>>(
            partition_slices),
        std::make_shared<Strings>(options), seq));
  }
  return res;
}
std::vector<std::shared_ptr<PartitionSlice>>
OBMySQLObjectParser::ProcessRangePartitionList(
    OBParser::Range_partition_listContext* c) {
  std::vector<std::shared_ptr<PartitionSlice>> res;
  for (const auto& ctx : c->range_partition_element()) {
    RawConstant partition_name =
        ctx->relation_factor() != nullptr
            ? HandleIdentifierName(ctx->relation_factor()->getText())
            : RawConstant();
    int seq = -1;
    Strings options;
    if (ctx->INTNUM() != nullptr) {
      seq = std::stoi(ctx->INTNUM()->getText());
    }
    std::vector<std::shared_ptr<PartitionSlice>> partition_slices;
    if (ctx->opt_hash_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptHashSubpartitionList(ctx->opt_hash_subpartition_list());
    } else if (ctx->opt_list_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptListSubpartitionList(ctx->opt_list_subpartition_list());
    } else if (ctx->opt_range_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptRangeSubpartitionList(ctx->opt_range_subpartition_list());
    }
    std::vector<RawConstant> end;
    if (ctx->range_partition_expr()->MAXVALUE() != nullptr) {
      end.push_back(
          RawConstant(ctx->range_partition_expr()->MAXVALUE()->getText()));
    } else {
      for (const auto& range_ctx :
           ctx->range_partition_expr()->range_expr_list()->range_expr()) {
        end.push_back(Util::GetValueString(range_ctx));
      }
    }
    res.push_back(std::make_shared<RangePartitionSlice>(
        partition_name, nullptr, false,
        std::make_shared<std::vector<RawConstant>>(end), false,
        RangePartitionSliceExprIdentifier::LESS_THAN,
        std::make_shared<std::vector<std::shared_ptr<PartitionSlice>>>(
            partition_slices),
        std::make_shared<Strings>(options), seq));
  }
  return res;
}

std::vector<std::shared_ptr<PartitionSlice>>
OBMySQLObjectParser::ProcessListPartitionList(
    OBParser::List_partition_listContext* c) {
  std::vector<std::shared_ptr<PartitionSlice>> res;
  for (const auto& ctx : c->list_partition_element()) {
    RawConstant partition_name =
        ctx->relation_factor() != nullptr
            ? HandleIdentifierName(ctx->relation_factor()->getText())
            : RawConstant();
    int seq = -1;
    Strings options;
    if (ctx->INTNUM() != nullptr) {
      seq = std::stoi(ctx->INTNUM()->getText());
    }
    std::vector<std::shared_ptr<PartitionSlice>> partition_slices;
    if (ctx->opt_hash_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptHashSubpartitionList(ctx->opt_hash_subpartition_list());
    } else if (ctx->opt_list_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptListSubpartitionList(ctx->opt_list_subpartition_list());
    } else if (ctx->opt_range_subpartition_list() != nullptr) {
      partition_slices =
          ProcessOptRangeSubpartitionList(ctx->opt_range_subpartition_list());
    }
    std::vector<RawConstant> values;
    if (ctx->list_partition_expr()->DEFAULT() != nullptr) {
      values.push_back(RawConstant(
          ctx->list_partition_expr()->DEFAULT()->getSymbol()->getText()));
    } else {
      for (const auto& expr_ctx :
           ctx->list_partition_expr()->list_expr()->expr()) {
        values.push_back(Util::GetValueString(expr_ctx));
      }
    }
    res.push_back(std::make_shared<ListPartitionSlice>(
        partition_name,
        std::make_shared<std::vector<std::shared_ptr<PartitionSlice>>>(
            partition_slices),
        std::make_shared<Strings>(options),
        std::make_shared<std::vector<RawConstant>>(values), seq));
  }
  return res;
}

std::shared_ptr<GeneralPartitionInfo>
OBMySQLObjectParser::ProcessSubpartitionOption(
    OBParser::Subpartition_optionContext* c) {
  PartitionType partition_type = PartitionType::NONE;
  std::vector<RawConstant> sub_partition_names;
  std::string expr;
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> expr_tokens =
      std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
  std::vector<std::shared_ptr<PartitionSlice>> options;
  int partition_num = -1;
  bool contain_columns_key_word = false;
  if (c->subpartition_template_option() != nullptr) {
    OBParser::Subpartition_template_optionContext*
        subpartition_template_option_context =
            c->subpartition_template_option();
    if (nullptr != subpartition_template_option_context->COLUMNS()) {
      contain_columns_key_word = true;
    }
    if (subpartition_template_option_context->BY() != nullptr) {
      if (subpartition_template_option_context->HASH() != nullptr) {
        partition_type = PartitionType::HASH;
        options = ProcessOptHashSubpartitionList(
            subpartition_template_option_context->opt_hash_subpartition_list());
      } else if (subpartition_template_option_context->BISON_LIST() !=
                 nullptr) {
        partition_type = PartitionType::LIST;
        options = ProcessOptListSubpartitionList(
            subpartition_template_option_context->opt_list_subpartition_list());
      } else if (subpartition_template_option_context->RANGE() != nullptr) {
        partition_type = PartitionType::RANGE;
        options = ProcessOptRangeSubpartitionList(
            subpartition_template_option_context
                ->opt_range_subpartition_list());
      } else if (subpartition_template_option_context->KEY() != nullptr) {
        partition_type = PartitionType::KEY;
        options = ProcessOptHashSubpartitionList(
            subpartition_template_option_context->opt_hash_subpartition_list());
      }
      if (c->subpartition_template_option()->expr() != nullptr) {
        expr = Util::GetCtxString(c->subpartition_template_option()->expr());
        AnalysisTokens<OBParser::Column_nameContext>(
            c->subpartition_template_option()->expr(), expr_tokens);
        if (expr_tokens->size() == 1 && expr_tokens->at(0)->GetTokenType() ==
                                            ExprTokenType::IDENTIFIER_NAME) {
          sub_partition_names.push_back(HandleIdentifierName(expr));
        }
      } else {
        sub_partition_names = RetrieveColumnLists(
            subpartition_template_option_context->column_name_list());
      }
    } else {
      return nullptr;
    }
  } else {
    OBParser::Subpartition_individual_optionContext* si =
        c->subpartition_individual_option();
    if (nullptr != si->COLUMNS()) {
      contain_columns_key_word = true;
    }
    if (si->BY() != nullptr) {
      if (si->INTNUM() != nullptr) {
        partition_num = std::stoi(si->INTNUM()->getText());
      }
      if (si->HASH() != nullptr) {
        partition_type = PartitionType::HASH;
      } else if (si->BISON_LIST() != nullptr) {
        partition_type = PartitionType::LIST;
      } else if (si->RANGE() != nullptr) {
        partition_type = PartitionType::RANGE;
      } else {
        partition_type = PartitionType::KEY;
      }
      if (c->subpartition_individual_option()->expr() != nullptr) {
        expr = Util::GetCtxString(c->subpartition_individual_option()->expr());
        AnalysisTokens<OBParser::Column_nameContext>(
            c->subpartition_individual_option()->expr(), expr_tokens);
        if (expr_tokens->size() == 1 && expr_tokens->at(0)->GetTokenType() ==
                                            ExprTokenType::IDENTIFIER_NAME) {
          sub_partition_names.push_back(HandleIdentifierName(expr));
        }
      } else {
        sub_partition_names = RetrieveColumnLists(si->column_name_list());
      }
    }
  }
  return std::make_shared<GeneralPartitionInfo>(
      sub_partition_names.empty()
          ? std::make_shared<PartitionValueSourceInfo>(expr, expr_tokens)
          : std::make_shared<PartitionValueSourceInfo>(sub_partition_names),
      partition_type, partition_num, nullptr,
      std::make_shared<std::vector<std::shared_ptr<PartitionSlice>>>(options),
      contain_columns_key_word);
}

std::vector<std::shared_ptr<PartitionSlice>>
OBMySQLObjectParser::ProcessOptHashSubpartitionList(
    OBParser::Opt_hash_subpartition_listContext* c) {
  std::vector<std::shared_ptr<PartitionSlice>> res;
  OBParser::Hash_subpartition_listContext* hs = c->hash_subpartition_list();
  for (const auto& ctx : hs->hash_subpartition_element()) {
    RawConstant partition_name =
        ctx->relation_factor() == nullptr
            ? RawConstant()
            : HandleIdentifierName(Util::GetCtxString(ctx->relation_factor()));
    std::shared_ptr<Strings> option_infos = std::make_shared<Strings>();
    if (nullptr != ctx->ENGINE_()) {
      option_infos->push_back(ctx->ENGINE_()->getText() + " " +
                              ctx->COMP_EQ()->getText() + " " +
                              ctx->INNODB()->getText());
    }
    res.push_back(
        std::make_shared<HashPartitionSlice>(partition_name, option_infos, -1));
  }
  return res;
}

std::vector<std::shared_ptr<PartitionSlice>>
OBMySQLObjectParser::ProcessOptListSubpartitionList(
    OBParser::Opt_list_subpartition_listContext* c) {
  std::vector<std::shared_ptr<PartitionSlice>> res;
  for (const auto& ctx :
       c->list_subpartition_list()->list_subpartition_element()) {
    RawConstant partition_name =
        ctx->relation_factor() == nullptr
            ? RawConstant()
            : HandleIdentifierName(ctx->relation_factor()->getText());

    std::vector<RawConstant> values;
    if (ctx->list_partition_expr()->DEFAULT() != nullptr) {
      values.push_back(RawConstant(
          ctx->list_partition_expr()->DEFAULT()->getSymbol()->getText()));
    } else {
      for (const auto& expr_ctx :
           ctx->list_partition_expr()->list_expr()->expr()) {
        values.push_back(Util::GetCtxString(expr_ctx));
      }
    }
    res.push_back(std::make_shared<ListPartitionSlice>(
        partition_name, nullptr,
        std::make_shared<std::vector<RawConstant>>(values)));
  }
  return res;
}

std::vector<std::shared_ptr<PartitionSlice>>
OBMySQLObjectParser::ProcessOptRangeSubpartitionList(
    OBParser::Opt_range_subpartition_listContext* c) {
  std::vector<std::shared_ptr<PartitionSlice>> res;
  for (const auto& ctx :
       c->range_subpartition_list()->range_subpartition_element()) {
    RawConstant partition_name =
        ctx->relation_factor() == nullptr
            ? RawConstant()
            : HandleIdentifierName(Util::GetCtxString(ctx->relation_factor()));
    std::shared_ptr<std::vector<RawConstant>> end =
        std::make_shared<std::vector<RawConstant>>();
    if (nullptr != ctx->range_partition_expr()->MAXVALUE()) {
      end->push_back(
          RawConstant(ctx->range_partition_expr()->MAXVALUE()->getText()));
    } else {
      for (const auto& expr_ctx :
           ctx->range_partition_expr()->range_expr_list()->range_expr()) {
        end->push_back(Util::GetValueString(expr_ctx));
      }
    }
    res.push_back(std::make_shared<RangePartitionSlice>(
        partition_name, RangePartitionSliceExprIdentifier::LESS_THAN, nullptr,
        false, end, false, nullptr));
  }
  return res;
}

void OBMySQLObjectParser::exitCreate_table_stmt(
    OBParser::Create_table_stmtContext*) {
  if (!anonymous_primary_key_columns.empty()) {
    std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> columns =
        std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
    for (const auto& pk_column : anonymous_primary_key_columns) {
      columns->push_back(std::make_shared<ExprColumnRef>(pk_column));
    }
    actions.push_back(std::make_shared<TableConstraintObject>(
        catalog, table_name, "", RawConstant(), IndexType::PRIMARY, nullptr,
        columns));
  }
  int un_index = 1;
  for (const auto uk_column : anonymous_unique_key_columns) {
    std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> columns =
        std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
    columns->push_back(std::make_shared<ExprColumnRef>(uk_column));
    actions.push_back(std::make_shared<TableConstraintObject>(
        catalog, table_name, "",
        RawConstant("GENE_" + std::to_string(un_index++)), IndexType::UNIQUE,
        nullptr, columns));
  }

  std::shared_ptr<TableConstraintObject> pk = nullptr;
  for (const auto& action : actions) {
    if (ObjectType::TABLE_CONSTRAINT_OBJECT == action->GetObjectType()) {
      auto constraint_object =
          std::dynamic_pointer_cast<TableConstraintObject>(action);
      if (constraint_object->GetIndexType() == IndexType::PRIMARY) {
        pk = constraint_object;
      }
    }
  }
  if (pk != nullptr) {
    const auto& affected_columns = *pk->GetRawAffectedColumns();
    for (const auto& action : actions) {
      if (ObjectType::TABLE_COLUMN_OBJECT == action->GetObjectType()) {
        auto column_object =
            std::dynamic_pointer_cast<TableColumnObject>(action);
        for (const auto column : affected_columns) {
          if ((ddl_parse_context->is_case_sensitive &&
               column->token.value ==
                   column_object->GetRawColumnNameToAdd().value) ||
              (!ddl_parse_context->is_case_sensitive &&
               Util::EqualsIgnoreCase(
                   column->token.value,
                   column_object->GetRawColumnNameToAdd().value))) {
            column_object->GetColumnAttributes()->SetIsNullable(false);
          }
        }
      }
    }
  }
}
// drop table
void OBMySQLObjectParser::enterDrop_table_stmt(
    OBParser::Drop_table_stmtContext* ctx) {
  std::vector<std::pair<Catalog, common::RawConstant>> catalog_and_tables;
  auto relations = ctx->table_list()->relation_factor();
  for (const auto& relation : relations) {
    auto table = RetrieveSchemaAndTableName(relation);
    catalog_and_tables.push_back(table);
  }
  parent_object = std::make_shared<DropTableObject>(
      catalog_and_tables, Util::GetCtxString(ctx), ctx->TEMPORARY() != nullptr,
      ctx->EXISTS() != nullptr);
}

void OBMySQLObjectParser::enterTruncate_table_stmt(
    OBParser::Truncate_table_stmtContext* ctx) {
  auto db_and_table = RetrieveSchemaAndTableName(ctx->relation_factor());
  parent_object = std::make_shared<TruncateTableObject>(
      db_and_table, Util::GetCtxString(ctx));
}

void OBMySQLObjectParser::exitTruncate_table_stmt(
    OBParser::Truncate_table_stmtContext*) {}

std::pair<Catalog, RawConstant> OBMySQLObjectParser::RetrieveSchemaAndTableName(
    OBParser::Relation_factorContext* relation_factor_context) {
  if (relation_factor_context->dot_relation_factor() != nullptr) {
    std::string table_name;
    if (relation_factor_context->dot_relation_factor()->relation_name() !=
        nullptr) {
      table_name = Util::GetCtxString(
          relation_factor_context->dot_relation_factor()->relation_name());
    } else {
      table_name =
          Util::GetCtxString(relation_factor_context->dot_relation_factor()
                                 ->mysql_reserved_keyword());
    }
    return std::make_pair(RawConstant(ddl_parse_context->default_schema),
                          ProcessTableOrDbName(table_name, ddl_parse_context));
  }
  return RetrieveSchemaAndTableName(
      relation_factor_context->normal_relation_factor());
}
std::pair<Catalog, RawConstant> OBMySQLObjectParser::RetrieveSchemaAndTableName(
    OBParser::Normal_relation_factorContext* ctx) {
  if (ctx->Dot() != nullptr) {
    return std::make_pair(
        ProcessTableOrDbName(Util::GetCtxString(ctx->relation_name(0)),
                             ddl_parse_context),
        ctx->mysql_reserved_keyword() == nullptr
            ? ProcessTableOrDbName(Util::GetCtxString(ctx->relation_name(1)),
                                   ddl_parse_context)
            : ProcessTableOrDbName(
                  Util::GetCtxString(ctx->mysql_reserved_keyword()),
                  ddl_parse_context));
  }
  return std::make_pair(
      RawConstant(ddl_parse_context->default_schema),
      ProcessTableOrDbName(Util::GetCtxString(ctx->relation_name(0)),
                           ddl_parse_context));
}

RawConstant OBMySQLObjectParser::ProcessTableOrDbName(
    const std::string& str, std::shared_ptr<ParseContext> ddl_parse_context) {
  if (ddl_parse_context->is_case_sensitive) {
    // if source is case sensitive,
    // If the table name has an escape character, keep it as it is
    // if the table name does not have an escape character, it is converted to
    // lowercase
    return ProcessMySqlEscapeTableOrDbName(
        str, ddl_parse_context->escape_char,
        Util::IsEscapeString(str, ddl_parse_context->escape_char) ? false
                                                                  : true);
  }
  return ProcessMySqlEscapeTableOrDbName(str, ddl_parse_context->escape_char,
                                         true);
}
RawConstant OBMySQLObjectParser::ProcessMySqlEscapeTableOrDbName(
    const std::string& name, const std::string& escape_string, bool low_case) {
  if (Util::IsEscapeString(name, escape_string)) {
    return RawConstant(true, name.substr(1, name.length() - 2),
                       low_case
                           ? Util::LowerCase(name.substr(1, name.length() - 2))
                           : name.substr(1, name.length() - 2));
  }
  return RawConstant(false, name, low_case ? Util::LowerCase(name) : name);
}

void OBMySQLObjectParser::exitDrop_table_stmt(
    OBParser::Drop_table_stmtContext*) {}
void OBMySQLObjectParser::exitSql_stmt(OBParser::Sql_stmtContext* context) {
  if (parent_object != nullptr &&
      parent_object->GetObjectType() == ObjectType::ALTER_TABLE_OBJECT) {
    ObjectPtrs added_coments;
    for (const auto& object : comment_list_) {
      bool add_comment = true;
      std::shared_ptr<CommentObject> comment_object =
          std::dynamic_pointer_cast<CommentObject>(object);
      for (const auto object : actions) {
        if (std::shared_ptr<AlterTableColumnObject> alter_table_column_object =
                std::dynamic_pointer_cast<AlterTableColumnObject>(object)) {
          if (Util::RawConstantValue(
                  comment_object->GetCommentPair().comment_target) ==
              (alter_table_column_object->GetModifiedColumnDefinition()
                   ->GetColumnNameToAdd())) {
            ObjectPtrs tmp;
            tmp.push_back(comment_object);
            alter_table_column_object->SetSubObjects(tmp);
            add_comment = false;
            break;
          }
        }
      }
      if (add_comment) added_coments.push_back(comment_object);
    }
    actions.insert(actions.end(), added_coments.begin(), added_coments.end());

  } else {
    actions.insert(actions.end(), comment_list_.begin(), comment_list_.end());
  }
  if (!actions.empty()) {
    parent_object->SetSubObjects(actions);
  }
}
void OBMySQLObjectParser::enterAlter_table_stmt(
    OBParser::Alter_table_stmtContext* context) {
  auto db_and_table_name =
      RetrieveSchemaAndTableName(context->relation_factor());
  catalog = db_and_table_name.first;
  table_name = db_and_table_name.second;

  // Only support alter table add/modify column now.
  parent_object = std::make_shared<AlterTableObject>(
      catalog, table_name, ddl_parse_context->raw_ddl);
  s_action = ObjectType::ALTER_TABLE_OBJECT;

  ProcessAlterTableActions(context->alter_table_actions());
}
void OBMySQLObjectParser::ProcessAlterTableActions(
    OBParser::Alter_table_actionsContext* ctx) {
  if (ctx->alter_table_actions() != nullptr) {
    ProcessAlterTableActions(ctx->alter_table_actions());
  }
  if (ctx->alter_table_action() != nullptr) {
    ProcessAlterTableAction(ctx->alter_table_action());
  }
}
void OBMySQLObjectParser::ProcessAlterTableAction(
    OBParser::Alter_table_actionContext* ctx) {
  if (ctx->alter_column_option() != nullptr) {
    ProcessAlterTableAlterColumnAction(ctx->alter_column_option());
  } else if (ctx->RENAME() != nullptr) {
    auto target_schema_info =
        RetrieveSchemaAndTableName(ctx->relation_factor());
    std::vector<RenameTableObject::RenamePair> rename_pairs;
    rename_pairs.push_back(RenameTableObject::RenamePair(
        catalog, table_name, Catalog(target_schema_info.first),
        target_schema_info.second));
    actions.push_back(std::make_shared<RenameTableObject>(
        rename_pairs, catalog, table_name, Util::GetCtxString(ctx)));
  } else if (ctx->alter_index_option() != nullptr) {
    ProcessAlterTableIndex(ctx->alter_index_option());
  } else if (ctx->alter_constraint_option() != nullptr) {
    ProcessAlterTableConstraint(ctx->alter_constraint_option());
  } else if (ctx->alter_foreign_key_action() != nullptr) {
    ProcessAlterForeignKey(ctx->alter_foreign_key_action());
  } else if (ctx->alter_partition_option() != nullptr) {
    ProcessAlterPartition(ctx->alter_partition_option());
  } else if (ctx->table_option_list_space_seperated() != nullptr) {
    // alter table add/remove comment
    ProcessAlterTableOption(ctx->table_option_list_space_seperated());
  } else {
    ddl_parse_context->SetErrMsg("unsupported ALTER TABLE statement");
  }
}

void OBMySQLObjectParser::ProcessAlterForeignKey(
    OBParser::Alter_foreign_key_actionContext* ctx) {
  if (ctx->DROP() != nullptr) {
    RawConstant index_name =
        HandleIdentifierName(Util::GetCtxString(ctx->index_name()));
    actions.push_back(std::make_shared<DropTableConstraintObject>(
        catalog, table_name, Util::GetCtxString(ctx), IndexType::FOREIGN,
        index_name));
  } else if (ctx->ADD() != nullptr) {
    RawConstant index_name;
    if (ctx->index_name() != nullptr) {
      index_name = HandleIdentifierName(Util::GetCtxString(ctx->index_name()));
    }
    if (ctx->constraint_name() != nullptr &&
        ctx->constraint_name() != nullptr) {
      index_name = HandleIdentifierName(
          Util::GetCtxString(ctx->constraint_name()->relation_name()));
    }
    auto columns = RetrieveColumnLists(ctx->column_name_list().at(0));
    auto parent_table_info = RetrieveSchemaAndTableName(ctx->relation_factor());
    auto parents_columns = RetrieveColumnLists(ctx->column_name_list().at(1));
    auto cascade_options = std::make_shared<std::vector<
        TableReferenceConstraintObject::FKReferenceOperationCascade>>();
    if (nullptr != ctx->reference_option()) {
      cascade_options->push_back(ResolveReferenceType(ctx->reference_option()));
      RetrieveReferenceOption(ctx->opt_reference_option_list(),
                              cascade_options);
    }
    actions.push_back(std::make_shared<TableReferenceConstraintObject>(
        catalog, table_name, Util::GetCtxString(ctx), index_name,
        IndexType::FOREIGN, std::make_shared<std::vector<RawConstant>>(columns),
        TableReferenceConstraintObject::FKReferenceRange::PART_OF_TABLE_COLUMNS,
        parent_table_info.first, parent_table_info.second,
        std::make_shared<std::vector<RawConstant>>(parents_columns),
        cascade_options));
  }
}
TableReferenceConstraintObject::FKReferenceOperationCascade
OBMySQLObjectParser::ResolveReferenceType(
    OBParser::Reference_optionContext* ctx) {
  std::string action = Util::GetCtxString(ctx->reference_action());
  if (ctx->DELETE() != nullptr) {
    if (Util::EqualsIgnoreCase(action, "RESTRICT")) {
      return TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_RESTRICT;
    } else if (Util::EqualsIgnoreCase(action, "CASCADE")) {
      return TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_CASCADE;
    } else if (Util::ContainsIgnoreCase(action, "NULL")) {
      return TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_SET_NULL;
    } else if (Util::ContainsIgnoreCase(action, "DEFAULT")) {
      return TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_SET_DEFAULT;
    } else {
      return TableReferenceConstraintObject::FKReferenceOperationCascade::
          ON_DELETE_NO_ACTION;
    }
  }
  if (Util::EqualsIgnoreCase(action, "RESTRICT")) {
    return TableReferenceConstraintObject::FKReferenceOperationCascade::
        ON_UPDATE_RESTRICT;
  } else if (Util::ContainsIgnoreCase(action, "DEFAULT")) {
    return TableReferenceConstraintObject::FKReferenceOperationCascade::
        ON_UPDATE_DEFAULT;
  } else if (Util::ContainsIgnoreCase(action, "ACTION")) {
    return TableReferenceConstraintObject::FKReferenceOperationCascade::
        ON_UPDATE_NO_ACTION;
  } else if (Util::ContainsIgnoreCase(action, "CASCADE")) {
    return TableReferenceConstraintObject::FKReferenceOperationCascade::
        ON_UPDATE_CASCADE;
  } else if (Util::ContainsIgnoreCase(action, "NULL")) {
    return TableReferenceConstraintObject::FKReferenceOperationCascade::
        ON_UPDATE_SET_NULL;
  }
  return TableReferenceConstraintObject::FKReferenceOperationCascade::
      NOT_DEFINED;
}
void OBMySQLObjectParser::RetrieveReferenceOption(
    OBParser::Opt_reference_option_listContext* ctx,
    std::shared_ptr<std::vector<
        TableReferenceConstraintObject::FKReferenceOperationCascade>>
        sink) {
  if (ctx->reference_option() != nullptr) {
    sink->push_back(ResolveReferenceType(ctx->reference_option()));
    RetrieveReferenceOption(ctx->opt_reference_option_list(), sink);
  }
}

void OBMySQLObjectParser::ProcessAlterPartition(
    OBParser::Alter_partition_optionContext* ctx) {
  if (ctx->DROP() != nullptr) {
    std::shared_ptr<std::vector<RawConstant>> partition_to_drop =
        std::make_shared<std::vector<RawConstant>>();
    RetrieveNameList(ctx->drop_partition_name_list()->name_list(),
                     partition_to_drop);

    int partition_level = ctx->PARTITION() != nullptr
                              ? PartitionObject::partition_level
                              : PartitionObject::sub_partition_level;
    actions.push_back(std::make_shared<DropTablePartitionObject>(
        catalog, table_name, partition_to_drop, partition_level));
  } else if (ctx->ADD() != nullptr) {
    ProcessOptPartitionRangeOrList(ctx->opt_partition_range_or_list());
  } else if (ctx->TRUNCATE() != nullptr) {
    std::shared_ptr<std::vector<RawConstant>> partition_to_truncate =
        std::make_shared<std::vector<RawConstant>>();
    RetrieveNameList(ctx->name_list(), partition_to_truncate);
    int partition_level = ctx->PARTITION() != nullptr
                              ? PartitionObject::partition_level
                              : PartitionObject::sub_partition_level;
    actions.push_back(std::make_shared<TruncateTablePartitionObject>(
        catalog, table_name, partition_to_truncate, partition_level, false));
  } else if (ctx->modify_partition_info() != nullptr) {
    std::shared_ptr<TablePartitionObject> table_partition_object =
        ProcessPartitionOption(
            ctx->modify_partition_info()->partition_option());
    if (nullptr != table_partition_object) {
      actions.push_back(std::make_shared<AlterTablePartitionObject>(
          catalog, table_name, Util::GetCtxString(ctx), table_partition_object,
          AlterTablePartitionObject::AlterPartitionType::REDEFINE_PARTITION));
    }
  }
}

void OBMySQLObjectParser::ProcessAlterTableConstraint(
    OBParser::Alter_constraint_optionContext* ctx) {
  if (nullptr != ctx->constraint_definition() &&
      nullptr != ctx->constraint_definition()->expr()) {
    RawConstant check_name = HandleIdentifierName(
        Util::GetCtxString(ctx->constraint_definition()->constraint_name()));
    std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> expr_tokens =
        std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
    AnalysisTokens<OBParser::Column_nameContext>(
        ctx->constraint_definition()->expr(), expr_tokens);
    actions.push_back(std::make_shared<TableCheckConstraint>(
        catalog, table_name, check_name,
        Util::GetCtxString(ctx->constraint_definition()->expr()), expr_tokens));
  } else {
    ddl_parse_context->SetErrMsg("unsupported drop constraint");
  }
}

void OBMySQLObjectParser::ProcessAlterTableIndex(
    OBParser::Alter_index_optionContext* ctx) {
  if (ctx->ADD() != nullptr) {
    RawConstant index_name;
    if (!ctx->index_name().empty() && ctx->index_name().size() >= 1) {
      index_name =
          HandleIdentifierName(Util::GetCtxString(ctx->index_name().at(0)));
    }
    if (ctx->constraint_name() != nullptr) {
      index_name =
          HandleIdentifierName(Util::GetCtxString(ctx->constraint_name()));
    }
    if (ctx->PRIMARY() != nullptr) {
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> pk_names =
          std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
      auto columns = RetrieveColumnLists(ctx->column_name_list());
      for (const auto column : columns) {
        pk_names->push_back(std::make_shared<ExprColumnRef>(column));
      }
      actions.push_back(std::make_shared<TableConstraintObject>(
          catalog, table_name, Util::GetCtxString(ctx), index_name,
          IndexType::PRIMARY, nullptr, pk_names));
    } else {
      auto column_info_map = RetrieveKeyInfo(ctx->sort_column_list());
      std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> key_names =
          std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
      for (const auto column : ctx->sort_column_list()->sort_column_key()) {
        key_names->push_back(std::make_shared<ExprColumnRef>(
            ProcessColumnName(Util::GetCtxString(column->column_name()))));
      }
      IndexType index_type = IndexType::NORMAL;
      if (ctx->UNIQUE() != nullptr) {
        index_type = IndexType::UNIQUE;
      } else if (ctx->FULLTEXT() != nullptr) {
        index_type = IndexType::FULLTEXT;
      }
      actions.push_back(std::make_shared<TableConstraintObject>(
          catalog, table_name, Util::GetCtxString(ctx), index_name, index_type,
          column_info_map, key_names));
    }
  } else if (ctx->DROP() != nullptr) {
    if (nullptr != ctx->PRIMARY()) {
      actions.push_back(std::make_shared<DropTableConstraintObject>(
          catalog, table_name, Util::GetCtxString(ctx), IndexType::PRIMARY,
          RawConstant()));
    } else {
      RawConstant index_name =
          HandleIdentifierName(Util::GetCtxString(ctx->index_name().at(0)));
      actions.push_back(std::make_shared<DropIndexObject>(
          catalog, table_name, index_name, Util::GetCtxString(ctx)));
    }
  } else if (ctx->RENAME() != nullptr) {
    RawConstant origin_index =
        HandleIdentifierName(Util::GetCtxString(ctx->index_name().at(0)));
    RawConstant current_index_name =
        HandleIdentifierName(Util::GetCtxString(ctx->index_name().at(1)));
    actions.push_back(std::make_shared<RenameIndexObject>(
        catalog, table_name, Util::GetCtxString(ctx), origin_index,
        current_index_name));
  }
}

void OBMySQLObjectParser::ProcessAlterTableOption(
    OBParser::Table_option_list_space_seperatedContext* ctx) {
  if (nullptr != ctx->table_option()) {
    OBParser::Table_optionContext* option_context = ctx->table_option();
    if (option_context->COMMENT() != nullptr) {
      comment_list_.push_back(std::make_shared<CommentObject>(
          catalog, table_name, Util::GetCtxString(ctx),
          CommentObject::CommentTargetType::TABLE,
          CommentObject::CommentPair(
              table_name, option_context->STRING_VALUE()->getText())));
    }
  }
  if (nullptr != ctx->table_option_list_space_seperated()) {
    ProcessAlterTableOption(ctx->table_option_list_space_seperated());
  }
}
void OBMySQLObjectParser::ProcessAlterTableAlterColumnAction(
    OBParser::Alter_column_optionContext* ctx) {
  if (ctx == nullptr) {
    return;
  }
  std::shared_ptr<std::vector<AlterTableColumnObject::AlterColumnType>>
      alter_column_types = std::make_shared<
          std::vector<AlterTableColumnObject::AlterColumnType>>();
  if (ctx->ADD() != nullptr) {
    if (ctx->column_definition() != nullptr) {
      actions.push_back(ProcessColumnDefinition(ctx->column_definition()));
    } else {
      for (const auto& column :
           ctx->column_definition_list()->column_definition()) {
        actions.push_back(ProcessColumnDefinition(column));
      }
    }
  } else if (ctx->DROP() != nullptr) {
    RawConstant column_name_to_drop =
        GetColumnName(ctx->column_definition_ref());
    Strings drop_option;
    if (nullptr != ctx->CASCADE()) {
      drop_option.push_back(ctx->CASCADE()->getText());
    }
    if (nullptr != ctx->RESTRICT()) {
      drop_option.push_back(ctx->RESTRICT()->getText());
    }
    actions.push_back(std::make_shared<DropTableColumnObject>(
        catalog, table_name, Util::GetCtxString(ctx), column_name_to_drop,
        drop_option));
  } else if (ctx->ALTER() != nullptr) {
    RawConstant column_name_to_drop =
        GetColumnName(ctx->column_definition_ref());
    if (ctx->alter_column_behavior() != nullptr) {
      std::shared_ptr<ColumnAttributes> attributes =
          std::make_shared<ColumnAttributes>();
      if (ctx->alter_column_behavior()->DROP() != nullptr) {
        alter_column_types->push_back(
            AlterTableColumnObject::AlterColumnType::DROP_DEFAULT);
        std::shared_ptr<TableColumnObject> tmp =
            std::make_shared<TableColumnObject>(
                catalog, table_name, Util::GetCtxString(ctx),
                column_name_to_drop, RealDataType::UNSUPPORTED, nullptr,
                attributes,
                std::make_shared<ColumnLocationIdentifier>(
                    ColumnLocationIdentifier::ColPositionLocator::TAIL));
        actions.push_back(std::make_shared<AlterTableColumnObject>(
            catalog, table_name, Util::GetCtxString(ctx), column_name_to_drop,
            tmp, alter_column_types));
      } else {
        alter_column_types->push_back(
            AlterTableColumnObject::AlterColumnType::CHANGE_DEFAULT);
        attributes->SetDefaultValue(expr_parser.AnalysisSignedLiteral(
            ctx->alter_column_behavior()->signed_literal()));
        std::shared_ptr<TableColumnObject> tmp =
            std::make_shared<TableColumnObject>(
                catalog, table_name, Util::GetCtxString(ctx),
                column_name_to_drop, RealDataType::UNSUPPORTED, nullptr,
                attributes,
                std::make_shared<ColumnLocationIdentifier>(
                    ColumnLocationIdentifier::ColPositionLocator::TAIL));
        actions.push_back(std::make_shared<AlterTableColumnObject>(
            catalog, table_name, Util::GetCtxString(ctx), column_name_to_drop,
            tmp, alter_column_types));
      }
    }
  } else if (ctx->MODIFY() != nullptr) {
    alter_column_types->push_back(
        AlterTableColumnObject::AlterColumnType::CHANGE_DATA_TYPE);
    auto table_column_object =
        ProcessColumnDefinition(ctx->column_definition(), alter_column_types);
    actions.push_back(std::make_shared<AlterTableColumnObject>(
        catalog, table_name, Util::GetCtxString(ctx),
        table_column_object->GetRawColumnNameToAdd(), table_column_object,
        alter_column_types));
  } else if (ctx->CHANGE() != nullptr) {
    alter_column_types->push_back(
        AlterTableColumnObject::AlterColumnType::REDEFINE_COLUMN);
    auto table_column_object =
        ProcessColumnDefinition(ctx->column_definition(), alter_column_types);
    actions.push_back(std::make_shared<AlterTableColumnObject>(
        catalog, table_name, Util::GetCtxString(ctx),
        GetColumnName(ctx->column_definition_ref()), table_column_object,
        alter_column_types));
  } else {
    ddl_parse_context->SetErrMsg("unsupported " + Util::GetCtxString(ctx));
  }
}

void OBMySQLObjectParser::RetrieveNameList(
    OBParser::Name_listContext* ctx,
    std::shared_ptr<std::vector<RawConstant>> sink) {
  if (ctx->name_list() != nullptr) {
    RetrieveNameList(ctx->name_list(), sink);
  }
  if (ctx->NAME_OB() != nullptr) {
    sink->push_back(HandleIdentifierName(ctx->NAME_OB()->getText()));
  }
}
void OBMySQLObjectParser::ProcessOptPartitionRangeOrList(
    OBParser::Opt_partition_range_or_listContext* ctx) {
  std::vector<std::shared_ptr<PartitionSlice>> partition_slices;
  if (ctx->opt_range_partition_list() != nullptr) {
    partition_slices = ProcessRangePartitionList(
        ctx->opt_range_partition_list()->range_partition_list());
  } else if (ctx->opt_list_partition_list() != nullptr) {
    partition_slices = ProcessListPartitionList(
        ctx->opt_list_partition_list()->list_partition_list());
  }
  if (!partition_slices.empty()) {
    auto ret = std::make_shared<TablePartitionObject>(
        catalog, table_name, Util::GetCtxString(ctx), nullptr);
    for (const auto slice : partition_slices) {
      ret->AddPartitionSlice(slice);
    }
    actions.push_back(ret);
  }
}

void OBMySQLObjectParser::enterRename_table_stmt(
    OBParser::Rename_table_stmtContext* ctx) {
  std::vector<RenameTableObject::RenamePair> rename_pairs;
  for (OBParser::Rename_table_actionContext* act :
       ctx->rename_table_actions()->rename_table_action()) {
    std::pair<Catalog, RawConstant> source_schema_info =
        RetrieveSchemaAndTableName(act->relation_factor().at(0));
    std::pair<Catalog, RawConstant> dest_schema_info =
        RetrieveSchemaAndTableName(act->relation_factor().at(1));
    Catalog source = source_schema_info.first;
    Catalog dest = dest_schema_info.first;
    RawConstant source_table = source_schema_info.second;
    RawConstant dest_table = dest_schema_info.second;
    rename_pairs.push_back(
        RenameTableObject::RenamePair(source, source_table, dest, dest_table));
  }
  parent_object = std::make_shared<RenameTableObject>(rename_pairs,
                                                      Util::GetCtxString(ctx));
}

void OBMySQLObjectParser::enterCreate_index_stmt(
    OBParser::Create_index_stmtContext* ctx) {
  auto db_and_table = RetrieveSchemaAndTableName(ctx->relation_factor());
  catalog = db_and_table.first;
  table_name = db_and_table.second;
  bool is_not_exist = ctx->EXISTS() != nullptr;
  IndexType index_type = IndexType::NORMAL;
  if (ctx->UNIQUE() != nullptr) {
    index_type = IndexType::UNIQUE;
  } else if (ctx->FULLTEXT() != nullptr) {
    index_type = IndexType::FULLTEXT;
  } else if (ctx->SPATIAL() != nullptr) {
    index_type = IndexType::SPATIAL;
  }
  auto schema_and_index =
      RetrieveSchemaAndTableName(ctx->normal_relation_factor());
  RawConstant index_name = schema_and_index.second;
  Catalog index_schema;
  if ("" != schema_and_index.first.GetCatalogName()) {
    index_schema = Catalog(schema_and_index.first);
  }
  auto column_info_map = RetrieveKeyInfo(ctx->sort_column_list());
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> affect_columns =
      std::make_shared<std::vector<std::shared_ptr<ExprToken>>>();
  const auto& column_keys = ctx->sort_column_list()->sort_column_key();
  std::shared_ptr<std::vector<RawConstant>> length_of_pre_index_list =
      std::make_shared<std::vector<RawConstant>>();
  for (const auto& column_key : column_keys) {
    affect_columns->push_back(std::make_shared<ExprColumnRef>(
        ProcessColumnName(Util::GetCtxString(column_key->column_name()))));
    if (column_key->LeftParen() != nullptr) {
      length_of_pre_index_list->push_back(
          RawConstant(column_key->LeftParen()->getText() +
                      column_key->INTNUM(0)->getText() +
                      column_key->RightParen()->getText()));
    } else {
      length_of_pre_index_list->push_back(RawConstant());
    }
  }
  RetrieveComment(ctx->opt_index_options(), index_name.origin_string);
  parent_object = std::make_shared<TableConstraintObject>(
      catalog, table_name, Util::GetCtxString(ctx), index_name, index_type,
      column_info_map, affect_columns, length_of_pre_index_list);
}
void OBMySQLObjectParser::RetrieveComment(
    OBParser::Opt_index_optionsContext* ctx, const std::string& index_name) {
  if (ctx == nullptr) {
    return;
  }
  for (auto option : ctx->index_option()) {
    if (option->COMMENT() != nullptr) {
      comment_list_.push_back(std::make_shared<CommentObject>(
          catalog, table_name, Util::GetCtxString(option),
          CommentObject::CommentTargetType::CONSTRAINT,
          CommentObject::CommentPair(index_name,
                                     option->STRING_VALUE()->getText())));
    }
  }
}
void OBMySQLObjectParser::enterDrop_index_stmt(
    OBParser::Drop_index_stmtContext* ctx) {
  std::pair<Catalog, RawConstant> db_and_table =
      RetrieveSchemaAndTableName(ctx->relation_factor());
  catalog = db_and_table.first;
  table_name = db_and_table.second;
  RawConstant index_name =
      HandleIdentifierName(Util::GetCtxString(ctx->relation_name()));
  parent_object = std::make_shared<DropIndexObject>(
      catalog, table_name, index_name, Util::GetCtxString(ctx));
}

void OBMySQLObjectParser::enterCreate_view_stmt(
    OBParser::Create_view_stmtContext* ctx) {
  ddl_parse_context->SetErrMsg("parse create view not impl");
  return;
}

}  // namespace source
}  // namespace etransfer