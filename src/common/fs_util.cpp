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

#include <fstream>
#include "openssl/md5.h"
#include "fs_util.h"
#include "log.h"
#include "common.h"
#include "str.h"
#include "file_lock.h"

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
  OMS_STREAM_ERROR << "Can't access the path: " << path;
  return false;
}

bool mkdir_recursive(std::string path)
{
  bool mk_res = false;
  int n_rc = ::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (n_rc == -1) {
    switch (errno) {
      case ENOENT:
        // parent didn't exist, try to create it
        if (mkdir_recursive(path.substr(0, path.find_last_of('/'))))
          // Now, try to create again.
          mk_res = 0 == ::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        else
          mk_res = false;
        break;
      case EEXIST:
        // Done!
        mk_res = true;
        break;
      default:
        mk_res = false;
        break;
    }
  } else
    mk_res = true;
  return mk_res;
}

bool FsUtil::mkdir(const std::string& path)
{
  if (path.empty() || exist(path)) {
    return true;
  }

  // TODO... recursive
  if (!mkdir_recursive(path) && EEXIST != errno) {
    OMS_STREAM_ERROR << "Failed to mkdir, path: " << path << ". system error:" << strerror(errno);
    return false;
  }
  return true;
}

bool FsUtil::remove(const std::string& dest, bool recursive)
{
  if (recursive && !remove_subs(dest)) {
    return false;
  }

  int ret = ::remove(dest.c_str());
  if (ret != 0) {
    OMS_STREAM_ERROR << "Failed to remove " << dest << ", ret: " << ret << ", errno: " << errno
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
    OMS_STREAM_ERROR << "Failed to opendir: " << dest;
    return false;
  }
  struct dirent* ent = nullptr;
  while ((ent = readdir(cur_dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }
    if (!remove(dest + "/" + ent->d_name, ent->d_type == DT_DIR)) {
      OMS_STREAM_ERROR << "failed to remove: " << dest;
      closedir(cur_dir);
      return false;
    }
  }
  closedir(cur_dir);
  return true;
}

int FsUtil::write_file(const std::string& filename, const std::string& content)
{
  FILE* fp = ::fopen(filename.c_str(), "w+");
  if (fp == nullptr) {
    OMS_STREAM_ERROR << "Failed to open " << filename << ", errno: " << errno << ", error: " << strerror(errno);
    return OMS_FAILED;
  }
  size_t n = fwrite(content.c_str(), content.size(), 1, fp);
  fclose(fp);
  if (n != 1) {
    OMS_STREAM_ERROR << "Failed to write file, length error, file_path: " << filename;
    return OMS_FAILED;
  }
  return OMS_OK;
}

std::vector<std::string> FsUtil::dir_foreach(const std::string& dir,
    const std::function<void(const std::string&, const FileType, std::vector<std::string>&)>& func)
{
  DIR* cur_dir = opendir(dir.c_str());
  std::vector<std::string> rets;
  if (cur_dir == nullptr) {
    OMS_STREAM_ERROR << "Failed to opendir " << dir;
    return rets;
  }
  struct dirent* ent = nullptr;
  while ((ent = readdir(cur_dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    if (ent->d_type == DT_REG || ent->d_type == DT_DIR) {
      func(ent->d_name, (FileType)ent->d_type, rets);
    }
  }
  closedir(cur_dir);
  return rets;
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

int64_t FsUtil::append_file(FILE* stream, std::string& content, size_t size)
{
  size_t n = fwrite(content.c_str(), size, 1, stream);
  if (n != 1) {
    OMS_STREAM_ERROR << "Failed to write file, length error, written: " << n << "expected: " << size;
    return -1;
  }
  return n;
}

int64_t FsUtil::append_file(FILE* fp, unsigned char* content, size_t size)
{
  if (fp == nullptr) {
    OMS_ERROR("Failed to write file");
    return OMS_FAILED;
  }

  size_t n = fwrite(content, sizeof(unsigned char), size, fp);
  if (n != size) {
    OMS_ERROR("Failed to write file, length error, written: {} expected: ", n, size);
    return OMS_FAILED;
  }
  fflush(fp);
  return size;
}

FILE* FsUtil::fopen_binary(const std::string& name, const std::string& mode)
{
  return std::fopen(name.c_str(), mode.c_str());
}

void FsUtil::fclose_binary(FILE* fs)
{
  if (fs != nullptr) {
    fclose(fs);
    fs = nullptr;
  }
}

size_t FsUtil::read_file(std::ifstream& input, unsigned char* content, int64_t start_pos, int64_t len)
{
  if (input.is_open()) {
    input.seekg(start_pos);
    input.read(reinterpret_cast<char*>(content), len);
    //    if (len != input.gcount()) {
    //      return OMS_FAILED;
    //    }
  } else {
    OMS_STREAM_ERROR << "input is closed";
    return OMS_FAILED;
  }
  return OMS_OK;
}

int FsUtil::rewrite(FILE* input, unsigned char* content, uint64_t start_pos, uint64_t len)
{
  fseek(input, int64_t(start_pos), SEEK_SET);
  fwrite(content, 1, len, input);
  OMS_STREAM_DEBUG << "current file offset:" << ftell(input);
  if (ferror(input)) {
    OMS_ERROR("Failed to rewrite :{}", strerror(errno));
    return OMS_IO_ERROR;
  }
  fflush(input);
  return OMS_OK;
}

size_t FsUtil::read_file(FILE* input, unsigned char* content, int64_t start_pos, size_t len)
{
  fseek(input, start_pos, SEEK_SET);
  uint64_t count = fread(content, 1, len, input);
  if (ferror(input)) {
    OMS_ERROR("Failed to read_file :{}", strerror(errno));
    return OMS_IO_ERROR;
  }
  if (count != len) {
    OMS_ERROR("Failed to read_file,expected to be :{} actual:{}", len, count);
    return OMS_IO_ERROR;
  }
  return OMS_OK;
}

int FsUtil::seekg_last_line(FILE* fp, size_t& pos)
{
  if (fp != nullptr) {
    fseek(fp, 0, SEEK_END);
    char ch = ' ';
    while (ch != '\n') {
      fseek(fp, -2, SEEK_CUR);
      if (ftell(fp) <= 0) {
        fseek(fp, 0, SEEK_CUR);
        break;
      }
      ch = fgetc(fp);
    }
    pos = ftell(fp);
  }
  return OMS_OK;
}

bool FsUtil::read_file(const std::string& filename, std::string& content, bool trim_content)
{
  std::ifstream ifs(filename);
  if (!ifs.good()) {
    OMS_STREAM_ERROR << "Failed to open: " << filename << " errno:" << errno;
    return false;
  }

  std::stringstream buffer;
  buffer << ifs.rdbuf();
  content = buffer.str();
  if (trim_content) {
    trim(content);
  }
  return true;
}

bool FsUtil::read_number(const std::string& filename, int64_t& number)
{
  std::string val;
  if (!read_file(filename, val)) {
    return false;
  }
  number = strtoll(val.c_str(), nullptr, 10);
  return true;
}

bool FsUtil::read_lines(const std::string& filename, std::vector<std::string>& lines)
{
  std::ifstream ifs(filename);
  if (!ifs.good()) {
    OMS_STREAM_ERROR << "Failed to open: " << filename << " errno:" << errno;
    return false;
  }

  for (std::string line; std::getline(ifs, line);) {
    lines.emplace_back(std::move(line));
  }
  return true;
}

bool FsUtil::read_kvs(const std::string& filename, const std::string& sep, std::map<std::string, std::string>& kvs)
{
  if (sep.empty()) {
    OMS_STREAM_ERROR << "Invalid empty seperator";
    return false;
  }

  if (filename.empty()) {
    OMS_STREAM_ERROR << "Filename is empty";
    return false;
  }

  std::vector<std::string> lines;
  if (!exist(filename)) {
    return false;
  }
  if (!read_lines(filename, lines)) {
    return false;
  }
  for (const std::string& line : lines) {
    std::vector<std::string> kv(2);
    split_by_str(line, sep, kv);
    if (kv.size() != 2) {
      continue;
    }
    kvs.emplace(std::move(kv[0]), std::move(kv[1]));
  }
  return true;
}

uint64_t FsUtil::file_size(const std::string& filename)
{
  if (filename.empty()) {
    return 0;
  }
  struct stat file_stat;
  int file_size = stat(filename.c_str(), &file_stat);
  return (file_size == 0 ? file_stat.st_size : 0);
}

int64_t FsUtil::append_file(const std::string& file, MsgBuf& content)
{
  if (file.empty()) {
    OMS_STREAM_ERROR << "Failed to write file " << file;
    return OMS_FAILED;
  }
  FILE* fp = FsUtil::fopen_binary(file);
  if (fp == nullptr) {
    OMS_STREAM_ERROR << "Failed to open file:" << file;
    return OMS_FAILED;
  }

  for (const auto& iter : content) {
    size_t n = fwrite(iter.buffer(), sizeof(char), iter.size(), fp);
    if (n != iter.size()) {
      OMS_STREAM_ERROR << "Failed to write file, length error, written: " << n << "expected: " << iter.size();
      fclose(fp);
      return OMS_FAILED;
    }
  }
  int ret = fclose(fp);
  OMS_DEBUG("Success to write file:{} ret:{}", content.byte_size(), ret);
  return ret;
}

uint64_t FsUtil::folder_size(const std::string& folder)
{
  uintmax_t total_size = 0;
  std::error_code error_code;
  if (!exist(folder)) {
    return total_size;
  }
  try {
    for (auto& entry : fs::directory_iterator(folder)) {
      if (!entry.exists(error_code) || error_code) {
        continue;
      }

      if (entry.is_directory(error_code)) {
        if (error_code) {
          continue;
        }
        total_size += FsUtil::folder_size(entry.path());
      }

      if (entry.is_regular_file(error_code) && !error_code) {
        total_size += FsUtil::file_size_for_path(entry.path());
      }
    }
  } catch (std::exception& ex) {
    OMS_STREAM_ERROR << ex.what();
  }
  return total_size;
}

FsUtil::disk_info FsUtil::space(const std::string& path)
{
  disk_info disk_info{};
  try {
    std::error_code error_code;
    fs::space_info si = fs::space(path, error_code);
    if (error_code) {
      OMS_ERROR("Failed to calculate disk size{} for {}", error_code.message(), path);
      return disk_info;
    }
    disk_info.available = si.available;
    disk_info.free = si.free;
    disk_info.capacity = si.capacity;
  } catch (std::filesystem::filesystem_error const& ex) {
    OMS_ERROR(ex.code().message());
  }
  return disk_info;
}

uint64_t FsUtil::file_size_for_path(const std::filesystem::path& path)
{
  std::error_code error_code;
  uint64_t file_size = fs::file_size(path, error_code);
  if (error_code) {
    OMS_ERROR("Failed to calculate file size ", error_code.message());
    return 0;
  }
  return file_size;
}

int FsUtil::write_lines(const std::string& filename, const std::vector<std::string>& lines)
{
  std::ofstream file(filename);
  if (!file.is_open()) {
    OMS_ERROR("Failed to open file:{}", filename);
    return OMS_FAILED;
  }
  for (const auto& item : lines) {
    file << item << std::endl;
  }
  file.close();
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
