/**
 * Copyright (c) 2021 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "gtest/gtest.h"
#include "common.h"
#include "binlog_index.h"
#include "ob_log_event.h"
#include "binlog/common_util.h"

using namespace oceanbase::logproxy;
namespace fs = std::filesystem;
TEST(IndexFile, add_index)
{
  BinlogIndexRecord record;
  record._index = 1;
  record._file_name = fs::current_path().string() + "/" + "mysql-bin.1";
  record._position = 12543;
  std::string path(fs::current_path().string() + "/");
  add_index(path + BINLOG_INDEX_NAME, record);

  record._index = 2;
  record._file_name = fs::current_path().string() + "/" + "mysql-bin.2";
  record._position = 72578406;
  add_index(path + BINLOG_INDEX_NAME, record);
  ASSERT_EQ(true, FsUtil::remove(path + BINLOG_INDEX_NAME, false));
}

TEST(IndexFile, release_vector)
{
  std::vector<BinlogIndexRecord*> vector_ptr;

  auto* val_1 = new BinlogIndexRecord();
  vector_ptr.emplace_back(val_1);

  release_vector(vector_ptr);
}

TEST(IndexFile, fill_binlog_file_name)
{
  uint16_t index = 1;
  std::string file_name = oceanbase::binlog::CommonUtils::fill_binlog_file_name(index);
  ASSERT_EQ("mysql-bin.000001", file_name);
}