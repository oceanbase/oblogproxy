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
#include <sys/stat.h>
#include <fstream>

namespace oceanbase {
namespace logproxy {

class FsUtil {
public:
  typedef enum { FS_DIR = DT_DIR, FS_FILE = DT_REG } FileType;

  static bool exist(const std::string& path);

  static bool mkdir(const std::string& dest);

  static bool remove(const std::string& dest);

  static bool remove_subs(const std::string& dest);

  static bool write_file(const std::string& filename, const std::string& content);

  static void dir_foreach(const std::string& dir, const std::function<void(const std::string&, const FileType)>& func);

  static std::string file_md5(const std::string& filename);

  static bool read_file(const std::string& filename, std::string& content, bool trim_content = true);

  static bool read_number(const std::string& filename, int64_t& number);

  static bool read_lines(const std::string& filename, std::vector<std::string>& lines);

  static bool read_kvs(const std::string& filename, const std::string& sep, std::map<std::string, std::string>& kvs);
};

}  // namespace logproxy
}  // namespace oceanbase
