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

#include <iterator>
#include <utility>
#include "binlog_index.h"
#include "log.h"
#include "str.h"
#include "data_type.h"
#include "guard.hpp"
#include "file_lock.h"
#include "ob_log_event.h"

namespace oceanbase {
namespace logproxy {
namespace fs = std::filesystem;

static void fetch_index_record(FILE* fp, BinlogIndexRecord& record)
{
  char* line = nullptr;
  size_t len = 0;
  getline(&line, &len, fp);
  record.parse(line);
  free(line);
}

static int fetch_index_vector(
    FILE* fp, std::vector<BinlogIndexRecord*>& index_records, const std::string& last_purged_file)
{
  if (fp != nullptr) {
    uint64_t index = atoll(last_purged_file.c_str());
    bool purged = !last_purged_file.empty();
    char* buffer = nullptr;
    size_t size = 0;
    while ((getline(&buffer, &size, fp)) != -1) {
      auto* record = new BinlogIndexRecord();
      record->parse(buffer);
      if (record->_index > 0 && (!purged || index < record->_index)) {
        index_records.emplace_back(record);
      } else {
        if (purged && index < record->_index) {
          purged = false;
        }
        delete (record);
      }
      free(buffer);
      buffer = nullptr;
    }
    free(buffer);
    return OMS_OK;
  } else {
    return OMS_FAILED;
  }
}

/*!
 * \brief Lock the index.LOCK file and obtain binlog index operation rights
 * \param fp
 * \return Whether acquiring the lock was successful
 */
static int binlog_index_lock(FILE*& fp, const std::string& path)
{
  fs::path data_path = path;
  std::string parent_path = data_path.parent_path().c_str();
  if (fp != nullptr) {
    FsUtil::fclose_binary(fp);
  }
  std::string index_lock_file = parent_path + "/" + INDEX_LOCK_FILE;
  fp = FsUtil::fopen_binary(index_lock_file, "a+");
  if (fp == nullptr) {
    OMS_ERROR("Failed to open locked file:{}", index_lock_file);
    return OMS_FAILED;
  }

  int fd = fileno(fp);
  if (OMS_PROC_WRLOCK(fd) == -1) {
    OMS_ERROR("Failed to lock file:{}", index_lock_file);
    return OMS_FAILED;
  }
  return OMS_OK;
}

/*!
 * \brief UnLock the index.LOCK file
 * \param fp
 * \return Is unlocking successful?
 */
int binlog_index_unlock(FILE*& fp)
{
  defer(FsUtil::fclose_binary(fp));
  if (fp == nullptr) {
    OMS_ERROR("Failed to open locked file");
    return OMS_FAILED;
  }

  int fd = fileno(fp);
  if (OMS_PROC_UNLOCK(fd) == -1) {
    OMS_ERROR("Failed to unlock file:{}", INDEX_LOCK_FILE);
    return OMS_FAILED;
  }
  return OMS_OK;
}

int fetch_index_vector(const std::string& index_file_name, std::vector<BinlogIndexRecord*>& index_records, bool lock)
{
  std::vector<std::string> purged_files;
  fs::path data_path = index_file_name;
  std::string purge_file_path = data_path.parent_path().string() + "/" + BINLOG_PURGED_NAME;

  if (!FsUtil::exist(purge_file_path)) {
    std::ofstream file(purge_file_path);
    if (!file) {
      OMS_ERROR("Failed to create file :{}", purge_file_path);
      return OMS_FAILED;
    }
    file.close();
  }
  if (!FsUtil::read_lines(purge_file_path, purged_files)) {
    OMS_ERROR("Failed to open file:{}", purge_file_path);
    return OMS_FAILED;
  }
  std::string last_purged_file;
  if (!purged_files.empty()) {
    last_purged_file = purged_files.back();
  }
  /*
   * Locked read
   */
  if (lock) {
    FILE* lock_fp = nullptr;
    defer(binlog_index_unlock(lock_fp));
    if (binlog_index_lock(lock_fp, index_file_name) == OMS_FAILED) {
      OMS_ERROR("Failed to lock file:{}", INDEX_LOCK_FILE);
      return OMS_FAILED;
    }
    FILE* fp = FsUtil::fopen_binary(index_file_name, "r+");
    defer(FsUtil::fclose_binary(fp));
    return fetch_index_vector(fp, index_records, last_purged_file);
  }

  FILE* fp = FsUtil::fopen_binary(index_file_name, "r+");
  defer(FsUtil::fclose_binary(fp));
  return fetch_index_vector(fp, index_records, last_purged_file);
}

int add_index(const std::string& index_file_name, const BinlogIndexRecord& record)
{
  FILE* lock_fp = nullptr;
  defer(binlog_index_unlock(lock_fp));
  if (binlog_index_lock(lock_fp, index_file_name) == OMS_FAILED) {
    OMS_ERROR("Failed to lock file:{}", INDEX_LOCK_FILE);
    return OMS_FAILED;
  }
  FILE* fp = FsUtil::fopen_binary(index_file_name);
  defer(FsUtil::fclose_binary(fp));
  if (fp == nullptr) {
    OMS_ERROR("Failed to open file:{},reason:{}", index_file_name, logproxy::system_err(errno));
    return OMS_FAILED;
  }
  auto ret = FsUtil::append_file(fp, (unsigned char*)record.to_string().c_str(), record.to_string().size());
  if (ret == OMS_FAILED) {
    OMS_ERROR("Failed to add index file:{}", binlog::CommonUtils::fill_binlog_file_name(record._index));
    return OMS_FAILED;
  }
  OMS_INFO("add binlog index file:{} value:{}", record._file_name, record.to_string());
  return OMS_OK;
}

std::string get_mapping_str(const std::pair<std::string, int64_t>& mapping)
{
  return mapping.first + "=" + std::to_string(mapping.second);
}

std::pair<std::string, int64_t> parse_mapping_str(const std::string& mapping_str)
{
  std::pair<std::string, int64_t> mapping_pair;
  if (mapping_str.empty()) {
    return mapping_pair;
  }
  unsigned long pos = mapping_str.find('=');
  mapping_pair.first = mapping_str.substr(0, pos);
  mapping_pair.second = std::stol(mapping_str.substr(pos + 1, mapping_str.size()));
  return mapping_pair;
}

bool is_active(const std::string& file, std::vector<BinlogIndexRecord*>& index_records)
{
  return index_records.back()->_file_name == file || index_records.back()->_file_name.empty();
}

/**
 * get the position of row pos at the end
 * @param pos
 * @param fstream
 */
void seekg_index(size_t& pos, FILE* fp)
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
}

int update_index(const std::string& index_file_name, const BinlogIndexRecord& record, size_t pos)
{
  FILE* lock_fp = nullptr;
  defer(binlog_index_unlock(lock_fp));
  if (binlog_index_lock(lock_fp, index_file_name) == OMS_FAILED) {
    OMS_ERROR("Failed to lock file:{}", INDEX_LOCK_FILE);
    return OMS_FAILED;
  }
  std::string temp = index_file_name + ".tmp";
  std::error_code err;
  std::vector<std::string> records;
  if (!FsUtil::read_lines(index_file_name, records)) {
    OMS_ERROR("Failed to read index file");
    return OMS_FAILED;
  }

  // Find the last row of records and overwrite it
  BinlogIndexRecord last_record;
  last_record.parse(records.back());

  if (std::equal(last_record._file_name.begin(), last_record._file_name.end(), record._file_name.c_str())) {
    records.pop_back();
    records.emplace_back(record.serialize());
  } else {
    OMS_ERROR("The latest binlog index file:{} is not the latest file record in the current memory:{}",
        binlog::CommonUtils::fill_binlog_file_name(last_record._index),
        binlog::CommonUtils::fill_binlog_file_name(record._index));
    records.emplace_back(record.serialize());
  }

  if (FsUtil::write_lines(temp, records)) {
    OMS_ERROR("Failed to write to file:{}", temp);
    return OMS_FAILED;
  }
  fs::rename(temp, index_file_name, err);
  if (err) {
    OMS_ERROR("Failed to rename file:{} to {},reason:{}", temp, index_file_name, logproxy::system_err(errno));
    return OMS_FAILED;
  }
  return OMS_OK;
}

int get_index(const std::string& index_file_name, BinlogIndexRecord& record, size_t index, const std::string& base_path)
{
  FILE* lock_fp = nullptr;
  defer(binlog_index_unlock(lock_fp));
  if (binlog_index_lock(lock_fp, index_file_name) == OMS_FAILED) {
    OMS_ERROR("Failed to lock file:{}", INDEX_LOCK_FILE);
    return OMS_FAILED;
  }
  FILE* fp = FsUtil::fopen_binary(index_file_name, "r+");
  defer(FsUtil::fclose_binary(fp));
  if (fp != nullptr) {
    seekg_index(index, fp);
    fetch_index_record(fp, record);
    OMS_DEBUG("get binlog index file:{} value:{}", record._file_name, record.to_string());
  } else {
    OMS_ERROR("Failed to open binlog index file:{},reason:{}", index_file_name, logproxy::system_err(errno));
    return OMS_FAILED;
  }
  return OMS_OK;
}

uint64_t convert_ts(const std::string& before_purge_ts)
{
  IDate date = str_2_idate(before_purge_ts);
  tm tt;
  tt.tm_year = date.year - 1900;
  tt.tm_mon = date.month - 1;
  tt.tm_mday = date.day;
  tt.tm_hour = date.hour;
  tt.tm_min = date.minute;
  tt.tm_sec = date.second;
  tt.tm_isdst = 0;
  auto time_c = mktime(&tt);
  uint64_t purge_ts = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::from_time_t(time_c).time_since_epoch())
                          .count();
  return purge_ts;
}

int purge_binlog_before_ts(const std::string& before_purge_ts, std::vector<BinlogIndexRecord*>& index_records,
    std::string& error_msg, std::vector<std::string>& purge_binlog_files, uint64_t& last_purged_index)
{
  std::vector<BinlogIndexRecord*>::iterator it;
  if (before_purge_ts.empty()) {
    error_msg = "The specified cleanup time cannot be empty";
    OMS_ERROR(error_msg);
    return OMS_FAILED;
  } else {
    // Convert the clean binlog log time to time stamp without time zone
    uint64_t purge_ts = convert_ts(before_purge_ts);

    if (!index_records.empty()) {
      for (it = index_records.begin(); it != index_records.end();) {
        // The binlog file being written cannot be deleted
        if ((*it)->get_checkpoint() < purge_ts && !is_active((*it)->get_file_name(), index_records)) {
          // purge
          purge_binlog_files.emplace_back((*it)->_file_name);
          last_purged_index = (*it)->_index;
          delete *it;
          it = index_records.erase(it);
        } else {
          ++it;
        }
      }
    }
  }
  return OMS_OK;
}

int purge_binlog_to_file(const std::string& to_binlog_index, std::string& error_msg,
    std::vector<BinlogIndexRecord*>& index_records, std::vector<std::string>& purge_binlog_files,
    uint64_t& last_purged_index)
{
  int ret = OMS_OK;
  std::vector<BinlogIndexRecord*>::reverse_iterator rit;
  bool found = false;
  for (rit = index_records.rbegin(); rit != index_records.rend();) {
    if (std::equal((*rit)->get_file_name().begin(), (*rit)->get_file_name().end(), to_binlog_index.c_str())) {
      found = true;
      if (!is_active((*rit)->get_file_name(), index_records)) {
        last_purged_index = (*rit)->_index;
      }
    }
    // The binlog file being written cannot be deleted
    if (found && !is_active((*rit)->get_file_name(), index_records)) {
      purge_binlog_files.emplace_back((*rit)->_file_name);
      delete *rit;
      rit = std::vector<BinlogIndexRecord*>::reverse_iterator(index_records.erase((++rit).base()));
    } else {
      ++rit;
    }
  }
  if (!found) {
    error_msg = "Failed to find file: " + to_binlog_index;
    OMS_ERROR(error_msg);
    ret = OMS_FAILED;
  }
  return ret;
}

int rewrite_index_file(
    const std::string& index_file_name, std::string& error_msg, std::vector<BinlogIndexRecord*>& index_records)
{
  std::vector<BinlogIndexRecord*>::iterator it;
  std::string temp = index_file_name + ".remove";
  std::stringstream records;
  std::error_code err;
  for (it = index_records.begin(); it != index_records.end(); ++it) {
    records << (*it)->to_string();
  }

  FILE* fp_temp = FsUtil::fopen_binary(temp);
  defer(FsUtil::fclose_binary(fp_temp));
  if (fp_temp != nullptr) {
    std::string records_str = records.str();
    FsUtil::append_file(fp_temp, records_str, records_str.size());
    fs::rename(temp, index_file_name, err);
    if (err) {
      OMS_ERROR("Failed to rename file {} to {},reason:{}", temp, index_file_name, logproxy::system_err(errno));
      return OMS_FAILED;
    }
  } else {
    error_msg = "Failed to open file:" + temp;
    OMS_ERROR(error_msg);
    return OMS_FAILED;
  }
  return OMS_OK;
}

int purge_binlog_index(const std::string& base_name, const std::string& binlog_file, const std::string& before_purge_ts,
    std::string& error_msg, std::vector<std::string>& purge_binlog_files)
{
  std::vector<BinlogIndexRecord*> index_records;
  std::string index_file_path = base_name + BINLOG_INDEX_NAME;

  FILE* lock_fp = nullptr;
  defer(binlog_index_unlock(lock_fp));
  if (binlog_index_lock(lock_fp, index_file_path) == OMS_FAILED) {
    OMS_ERROR("Failed to lock file:{}", index_file_path);
    return OMS_FAILED;
  }

  FILE* fp = FsUtil::fopen_binary(index_file_path, "r+");
  defer(FsUtil::fclose_binary(fp));
  if (fp == nullptr) {
    error_msg = "Failed to open file:" + index_file_path;
    OMS_STREAM_ERROR << error_msg;
    return OMS_FAILED;
  }
  int ret = OMS_OK;
  fetch_index_vector(index_file_path, index_records, false);
  defer(release_vector(index_records));
  uint64_t last_purged_index = 0;
  if (binlog_file.empty()) {
    ret = purge_binlog_before_ts(before_purge_ts, index_records, error_msg, purge_binlog_files, last_purged_index);
  } else {
    ret =
        purge_binlog_to_file(base_name + binlog_file, error_msg, index_records, purge_binlog_files, last_purged_index);
  }

  /*!
   * Only record the binlog file in the purged file. Where does it need to be cleaned ?
   */
  if (ret == OMS_OK && last_purged_index != 0) {
    std::string purge_file_path = base_name + BINLOG_PURGED_NAME;
    ret = FsUtil::write_file(purge_file_path, std::to_string(last_purged_index));
    if (ret != OMS_OK) {
      OMS_ERROR("Failed to record the log cleanup event");
      return ret;
    }
  }
  return ret;
}

int merge_binlog_index(const std::string& binlog_index_file)
{
  FILE* lock_fp = nullptr;
  defer(binlog_index_unlock(lock_fp));
  if (binlog_index_lock(lock_fp, binlog_index_file) == OMS_FAILED) {
    OMS_ERROR("Failed to lock file:{}", binlog_index_file);
    return OMS_FAILED;
  }
  std::vector<BinlogIndexRecord*> index_records;
  defer(release_vector(index_records));
  int ret = fetch_index_vector(binlog_index_file, index_records, false);
  if (ret != OMS_OK || index_records.empty()) {
    OMS_ERROR("Failed to merge binlog index");
    return OMS_FAILED;
  }
  /*!
   * Overwrite mysql index file
   */
  std::string temp = binlog_index_file + ".tmp";
  std::error_code err;
  std::vector<std::string> records;
  for (const auto* item : index_records) {
    records.push_back(item->serialize());
  }
  if (FsUtil::write_lines(temp, records)) {
    OMS_ERROR("Failed to write to file:{}", temp);
    return OMS_FAILED;
  }
  fs::rename(temp, binlog_index_file, err);
  if (err) {
    OMS_ERROR("Failed to rename file:{} to {},reason:{}", temp, binlog_index_file, logproxy::system_err(errno));
    return OMS_FAILED;
  }

  return OMS_OK;
}

BinlogIndexRecord::BinlogIndexRecord(const std::string& file_name, int index) : _file_name(file_name), _index(index)
{}

BinlogIndexRecord::~BinlogIndexRecord()
{}

const std::pair<std::string, uint64_t>& BinlogIndexRecord::get_before_mapping() const
{
  return _before_mapping;
}

void BinlogIndexRecord::set_before_mapping(const std::pair<std::string, uint64_t>& before_mapping)
{
  BinlogIndexRecord::_before_mapping = before_mapping;
}

const std::pair<std::string, uint64_t>& BinlogIndexRecord::get_current_mapping() const
{
  return _current_mapping;
}

void BinlogIndexRecord::set_current_mapping(const std::pair<std::string, uint64_t>& current_mapping)
{
  BinlogIndexRecord::_current_mapping = current_mapping;
}

const std::string& BinlogIndexRecord::get_file_name() const
{
  return _file_name;
}

void BinlogIndexRecord::set_file_name(const std::string& file_name)
{
  _file_name = file_name;
}

uint64_t BinlogIndexRecord::get_index() const
{
  return _index;
}

void BinlogIndexRecord::set_index(uint64_t index)
{
  _index = index;
}

uint64_t BinlogIndexRecord::get_checkpoint() const
{
  return _checkpoint;
}

void BinlogIndexRecord::set_checkpoint(uint64_t checkpoint)
{
  _checkpoint = checkpoint;
}

uint64_t BinlogIndexRecord::get_position() const
{
  return _position;
}

void BinlogIndexRecord::set_position(uint64_t position)
{
  _position = position;
}

std::string BinlogIndexRecord::to_string() const
{
  std::stringstream str;
  str << _file_name << "\t" << _index << "\t" << get_mapping_str(_current_mapping) << "\t"
      << get_mapping_str(_before_mapping) << "\t" << _checkpoint << "\t" << _position << std::endl;
  return str.str();
}

void BinlogIndexRecord::parse(const std::string& content)
{
  std::vector<std::string> set;
  split(content, '\t', set);
  if (set.size() == 6) {
    this->_file_name = set.at(0);
    this->_index = std::stol(set.at(1));
    this->_current_mapping = parse_mapping_str(set.at(2));
    this->_before_mapping = parse_mapping_str(set.at(3));
    this->_checkpoint = std::stol(set.at(4));
    this->_position = std::stol(set.at(5));
  } else {
    OMS_WARN("Failed to fetch index record,value:{} set size:{}", content, set.size());
  }
}

std::string BinlogIndexRecord::serialize() const
{
  std::stringstream str;
  str << _file_name << "\t" << _index << "\t" << get_mapping_str(_current_mapping) << "\t"
      << get_mapping_str(_before_mapping) << "\t" << _checkpoint << "\t" << _position;
  return str.str();
}

}  // namespace logproxy
}  // namespace oceanbase
