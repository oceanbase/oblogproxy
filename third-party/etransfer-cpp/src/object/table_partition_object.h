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
#include "object/expr_token.h"
#include "object/object.h"
#include "object/type_info.h"
namespace etransfer {
namespace object {
enum PartitionType { HASH, RANGE, LIST, COLUMN, KEY, NONE };
enum RangePartitionSliceExprIdentifier {
  BETWEEN,
  GREAT_THAN,
  LESS_THAN,
  EQUALS
};

enum PartitionValueSource {
  COLUMNS,
  EXPR,
};
class PartitionSlice;
class PartitionValueSourceInfo {
 public:
  PartitionValueSource partition_value_source;
  std::vector<RawConstant> partition_columns;
  std::string expr;
  std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>> expr_tokens;

  PartitionValueSourceInfo(std::vector<RawConstant> partition_columns)
      : partition_value_source(PartitionValueSource::COLUMNS),
        partition_columns(partition_columns) {}

  PartitionValueSourceInfo(
      const std::string& expr,
      const std::shared_ptr<std::vector<std::shared_ptr<ExprToken>>>
          expr_tokens)
      : partition_value_source(PartitionValueSource::EXPR),
        expr(expr),
        expr_tokens(expr_tokens) {}
};

class GeneralPartitionInfo {
 public:
  std::shared_ptr<PartitionValueSourceInfo> partition_value_source_infos;
  PartitionType partition_type;
  int partition_num;
  std::shared_ptr<Strings> options;
  std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
      sub_partition_options;
  std::string algorithm;
  bool contain_columns_key_word;
  std::shared_ptr<ExprToken> interval_expr;

  GeneralPartitionInfo(){};
  GeneralPartitionInfo(
      std::shared_ptr<PartitionValueSourceInfo> partition_value_source_infos,
      PartitionType partition_type, int partition_num,
      std::shared_ptr<Strings> options,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partition_options,
      const std::string& algorithm, bool contain_columns_key_word,
      std::shared_ptr<ExprToken> interval_expr)
      : partition_value_source_infos(partition_value_source_infos),
        partition_type(partition_type),
        partition_num(partition_num),
        options(options),
        sub_partition_options(sub_partition_options),
        algorithm(algorithm),
        contain_columns_key_word(contain_columns_key_word),
        interval_expr(interval_expr) {}

  GeneralPartitionInfo(
      std::shared_ptr<PartitionValueSourceInfo> partition_value_source_infos,
      PartitionType partition_type, int partition_num,
      std::shared_ptr<Strings> options,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partition_options,
      const std::string& algorithm, bool contain_columns_key_word)
      : GeneralPartitionInfo(partition_value_source_infos, partition_type,
                             partition_num, options, sub_partition_options,
                             algorithm, contain_columns_key_word, nullptr) {}

  GeneralPartitionInfo(
      std::shared_ptr<PartitionValueSourceInfo> partition_value_source_infos,
      PartitionType partition_type, int partition_num,
      std::shared_ptr<Strings> options,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partition_options,
      bool contain_columns_key_word)
      : GeneralPartitionInfo(partition_value_source_infos, partition_type,
                             partition_num, options, sub_partition_options, "",
                             contain_columns_key_word) {}
};

class PartitionSlice {
 public:
  RawConstant partition_name;
  PartitionType partition_type;
  std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>> sub_partitions;
  std::shared_ptr<Strings> option_info;
  int seq;
  bool is_sub_partition = false;

  PartitionSlice(const RawConstant& partition_name,
                 PartitionType partition_type,
                 std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
                     sub_partitions,
                 std::shared_ptr<Strings> option_info, int seq)
      : partition_name(partition_name),
        partition_type(partition_type),
        sub_partitions(sub_partitions),
        option_info(option_info),
        seq(seq) {}

  PartitionType GetPartitionType() { return partition_type; }

  virtual ~PartitionSlice() = default;
};

class ListPartitionSlice : public PartitionSlice {
 public:
  std::shared_ptr<std::vector<RawConstant>> list_values;
  // this constructor is for building sub partition slices
  ListPartitionSlice(const RawConstant& partition_name,
                     std::shared_ptr<Strings> option_info,
                     std::shared_ptr<std::vector<RawConstant>> list_values)
      : ListPartitionSlice(partition_name, nullptr, option_info, list_values,
                           -1) {
    is_sub_partition = true;
  }

  ListPartitionSlice(
      const RawConstant& partition_name,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partitions,
      std::shared_ptr<Strings> option_info,
      std::shared_ptr<std::vector<RawConstant>> list_values, int seq)
      : PartitionSlice(partition_name, PartitionType::LIST, sub_partitions,
                       option_info, seq),
        list_values(list_values) {}
};

class HashPartitionSlice : public PartitionSlice {
 public:
  int partition_number;

  HashPartitionSlice(
      const RawConstant& partition_name,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partitions,
      std::shared_ptr<Strings> option_info, int seq, int partition_number)
      : PartitionSlice(partition_name, PartitionType::HASH, sub_partitions,
                       option_info, seq),
        partition_number(partition_number) {}
  HashPartitionSlice(
      const RawConstant& partition_name,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partitions,
      std::shared_ptr<Strings> option_info, int seq)
      : HashPartitionSlice(partition_name, sub_partitions, option_info, seq,
                           TypeInfo::ANONYMOUS_NUMBER) {}

  HashPartitionSlice(const RawConstant& partition_name,
                     std::shared_ptr<Strings> option_info, int seq,
                     int partition_number)
      : HashPartitionSlice(partition_name, nullptr, option_info, seq,
                           partition_number) {
    is_sub_partition = true;
  }

  HashPartitionSlice(const RawConstant& partition_name,
                     std::shared_ptr<Strings> option_info, int seq)
      : HashPartitionSlice(partition_name, option_info, seq,
                           TypeInfo::ANONYMOUS_NUMBER) {}

  int GetPartitionNumber() { return partition_number; }
};

class RangePartitionSlice : public PartitionSlice {
 public:
  RawConstant slice_name;
  std::shared_ptr<std::vector<RawConstant>> start_expr;
  bool contains_start_point;
  std::shared_ptr<std::vector<RawConstant>> end_expr;
  bool contains_end_point;
  RangePartitionSliceExprIdentifier slice_expr_identifier;
  std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>> sub_partitions;

  // this constructor is for building sub partition slices
  RangePartitionSlice(const RawConstant& slice_name,
                      RangePartitionSliceExprIdentifier slice_expr_identifier,
                      std::shared_ptr<std::vector<RawConstant>> start_expr,
                      bool contains_start_point,
                      std::shared_ptr<std::vector<RawConstant>> end_expr,
                      bool contains_end_point, std::shared_ptr<Strings> options)
      : RangePartitionSlice(slice_name, start_expr, contains_start_point,
                            end_expr, contains_end_point, slice_expr_identifier,
                            nullptr, options, -1) {
    is_sub_partition = true;
  }

  RangePartitionSlice(
      const RawConstant& slice_name,
      std::shared_ptr<std::vector<RawConstant>> start_expr,
      bool contains_start_point,
      std::shared_ptr<std::vector<RawConstant>> end_expr,
      bool contains_end_point,
      RangePartitionSliceExprIdentifier slice_expr_identifier,
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>>
          sub_partitions,
      std::shared_ptr<Strings> options, int seq)
      : PartitionSlice(slice_name, PartitionType::RANGE, sub_partitions,
                       options, seq),
        slice_name(slice_name),
        start_expr(start_expr),
        end_expr(end_expr),
        slice_expr_identifier(slice_expr_identifier),
        sub_partitions(sub_partitions),
        contains_start_point(contains_end_point),
        contains_end_point(contains_end_point) {}

  PartitionType GetPartitionType() { return PartitionType::RANGE; }
};
class TablePartitionObject : public Object {
 private:
  std::vector<std::shared_ptr<PartitionSlice>> partition_slices_;
  std::shared_ptr<GeneralPartitionInfo> partition_info_;
  std::shared_ptr<GeneralPartitionInfo> sub_partition_info_;

 public:
  TablePartitionObject(Catalog catalog, RawConstant object_name,
                       std::string raw_ddl,
                       std::shared_ptr<GeneralPartitionInfo> partition_info)
      : Object(catalog, object_name, raw_ddl,
               ObjectType::TABLE_PARTITION_OBJECT),
        partition_info_(partition_info) {}

  static std::string GetPartitionTypeName(PartitionType partition_type);

  std::shared_ptr<GeneralPartitionInfo> GetPartitionInfo() {
    return partition_info_;
  }

  std::shared_ptr<GeneralPartitionInfo> GetSubPartitionInfo() {
    return sub_partition_info_;
  }

  std::vector<std::shared_ptr<PartitionSlice>> GetPartitionSlices() {
    return partition_slices_;
  }

  void AddPartitionSlice(std::shared_ptr<PartitionSlice> partition_slice) {
    partition_slices_.push_back(partition_slice);
  }

  void SetSubPartitionInfo(
      std::shared_ptr<GeneralPartitionInfo> sub_partition_info) {
    this->sub_partition_info_ = sub_partition_info;
  }
};

}  // namespace object

}  // namespace etransfer
