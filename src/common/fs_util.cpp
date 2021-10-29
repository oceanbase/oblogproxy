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

#include <libgen.h>
#include <unistd.h>
#include "openssl/md5.h"
#include "common/log.h"
#include "common/fs_util.h"

namespace oceanbase {
namespace logproxy {

bool FsUtil::exist(const std::string& path)
{
  struct stat info;
  int ret = stat(path.c_str(), &info);
  if (ret == 0) {
    return true;
  }
  if (errno == ENOENT) {
    return false;
  }
  OMS_ERROR << "Can't access the path: " << path;
  return false;
}

bool FsUtil::mkdir(const std::string& path)
{
  if (path.empty() || exist(path)) {
    return true;
  }

  // TODO... recursive

  if (::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
    OMS_ERROR << "Failed to mkdir, path: " << path << ". system error:" << strerror(errno);
    return false;
  }
  return true;
}

bool FsUtil::remove(const std::string& dest)
{
  if (!remove_subs(dest)) {
    return false;
  }

  int ret = ::remove(dest.c_str());
  if (ret != 0) {
    OMS_ERROR << "Failed to remove " << dest << ", ret: " << ret << ", errno: " << errno
              << ", error: " << strerror(errno);
    return false;
  }
  return true;
}

bool FsUtil::remove_subs(const std::string& dest)
{
  if (!exist(dest)) {
    return true;
  }

  DIR* cur_dir = opendir(dest.c_str());
  if (cur_dir == nullptr) {
    OMS_ERROR << "Failed to opendir: " << dest;
    return false;
  }
  struct dirent* ent = nullptr;
  while ((ent = readdir(cur_dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }
    if (!remove(dest + "/" + ent->d_name)) {
      OMS_ERROR << "failed to remove: " << dest;
      closedir(cur_dir);
      return false;
    }
  }
  closedir(cur_dir);
  return true;
}

bool FsUtil::write_file(const std::string& filename, const std::string& content)
{
  FILE* fp = ::fopen(filename.c_str(), "w+");
  if (fp == nullptr) {
    OMS_ERROR << "Failed to open " << filename << ", errno: " << errno << ", error: " << strerror(errno);
    return false;
  }
  size_t n = fwrite(content.c_str(), content.size(), 1, fp);
  fclose(fp);
  if (n != 1) {
    OMS_ERROR << "Failed to write file, length error, file_path: " << filename;
    return false;
  }
  return true;
}

void FsUtil::dir_foreach(const std::string& dir, const std::function<void(const std::string&, const FileType)>& func)
{
  DIR* cur_dir = opendir(dir.c_str());
  if (cur_dir == nullptr) {
    OMS_ERROR << "Failed to opendir " << dir;
    return;
  }
  struct dirent* ent = nullptr;
  while ((ent = readdir(cur_dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    if (ent->d_type == DT_REG || ent->d_type == DT_DIR) {
      func(ent->d_name, (FileType)ent->d_type);
    }
  }
  closedir(cur_dir);
}

std::string FsUtil::file_md5(const std::string& filename)
{
  MD5_CTX ctx;
  MD5_Init(&ctx);

  FILE* fp = fopen(filename.c_str(), "rb");
  if (fp == nullptr) {
    return "";
  }

  unsigned char buf[8092];
  size_t count = 0;
  while ((count = fread(buf, 1, sizeof(buf), fp)) > 0) {
    MD5_Update(&ctx, buf, count);
  }
  fclose(fp);

  buf[0] = '\0';
  MD5_Final(buf, &ctx);

  size_t idx = 0;
  char md5[33] = "\0";
  for (size_t i = 0; i < 16; ++i) {
    idx += snprintf(md5 + idx, 32, "%02x", buf[i]);
  }
  return {md5};
}

}  // namespace logproxy
}  // namespace oceanbase
