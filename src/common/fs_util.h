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

#pragma once

#include <string>
#include <functional>
#include <dirent.h>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include "msg_buf.h"
#include "common.h"
#include "log.h"
#include "guard.hpp"

namespace oceanbase {
namespace logproxy {
namespace fs = std::filesystem;
class FsUtil {
public:
  typedef enum { FS_DIR = DT_DIR, FS_FILE = DT_REG } FileType;

  struct disk_info {
    uint64_t capacity;
    uint64_t free;
    uint64_t available;
  };

  static bool exist(const std::string& path);

  static bool mkdir(const std::string& dest);

  static bool remove(const std::string& dest, bool recursive = true);

  static bool remove_subs(const std::string& dest);

  static int write_file(const std::string& filename, const std::string& content);

  static std::vector<std::string> dir_foreach(const std::string& dir,
      const std::function<void(const std::string&, const FileType, std::vector<std::string>&)>& func);

  static std::string file_md5(const std::string& filename);

  static int64_t append_file(FILE* stream, std::string& content, size_t size);

  static int64_t append_file(FILE* fp, unsigned char* content, size_t size);

  static int64_t append_file(FILE* fs, MsgBuf& content, size_t size);

  static int64_t append_file(const std::string& file, MsgBuf& content);

  static FILE* fopen_binary(const std::string& name, const std::string& mode = "ab+");

  static void fclose_binary(FILE* fs);

  static size_t read_file(std::ifstream& input, unsigned char* content, int64_t start_pos, int64_t len);

  static int rewrite(FILE* input, unsigned char* content, uint64_t start_pos, uint64_t len);

  static size_t read_file(FILE* input, unsigned char* content, int64_t start_pos, size_t len);

  static int seekg_last_line(FILE* fp, size_t& pos);

  static bool read_file(const std::string& filename, std::string& content, bool trim_content = true);

  static bool read_number(const std::string& filename, int64_t& number);

  static bool read_lines(const std::string& filename, std::vector<std::string>& lines);

  static bool read_kvs(const std::string& filename, const std::string& sep, std::map<std::string, std::string>& kvs);

  static uint64_t file_size(const std::string& filename);

  static uint64_t file_size_for_path(const std::filesystem::path& path);

  static uint64_t folder_size(const std::string& folder);

  static disk_info space(const std::string& path);
};

template <typename T>
void write_obj(std::ofstream& os, T& obj)
{
  os.write(reinterpret_cast<char*>(&obj), sizeof(T));
}

template <typename T>
bool read_obj(std::ifstream& is, T& obj)
{
  if (is.read(reinterpret_cast<char*>(&obj), sizeof(T))) {
    return true;
  }
  return false;
}

template <typename T>
void batch_read_obj(std::ifstream& is, std::vector<T>& objs, int64_t size)
{
  if (size == 0) {
    size = INT32_MAX;
  }

  for (int i = 0; i < size; ++i) {
    T record = T();
    if (!read_obj(is, record)) {
      break;
    }
    objs.emplace_back(record);
  }
}

}  // namespace logproxy
}  // namespace oceanbase
