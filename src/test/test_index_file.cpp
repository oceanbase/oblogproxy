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
  {
    uint32_t index = 1;
    std::string file_name = oceanbase::binlog::CommonUtils::fill_binlog_file_name(index);
    ASSERT_EQ("mysql-bin.000001", file_name);
  }

  {
    uint64_t index = UINT64_MAX;
    std::string file_name = oceanbase::binlog::CommonUtils::fill_binlog_file_name(index);
    ASSERT_EQ("mysql-bin.18446744073709551615", file_name);
  }

  {
    uint64_t index = 999999;
    std::string file_name = oceanbase::binlog::CommonUtils::fill_binlog_file_name(index);
    ASSERT_EQ("mysql-bin.999999", file_name);
  }

  {
    uint64_t index = 65535;
    std::string file_name = oceanbase::binlog::CommonUtils::fill_binlog_file_name(index);
    ASSERT_EQ("mysql-bin.065535", file_name);
  }
}

// TEST(IndexFile, file_lock)
//{
//   /*
//    * Test the basic file lock function. It is expected that the child process will get the lock first, and the parent
//    process will get the lock 3 seconds after the child process gets the lock.
//    */
//   std::string path = fs::current_path().c_str();
//   std::string file = path + "/" + BINLOG_INDEX_NAME;
//
//   pid_t pid = fork();
//
//   if (pid == 0) {
//     OMS_INFO("child process");
//     FILE* lock_fp = nullptr;
//     defer(binlog_index_unlock(lock_fp));
//     if (binlog_index_lock(lock_fp, file) == -1) {
//       OMS_INFO("File lock acquisition failed in child process");
//     } else {
//       OMS_INFO("File lock acquired in child process");
//       sleep(3); // Simulate the state of a child process holding a lock
//     }
//   } else if (pid > 0) {
//     OMS_INFO("Parent process");
//     sleep(1);
//     FILE* lock_fp = nullptr;
//     defer(binlog_index_unlock(lock_fp));
//     if (binlog_index_lock(lock_fp, file) == -1) {
//       OMS_INFO("File lock acquisition failed in parent process");
//     } else {
//       OMS_INFO("File lock acquired in parent process");
//       sleep(3); // Simulate the state of the parent process holding the lock
//     }
//   } else {
//     OMS_INFO("Fork failed");
//   }
//
// }
//
// TEST(IndexFile, binlog_index)
//{
//   /*
//    * Test the basic file lock function. It is expected that the child process will get the lock first, and the parent
//    process will get the lock 3 seconds after the child process gets the lock.
//    */
//   std::string path = fs::current_path().c_str();
//   std::string file = path + "/" + BINLOG_INDEX_NAME;
//   auto fp = FsUtil::fopen_binary(file, "w");
//   assert(fp!=nullptr);
//   FsUtil::fclose_binary(fp);
//   BinlogIndexRecord record;
//   record.set_index(0);
//   record.set_file_name(path + "/obmysql-bin.000001");
//   std::pair<std::string, uint64_t> mapping;
//   mapping.first = "1004_300000";
//   mapping.second = 20000;
//   record.set_before_mapping(mapping);
//   record.set_current_mapping(mapping);
//   std::string base = path + "/obmysql-bin";
//   for (int i = 0; i < 10; ++i) {
//     record.set_index(record.get_index() + 1);
//     std::stringstream string_stream;
//     string_stream << std::setfill('0') << std::setw(6) << record.get_index();
//     record.set_file_name(base + "." + string_stream.str());
//     add_index(file, record);
//   }
//
//   pid_t pid = fork();
//
//   if (pid == 0) {
//     OMS_INFO("child process");
//     sleep(1);
//     BinlogIndexRecord index_record;
//     index_record.set_index(10);
//     index_record.set_file_name(path + "/obmysql-bin.000010");
//     std::pair<std::string, uint64_t> mapping;
//     mapping.first = "1004_300000";
//     mapping.second = 20000;
//     index_record.set_before_mapping(mapping);
//     index_record.set_current_mapping(mapping);
//     update_index(file, index_record);
//     OMS_INFO("record:{}", index_record.to_string());
//     index_record.set_index(11);
//     index_record.set_file_name(path + "/obmysql-bin.000011");
//     add_index(file, index_record);
//     std::vector<BinlogIndexRecord*> binlog_index_records;
//     defer(release_vector(binlog_index_records));
//     fetch_index_vector(file, binlog_index_records);
//     ASSERT_EQ(6, binlog_index_records.size());
//     ASSERT_EQ(binlog_index_records.back()->get_index(), 11);
//   } else if (pid > 0) {
//     OMS_INFO("Parent process");
//     std::vector<std::string> purge_files;
//     std::string err_msg;
//     purge_binlog_index(path + "/", "obmysql-bin.000005", "", err_msg, purge_files);
//   } else {
//     OMS_INFO("Fork failed");
//   }
// }
