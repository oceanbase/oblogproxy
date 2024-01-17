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

#include "sink/add_table_partition_object_builder.h"

#include "object/expr_token.h"
#include "sink/sql_builder_util.h"
namespace etransfer {
namespace sink {
Strings AddTablePartitionObjectBuilder::RealBuildSql(
    ObjectPtr db_object, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<TablePartitionObject> table_partition_object =
      std::dynamic_pointer_cast<TablePartitionObject>(db_object);
  Strings ret_vec;
  std::string ret;
  switch (parent_object->GetObjectType()) {
    case CREATE_TABLE_OBJECT:
    case INDEX_OBJECT:
      ret = BuildCreateAddPartitionObject(table_partition_object, parent_object,
                                          sql_builder_context);
      break;
    case ALTER_TABLE_OBJECT:
      ret = BuildAlterTableAddPartitionObject(
          table_partition_object, parent_object, sql_builder_context);
      break;
    default:
      sql_builder_context->SetErrMsg("unsupported add partition convert");
      return ret_vec;
  }
  ret_vec.push_back(ret);
  return ret_vec;
}

std::string AddTablePartitionObjectBuilder::BuildCreateAddPartitionObject(
    std::shared_ptr<TablePartitionObject> table_partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  line.append(" PARTITION BY ");
  if (table_partition_object->GetPartitionInfo()->algorithm != "") {
    line.append(table_partition_object->GetPartitionInfo()->algorithm)
        .append(" ");
  }

  line.append(TablePartitionObject::GetPartitionTypeName(
      table_partition_object->GetPartitionInfo()->partition_type));
  line.append(GetColumnsKeyword(table_partition_object->GetPartitionInfo(),
                                sql_builder_context));
  line.append(" (");
  line.append(
      BuildPartitionKeyStatement(table_partition_object, sql_builder_context));
  line.append(") ");
  if (table_partition_object->GetSubPartitionInfo() != nullptr) {
    line.append(BuildSubpartitionInfo(table_partition_object, parent_object,
                                      sql_builder_context));
  }
  if (!sql_builder_context->succeed) {
    return "";
  }
  if (table_partition_object->GetPartitionInfo()->partition_num > 0) {
    line.append("PARTITIONS ")
        .append(std::to_string(
            table_partition_object->GetPartitionInfo()->partition_num));
  }
  if (!table_partition_object->GetPartitionSlices().empty()) {
    line.append("\n(\n");
    const auto& partition_slices = table_partition_object->GetPartitionSlices();
    for (int i = 0; i < partition_slices.size(); i++) {
      const auto& partition_slice = partition_slices[i];
      if (i != 0) {
        line.append(",\n");
      }
      line.append(BuildPartitionSliceString(table_partition_object,
                                            partition_slice, parent_object,
                                            sql_builder_context));
    }
    line.append("\n)");
  }
  return line;
}

std::string AddTablePartitionObjectBuilder::BuildPartitionSliceString(
    std::shared_ptr<TablePartitionObject> table_partition_object,
    std::shared_ptr<PartitionSlice> partition_slice, ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string raw_info = table_partition_object->GetRawInfo();
  if (partition_slice->GetPartitionType() == PartitionType::RANGE) {
    std::shared_ptr<RangePartitionSlice> slice =
        std::dynamic_pointer_cast<RangePartitionSlice>(partition_slice);
    return BuildRangePartitionSlices(raw_info, slice, table_partition_object,
                                     parent_object, sql_builder_context);
  } else if (partition_slice->GetPartitionType() == PartitionType::LIST) {
    std::shared_ptr<ListPartitionSlice> slice =
        std::dynamic_pointer_cast<ListPartitionSlice>(partition_slice);
    return BuildListPartitionSlices(raw_info, slice, table_partition_object,
                                    parent_object, sql_builder_context);
  } else if (partition_slice->GetPartitionType() == PartitionType::HASH) {
    std::shared_ptr<HashPartitionSlice> slice =
        std::dynamic_pointer_cast<HashPartitionSlice>(partition_slice);
    return BuildHashPartitionSlices(slice, table_partition_object,
                                    parent_object, sql_builder_context);
  } else {
    return BuildKeyPartitionSlices(partition_slice, table_partition_object,
                                   parent_object, sql_builder_context);
  }
}

std::string AddTablePartitionObjectBuilder::BuildPartitionSliceName(
    std::shared_ptr<PartitionSlice> partition_slice,
    std::shared_ptr<BuildContext> sql_builder_context) {
  if ("" == partition_slice->partition_name.origin_string) {
    sql_builder_context->SetErrMsg("unsupported partition name is null");
    return "";
  }
  return SqlBuilderUtil::EscapeNormalObjectName(partition_slice->partition_name,
                                                sql_builder_context);
}

std::string AddTablePartitionObjectBuilder::BuildKeyPartitionSlices(
    std::shared_ptr<PartitionSlice> partition_slice,
    std::shared_ptr<TablePartitionObject> partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string part_string =
      partition_slice->is_sub_partition ? "\tSUBPARTITION " : "PARTITION ";
  std::string line("\t");
  line.append(part_string);
  line.append(BuildPartitionSliceName(partition_slice, sql_builder_context));
  if (partition_slice->sub_partitions != nullptr &&
      !partition_slice->sub_partitions->empty()) {
    sql_builder_context->SetErrMsg("unsupported partition combination");
  }
  return line;
}

std::string AddTablePartitionObjectBuilder::BuildSubPartSliceString(
    std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>> part_slices,
    std::shared_ptr<TablePartitionObject> partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  // build sub partition
  std::string line;
  if (part_slices == nullptr || part_slices->empty()) {
    return line;
  }
  line.append("\n\t(\n");
  for (int i = 0; i < (*part_slices).size(); i++) {
    const auto& sub_part_slice = part_slices->at(i);
    if (i > 0) {
      line.append(",\n");
    }
    line.append(BuildSubPartSlice(sub_part_slice, partition_object,
                                  parent_object, sql_builder_context));
  }
  line.append("\n\t)");
  return line;
}

std::string AddTablePartitionObjectBuilder::BuildSubPartSlice(
    std::shared_ptr<PartitionSlice> sub_part_slice,
    std::shared_ptr<TablePartitionObject> partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  if (sub_part_slice->partition_type == PartitionType::HASH) {
    return BuildHashPartitionSlices(
        std::dynamic_pointer_cast<HashPartitionSlice>(sub_part_slice),
        partition_object, parent_object, sql_builder_context);
  } else if (sub_part_slice->partition_type == PartitionType::KEY) {
    return BuildKeyPartitionSlices(sub_part_slice, partition_object,
                                   parent_object, sql_builder_context);
  }
  sql_builder_context->SetErrMsg("unsupported partition combination");
  return "";
}

std::string AddTablePartitionObjectBuilder::BuildHashPartitionSlices(
    std::shared_ptr<HashPartitionSlice> partition_slice,
    std::shared_ptr<TablePartitionObject> partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string part_string;
  std::string line("\t");

  // such as : partitions 4 / subpartitions 8
  if (partition_slice->GetPartitionNumber() != TypeInfo::ANONYMOUS_NUMBER) {
    part_string =
        partition_slice->is_sub_partition ? "\tSUBPARTITIONS " : "PARTITIONS ";
    line.append(part_string)
        .append(std::to_string(partition_slice->GetPartitionNumber()));
  } else {
    // such as : PARTITION (partition_name) / SUBPARTITION (subpartition_name)
    part_string =
        partition_slice->is_sub_partition ? "\tSUBPARTITION " : "PARTITION ";
    line.append(part_string);
    if ("" != partition_slice->partition_name.origin_string) {
      line.append(SqlBuilderUtil::EscapeNormalObjectName(
          partition_slice->partition_name, sql_builder_context));
    }
  }
  if (partition_slice->sub_partitions != nullptr &&
      !partition_slice->sub_partitions->empty()) {
    sql_builder_context->SetErrMsg("unsupported partition combination");
  }
  return line;
}

std::string AddTablePartitionObjectBuilder::BuildListPartitionSlices(
    const std::string& raw_info,
    std::shared_ptr<ListPartitionSlice> partition_slice,
    std::shared_ptr<TablePartitionObject> partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string part_string =
      partition_slice->is_sub_partition ? "\tSUBPARTITION " : "PARTITION ";
  std::string line("\t");
  std::string slice_name =
      BuildPartitionSliceName(partition_slice, sql_builder_context);
  line.append(part_string).append(slice_name).append(" VALUES");
  line.append(" IN");
  line.append("(");
  for (const auto& raw_constant : *partition_slice->list_values) {
    std::string list_value = Util::RawConstantValue(raw_constant);
    // TODO to_date
    // this is list by multi-columns
    CheckListPartitionSliceValue(list_value, sql_builder_context);
    if (!sql_builder_context->succeed) {
      return "";
    }
    line.append(raw_constant.origin_string);
    line.append(", ");
  }

  line = line.substr(0, line.length() - 2);  // remove last ','
  line.append(")");

  // build sub partition
  line.append(BuildSubPartSliceString(partition_slice->sub_partitions,
                                      partition_object, parent_object,
                                      sql_builder_context));

  return line;
}

std::string AddTablePartitionObjectBuilder::BuildRangePartitionSlices(
    const std::string& raw_info,
    std::shared_ptr<RangePartitionSlice> partition_slice,
    std::shared_ptr<TablePartitionObject> partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string part_string =
      partition_slice->is_sub_partition ? "\tSUBPARTITION " : "PARTITION ";
  std::string line("\t");
  std::string slice_name =
      BuildPartitionSliceName(partition_slice, sql_builder_context);
  auto end_expr = partition_slice->end_expr;
  auto start_expr = partition_slice->start_expr;
  if (end_expr->empty()) {
    sql_builder_context->SetErrMsg("unsupported partition convert");
    return "";
  }
  line.append(AppendRangePartitionStr(
      part_string, slice_name, partition_slice->contains_end_point,
      partition_slice->contains_start_point, sql_builder_context, end_expr,
      start_expr));

  // build sub partition
  line.append(BuildSubPartSliceString(partition_slice->sub_partitions,
                                      partition_object, parent_object,
                                      sql_builder_context));
  return line;
}

std::string AddTablePartitionObjectBuilder::AppendRangePartitionStr(
    const std::string& part_string, const std::string& slice_name,
    bool contains_end_point, bool contains_start_point,
    std::shared_ptr<BuildContext> sql_builder_context,
    std::shared_ptr<std::vector<RawConstant>> end_exprs,
    std::shared_ptr<std::vector<RawConstant>> start_exprs) {
  std::string line;
  line.append(part_string).append(slice_name).append(" VALUES LESS THAN (");
  for (int i = 0; i < end_exprs->size(); i++) {
    if (i > 0) {
      line.append(",");
    }
    line.append(end_exprs->at(i).origin_string);
  }
  line.append(")");
  if (contains_end_point) {
    sql_builder_context->SetErrMsg("add range partition lost endpoint value");
  }
  return line;
}

std::string AddTablePartitionObjectBuilder::GetColumnsKeyword(
    std::shared_ptr<GeneralPartitionInfo> partition_info,
    std::shared_ptr<BuildContext> context) {
  // first satisfy the partition type is range or list
  if (partition_info->partition_type == PartitionType::RANGE ||
      partition_info->partition_type == PartitionType::LIST) {
    if (partition_info->contain_columns_key_word) {
      return " COLUMNS";
    }
  }
  return "";
}

std::string AddTablePartitionObjectBuilder::BuildPartitionKeyStatement(
    std::shared_ptr<TablePartitionObject> table_partition_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::string line;
  if (table_partition_object->GetPartitionInfo()
          ->partition_value_source_infos->partition_value_source ==
      PartitionValueSource::COLUMNS) {
    const auto& partition_columns =
        table_partition_object->GetPartitionInfo()
            ->partition_value_source_infos->partition_columns;
    for (int i = 0; i < partition_columns.size(); i++) {
      if (i > 0) {
        line.append(",");
      }
      line.append(SqlBuilderUtil::GetRealColumnName(
          table_partition_object->GetCatalog(),
          table_partition_object->GetRawObjectName(), partition_columns.at(i),
          sql_builder_context));
    }
  } else {
    const auto& exprs = *table_partition_object->GetPartitionInfo()
                             ->partition_value_source_infos->expr_tokens;
    for (int i = 0; i < exprs.size(); i++) {
      if (i > 0) {
        line.append(" ");
      }
      if (exprs[i]->GetTokenType() == ExprTokenType::IDENTIFIER_NAME) {
        line.append(SqlBuilderUtil::GetRealColumnName(
            table_partition_object->GetCatalog(),
            table_partition_object->GetRawObjectName(), exprs[i]->token,
            sql_builder_context));
      } else {
        line.append(Util::RawConstantValue(exprs[i]->token));
      }
    }
  }
  return line;
}

std::string AddTablePartitionObjectBuilder::BuildSubpartitionInfo(
    std::shared_ptr<TablePartitionObject> table_partition_object,
    ObjectPtr parent_object,
    std::shared_ptr<BuildContext> sql_builder_context) {
  std::shared_ptr<GeneralPartitionInfo> sub_part_info =
      table_partition_object->GetSubPartitionInfo();
  std::string line;
  if (sub_part_info->sub_partition_options != nullptr &&
      !sub_part_info->sub_partition_options->empty()) {
    sql_builder_context->succeed = false;
  }
  line.append("SUBPARTITION BY ");
  if (sub_part_info->algorithm != "") {
    line.append(sub_part_info->algorithm).append(" ");
  }
  line.append(TablePartitionObject::GetPartitionTypeName(
      sub_part_info->partition_type));
  line.append(GetColumnsKeyword(sub_part_info, sql_builder_context));
  line.append("(");
  if (sub_part_info->partition_value_source_infos->partition_value_source ==
      PartitionValueSource::COLUMNS) {
    const auto& partition_columns =
        sub_part_info->partition_value_source_infos->partition_columns;
    for (int i = 0; i < partition_columns.size(); i++) {
      if (i > 0) {
        line.append(",");
      }
      line.append(SqlBuilderUtil::GetRealColumnName(
          table_partition_object->GetCatalog(),
          table_partition_object->GetRawObjectName(), partition_columns.at(i),
          sql_builder_context));
    }
  } else {
    const auto& exprs = *table_partition_object->GetSubPartitionInfo()
                             ->partition_value_source_infos->expr_tokens;
    for (int i = 0; i < exprs.size(); i++) {
      if (i > 0) {
        line.append(" ");
      }
      if (exprs[i]->GetTokenType() == ExprTokenType::IDENTIFIER_NAME) {
        line.append(SqlBuilderUtil::GetRealColumnName(
            table_partition_object->GetCatalog(),
            table_partition_object->GetRawObjectName(), exprs[i]->token,
            sql_builder_context));
      } else {
        line.append(Util::RawConstantValue(exprs[i]->token));
      }
    }
  }
  line.append(")");
  if (sub_part_info->partition_num > 0) {
    line.append(" SUBPARTITIONS ")
        .append(std::to_string(sub_part_info->partition_num));
  }

  if (!sub_part_info->sub_partition_options->empty()) {
    sql_builder_context->SetErrMsg(
        "mysql doesn't support to build : subpartition template");
    return "";
  }
  line.append(" ");
  return line;
}

void AddTablePartitionObjectBuilder::CheckListPartitionSliceValue(
    std::string list_value, std::shared_ptr<BuildContext> sql_builder_context) {
  if (Util::EqualsIgnoreCase(list_value, "DEFAULT")) {
    sql_builder_context->SetErrMsg(
        "mysql doesn't support list partition slice value " + list_value);
  }
}
}  // namespace sink

}  // namespace etransfer
