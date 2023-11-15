/**
 * Copyright (c) 2023 OceanBase
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
#include <string>
#include <vector>
#include <filesystem>
#include "fs_util.h"

namespace oceanbase {
namespace logproxy {
struct BinlogIndexRecord {
public:
  BinlogIndexRecord(const std::string& file_name, int index);

  BinlogIndexRecord() = default;

  virtual ~BinlogIndexRecord();

  const std::pair<std::string, uint64_t>& get_before_mapping() const;

  void set_before_mapping(const std::pair<std::string, uint64_t>& before_mapping);

  const std::pair<std::string, uint64_t>& get_current_mapping() const;

  void set_current_mapping(const std::pair<std::string, uint64_t>& current_mapping);

  const std::string& get_file_name() const;

  void set_file_name(const std::string& file_name);

  uint64_t get_index() const;

  void set_index(uint64_t index);

  uint64_t get_checkpoint() const;

  void set_checkpoint(uint64_t checkpoint);

  uint64_t get_position() const;

  void set_position(uint64_t position);

  std::string to_string() const;

  void parse(const std::string& content);

  std::string _file_name;
  uint64_t _index = 0;
  // mapping of ob txn and mysql txn
  std::pair<std::string, uint64_t> _before_mapping;
  std::pair<std::string, uint64_t> _current_mapping;
  uint64_t _checkpoint = 0;
  uint64_t _position = 0;
};

int fetch_index_vector(const std::string& index_file_name, std::vector<BinlogIndexRecord*>& index_records);

int fetch_index_vector(FILE* fp, std::vector<BinlogIndexRecord*>& index_records);

int add_index(const std::string& index_file_name, const BinlogIndexRecord& record);

int get_index(const std::string& index_file_name, BinlogIndexRecord& record, size_t index = 1);

int purge_binlog_index(const std::string& base_name, const std::string& binlog_file, const std::string& before_purge_ts,
    std::string& error_msg, std::vector<std::string>& purge_binlog_files);

/**
 *
 * @param index_file_name  binlog index filename
 * @param record
 * @param pos Invert pos records
 * @return
 */
int update_index(const std::string& index_file_name, const BinlogIndexRecord& record, size_t pos = 2);

std::string get_mapping_str(const std::pair<std::string, int64_t>& mapping);

std::pair<std::string, int64_t> parse_mapping_str(const std::string& mapping_str);

bool is_active(const std::string& file, std::vector<BinlogIndexRecord*>& index_records);

}  // namespace logproxy
}  // namespace oceanbase
