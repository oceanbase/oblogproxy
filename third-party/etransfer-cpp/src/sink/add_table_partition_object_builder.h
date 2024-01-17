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
#include "object/table_partition_object.h"
#include "sink/object_builder.h"
namespace etransfer {
namespace sink {

class AddTablePartitionObjectBuilder : public ObjectBuilder {
 public:
  AddTablePartitionObjectBuilder() {}

  Strings RealBuildSql(
      ObjectPtr db_object, ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context) override;

  std::string BuildCreateAddPartitionObject(
      std::shared_ptr<TablePartitionObject> table_partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string GetColumnsKeyword(
      std::shared_ptr<GeneralPartitionInfo> partition_info,
      std::shared_ptr<BuildContext> context);

  std::string BuildPartitionKeyStatement(
      std::shared_ptr<TablePartitionObject> table_partition_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildSubpartitionInfo(
      std::shared_ptr<TablePartitionObject> table_partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  // mysql or ob mysql doesn't support partition or subpartition name is null
  std::string BuildPartitionSliceName(
      std::shared_ptr<PartitionSlice> partition_slice,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildPartitionSliceString(
      std::shared_ptr<TablePartitionObject> table_partition_object,
      std::shared_ptr<PartitionSlice> partition_slice, ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildSubPartSliceString(
      std::shared_ptr<std::vector<std::shared_ptr<PartitionSlice>>> part_slices,
      std::shared_ptr<TablePartitionObject> partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildListPartitionSlices(
      const std::string& raw_info,
      std::shared_ptr<ListPartitionSlice> partition_slice,
      std::shared_ptr<TablePartitionObject> partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  // check if contain invalid list partition slice value, such as ob mysql
  // support special list value : default, it mean 'rest', but other db don't
  // support
  void CheckListPartitionSliceValue(
      std::string list_value,
      std::shared_ptr<BuildContext> sql_builder_context);

  // build key partition
  std::string BuildKeyPartitionSlices(
      std::shared_ptr<PartitionSlice> partition_slice,
      std::shared_ptr<TablePartitionObject> partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildHashPartitionSlices(
      std::shared_ptr<HashPartitionSlice> partition_slice,
      std::shared_ptr<TablePartitionObject> partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildRangePartitionSlices(
      const std::string& raw_info,
      std::shared_ptr<RangePartitionSlice> partition_slice,
      std::shared_ptr<TablePartitionObject> partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string BuildSubPartSlice(
      std::shared_ptr<PartitionSlice> sub_part_slice,
      std::shared_ptr<TablePartitionObject> partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context);

  std::string AppendRangePartitionStr(
      const std::string& part_string, const std::string& slice_name,
      bool contains_end_point, bool contains_start_point,
      std::shared_ptr<BuildContext> sql_builder_context,
      std::shared_ptr<std::vector<RawConstant>> end_exprs,
      std::shared_ptr<std::vector<RawConstant>> start_exprs);

  std::string BuildAlterTableAddPartitionObject(
      std::shared_ptr<TablePartitionObject> table_partition_object,
      ObjectPtr parent_object,
      std::shared_ptr<BuildContext> sql_builder_context) {
    std::string line("ADD PARTITION (\n");
    const auto& slices = table_partition_object->GetPartitionSlices();
    for (int i = 0; i < slices.size(); i++) {
      if (i > 0) {
        line.append(" ,\n");
      }
      line.append(BuildPartitionSliceString(table_partition_object, slices[i],
                                            parent_object,
                                            sql_builder_context));
    }
    line.append(")");
    return line;
  }
};
}  // namespace sink

}  // namespace etransfer
