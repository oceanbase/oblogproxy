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

#include <mutex>
#include <condition_variable>
#include <sys/poll.h>
#include "binlog_dumper.h"
#include "binlog_index.h"
#include "timer.h"
#include "config.h"
#include "guard.hpp"
#include "common_util.h"
#include "counter.h"
namespace oceanbase {
namespace logproxy {
void BinlogDumper::stop()
{
  if (is_run()) {
    Thread::stop();
  }
}

void BinlogDumper::run()
{
  /*!
   * @brief 注册
   */
  if (Config::instance().metric_enable.val()) {
    register_latency();
  }

  if (is_gtid_mod()) {
    if (seek_first_binlog_file() != OMS_OK) {
      // In GTID mode, the binlog file that meets the conditions cannot be found
      return;
    }
  }

  /*!
   * @brief Initialize the checksum parameter of the subscription,
   * which is only valid for the fist one fake rotate event
   */
  init_binlog_checksum();

  if (!_relative_file.empty()) {
    if (!verify_subscription_offset(_start_pos)) {
      OMS_ERROR("{}: The subscription offset [{},{}] is illegal", _connection->trace_id(), _relative_file, _start_pos);
      return;
    } else {
      OMS_INFO("{}:Subscription offset [{},{}] verification passed, start subscribing",
          _connection->trace_id(),
          _relative_file,
          _start_pos);
    }
  }

  while (is_run()) {
    OMS_INFO("{}: Begin send fake rotate event", _connection->trace_id());
    if (_relative_file.empty()) {
      vector<BinlogIndexRecord*> index_records;
      defer(release_vector(index_records));
      fetch_index_vector(_meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_records);
      if (!index_records.empty()) {
        set_file(binlog::CommonUtils::fill_binlog_file_name(index_records.front()->_index));
        OMS_INFO("{}: Find the first binlog file that is not included in the executed gtid,binlog file:{}",
            _connection->trace_id(),
            binlog::CommonUtils::fill_binlog_file_name(index_records.front()->_index));
      }
    }
    // 1.send fake rotate event
    if (send_fake_rotate_event(_relative_file, _start_pos) != IoResult::SUCCESS) {
      OMS_ERROR("{}: {}", _connection->trace_id(), "Failed to send fake rotate event");
      break;
    }

    if (_start_pos < BINLOG_MAGIC_SIZE) {
      OMS_ERROR("{}: The file offset is invalid.", _connection->trace_id());
      binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
          "Client requested master to start replication \"\n"
          "                    \"from position < 4.",
          "HY000"};
      _connection->send(error_packet);
      break;
    }

    // 2. check binlog file
    unsigned char magic[BINLOG_MAGIC_SIZE];
    OMS_INFO("{}: Start open binlog file: {}", _connection->trace_id(), _file.c_str());
    if (this->_fp != nullptr) {
      fclose(this->_fp);
      this->_fp = nullptr;
    }
    this->_fp = fopen(_file.c_str(), "rb+");
    if (this->_fp == nullptr) {
      OMS_ERROR(
          "{}: Failed to open binlog file: {},reason:{}", _connection->trace_id(), _file, logproxy::system_err(errno));
      binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR, "failed to open binlog file", "HY000"};
      _connection->send(error_packet);
      break;
    }
    // read magic number
    FsUtil::read_file(this->_fp, magic, 0, sizeof(magic));

    if (memcmp(magic, binlog_magic, sizeof(magic)) != 0) {
      OMS_ERROR("{}: The file format is invalid.", _connection->trace_id());
      binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
          "Binlog has bad magic number;  It's not a binary log file that can be used by this version of MySQL.",
          "HY000"};
      _connection->send(error_packet);
      break;
    }
    _checkpoint.first = _file;
    _checkpoint.second = _start_pos;
    // 3.send binlog file
    if (send_binlog(_checkpoint.first, _checkpoint.second) != IoResult::SUCCESS) {
      OMS_ERROR("{}: Failed to send binlog:{}", _connection->trace_id(), _relative_file);
      return;
    }

    if (_flag == BINLOG_DUMP_NON_BLOCK) {
      OMS_INFO("{}: Send eof packet", _connection->trace_id());
      // Clear and send cached data
      send_packet();
      _connection->send_eof_packet();
      break;
    }

    // 4. rotate binlog file
    if (seek_next_binlog() != OMS_OK) {
      binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR, "could not find next log.", "HY000"};
      _connection->send(error_packet);
      break;
    }
    _file = _checkpoint.first;
    _start_pos = BINLOG_MAGIC_SIZE;
  }
}

int BinlogDumper::seek_first_binlog_file()
{
  OMS_INFO("{}: Subscribe to BinLog using GTID mode", _connection->trace_id());
  // find all binlog file
  vector<BinlogIndexRecord*> index_records;
  fetch_index_vector(_meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_records);

  if (_exclude_gtid.empty()) {
    if (!index_records.empty()) {
      set_file(binlog::CommonUtils::fill_binlog_file_name(index_records.front()->_index));
      OMS_INFO("{}: Find the first binlog file that is not included in the executed gtid,binlog file:{}",
          _connection->trace_id(),
          binlog::CommonUtils::fill_binlog_file_name(index_records.front()->_index));
    }
  } else {
    bool is_subset = true;
    // Traverse the index records in reverse order, because when subscribing, need to search one by one from the latest
    // binlog file until find the binlog file included in the executed gtid collection.
    for (auto p_index_record = index_records.rbegin(); p_index_record != index_records.rend(); ++p_index_record) {
      vector<ObLogEvent*> binlog_events;
      seek_events((*p_index_record)->_file_name, binlog_events, PREVIOUS_GTIDS_LOG_EVENT, true);
      if (binlog_events.empty()) {
        // For the Binlog service, Gtid-related information must exist, otherwise it means that there is a problem
        // with the generated Binlog file
        OMS_ERROR("{}: Illegal Binlog file:{},because PREVIOUS_GTIDS_LOG_EVENT does not exist",
            _connection->trace_id(),
            (*p_index_record)->_file_name);
        binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
            "The requested gtid transaction no longer exists and may have been cleaned up",
            "HY000"};
        _connection->send(error_packet);
        release_vector(binlog_events);
        release_vector(index_records);
        return OMS_FAILED;
      }
      auto* previous_gtids_log_event = dynamic_cast<PreviousGtidsLogEvent*>(binlog_events.at(0));
      map<string, GtidMessage*> gtids = previous_gtids_log_event->get_gtid_messages();
      is_subset_executed_gtid(gtids, is_subset, (*p_index_record));
      release_vector(binlog_events);
      if (is_subset) {
        OMS_INFO("{}: Find the first binlog file that is not included in the executed gtid,binlog file:{}",
            _connection->trace_id(),
            binlog::CommonUtils::fill_binlog_file_name((*p_index_record)->_index));
        set_file(binlog::CommonUtils::fill_binlog_file_name((*p_index_record)->_index));
        break;
      }
    }
  }

  release_vector(index_records);
  if (get_relative_file().empty()) {
    OMS_ERROR(
        "{}:The requested gtid transaction no longer exists and may have been cleaned up", _connection->trace_id());
    binlog::ErrPacket error_packet{
        BINLOG_FATAL_ERROR, "The requested gtid transaction no longer exists and may have been cleaned up", "HY000"};
    _connection->send(error_packet);
    return OMS_FAILED;
  }
  return OMS_OK;
}

void BinlogDumper::is_subset_executed_gtid(
    map<std::string, GtidMessage*>& gtids, bool& is_subset, BinlogIndexRecord* p_index_record)
{
  for (auto& gtid : gtids) {
    GtidMessage* gtid_message = gtid.second;
    if (_exclude_gtid.find(gtid.first) == _exclude_gtid.end()) {
      OMS_INFO("{}: The current gtid {} is not included in the exclude gtid", _connection->trace_id(), gtid.first);
      is_subset = false;
      continue;
    }

    GtidMessage* message = _exclude_gtid.find(gtid.first)->second;
    // When it is an initialized binlog file, the txn range is empty, and we think that the initial Binlog file
    // has been found
    if (gtid_message->get_txn_range().empty()) {
      OMS_INFO("{}: When it is an initialized binlog file, the txn range is empty, and we think that the "
               "initial Binlog file has been found ",
          _connection->trace_id());
      is_subset = true;
      break;
    }
    //  txn range
    for (const auto& txn_range : gtid_message->get_txn_range()) {
      OMS_STREAM_DEBUG << "executed gtid:[" << txn_range.first << "," << txn_range.second << ")";
      bool include = false;
      for (const auto& exclude_txn : message->get_txn_range()) {
        OMS_STREAM_DEBUG << ""
                         << "request gtid:[" << exclude_txn.first << "," << exclude_txn.second << ")";
        if (txn_range.first >= exclude_txn.first && txn_range.second <= exclude_txn.second) {
          include = true;
          break;
        }
      }
      if (!include) {
        OMS_INFO("{}: The currently executed gtid does not contain the gtid up to the current binlog file, so it "
                 "needs to look forward,binlog file:{}",
            _connection->trace_id(),
            binlog::CommonUtils::fill_binlog_file_name(p_index_record->_index));
        is_subset = false;
        break;
      } else {
        is_subset = true;
        break;
      }
    }
    if (!is_subset) {
      break;
    }
  }
}

int BinlogDumper::seek_next_binlog()
{
  vector<BinlogIndexRecord*> index_records;
  fetch_index_vector(_meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_records);

  if (index_records.empty()) {
    OMS_ERROR("{}: Failed to fetch binlog indexes", _connection->trace_id());
    _error_message = "Failed to fetch binlog indexes";
    return OMS_FAILED;
  }

  bool found = false;
  for (const auto& index : index_records) {
    OMS_DEBUG("{} :index file name:{}  file:", _connection->trace_id(), index->_file_name, _file);
    if (found) {
      _checkpoint.first = index->_file_name;
      _relative_file = binlog::CommonUtils::fill_binlog_file_name(index->_index);
      _checkpoint.second = BINLOG_MAGIC_SIZE;
      wait_rotate_ready(_checkpoint.first);
      break;
    }
    if (strcmp(index->_file_name.c_str(), _file.c_str()) == 0) {
      found = true;
    }
  }
  release_vector(index_records);

  if (!found) {
    std::stringstream message;
    message << "Failed to find binlog file:" << _file
            << " in binlog index:" << _meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME;
    OMS_ERROR("{}: {}", _connection->trace_id(), message.str());
    _error_message = message.str();
    return OMS_FAILED;
  }
  return OMS_OK;
}

IoResult BinlogDumper::send_fake_rotate_event(const std::string& file_name, uint64_t start_pos)
{
  RotateEvent rotate_event = RotateEvent(0, file_name, 0, 0);
  rotate_event.set_next_binlog_position(start_pos);
  rotate_event.get_header()->set_next_position(0);
  rotate_event.get_header()->set_flags(32);

  /*!
   * @brief When the downstream checksum variable is set in the session variable, the session variable shall prevail.
   * Remove checksum 4 digits
   */
  if (get_binlog_checksum() != CRC32) {
    rotate_event.get_header()->set_event_length(
        rotate_event.get_header()->get_event_length() - rotate_event.get_checksum_len());
    rotate_event.set_checksum_flag(get_binlog_checksum());
  }
  rotate_event.get_header()->set_event_length(rotate_event.get_header()->get_event_length());
  OMS_INFO("{}: send events of binlog file: {}  rotate event len: {}",
      _connection->trace_id(),
      rotate_event.get_next_binlog_file_name(),
      rotate_event.get_header()->get_event_length());
  char* buff = static_cast<char*>(malloc(rotate_event.get_header()->get_event_length() + 1));
  int1store(reinterpret_cast<unsigned char*>(buff), 0);
  rotate_event.flush_to_buff(reinterpret_cast<unsigned char*>(buff + 1));

  uint32_t data_len = rotate_event.get_header()->get_event_length() + 1;

  IoResult ret = _connection->send_binlog_event(reinterpret_cast<const uint8_t*>(buff), data_len);
  free(buff);
  return ret;
}

IoResult BinlogDumper::send_packet()
{
  if (_packet.byte_size() <= 0) {
    return IoResult::SUCCESS;
  }
  IoResult ret;
  for (const auto& iter : _packet) {
    ret = _connection->send_binlog_event(reinterpret_cast<const uint8_t*>(iter.buffer()), iter.size());
    if (ret != IoResult::SUCCESS) {
      OMS_ERROR("{}: Failed to send packet, errno:{}, error:", _connection->trace_id(), errno, strerror(errno));
      break;
    }
  }
  _packet.reset();
  return ret;
}

IoResult BinlogDumper::send_heartbeat_event(uint64_t pos)
{
  HeartbeatEvent heartbeat_event = HeartbeatEvent(_checkpoint.first, pos);
  heartbeat_event.get_header()->set_timestamp(_checkpoint_ts);
  char* buff = static_cast<char*>(malloc(heartbeat_event.get_header()->get_event_length() + 1));
  FreeGuard<char*> free_guard(buff);
  int1store(reinterpret_cast<unsigned char*>(buff), 0);
  heartbeat_event.flush_to_buff(reinterpret_cast<unsigned char*>(buff + 1));
  OMS_DEBUG("{}: Send heartbeat event,binlog file:{},offset:{}",
      this->_connection->trace_id(),
      heartbeat_event.get_binlog_file_name(),
      heartbeat_event.get_header()->get_next_position());
  mark_metrics(heartbeat_event.get_header()->get_timestamp(), heartbeat_event.get_header()->get_event_length());
  return _connection->send_binlog_event(
      reinterpret_cast<const uint8_t*>(buff), heartbeat_event.get_header()->get_event_length() + 1);
}

IoResult BinlogDumper::send_format_description_event(FILE* stream, const std::string& file)
{
  if (get_binlog_checksum() == UNDEF && Config::instance().binlog_checksum.val()) {
    this->_connection->send_err_packet(BINLOG_FATAL_ERROR,
        "Slave can not handle replication events with the checksum that master is configured to log",
        "HY000");
    return IoResult::FAIL;
  }
  _checkpoint.second = BINLOG_MAGIC_SIZE;
  bool skip_record = false;
  int ret = seek_event(stream, _packet, skip_record);
  if (ret != FORMAT_DESCRIPTION_EVENT) {
    return IoResult::FAIL;
  }
  return send_packet();
}

int BinlogDumper::seek_event(FILE* stream, MsgBuf& msg_buf, bool& skip_record)
{
  unsigned char buff[COMMON_HEADER_LENGTH];
  // read common header
  size_t ret = FsUtil::read_file(stream, buff, _checkpoint.second, COMMON_HEADER_LENGTH);
  if (ret != OMS_OK) {
    OMS_ERROR("{}: Failed to seek event header from offset:{}", _connection->trace_id(), _checkpoint.second);
    return OMS_FAILED;
  }
  OblogEventHeader header = OblogEventHeader();
  header.deserialize(buff);
  uint32_t event_len = header.get_event_length();

  auto* event_buf = static_cast<unsigned char*>(malloc(header.get_event_length() + 1));
  FreeGuard<unsigned char*> free_guard(event_buf);
  int1store(event_buf, 0);
  ret = FsUtil::read_file(stream, event_buf + 1, _checkpoint.second, event_len);
  if (ret != OMS_OK) {
    OMS_ERROR("{}: Failed to seek event from offset:{}", _connection->trace_id(), _checkpoint.second);
    return OMS_FAILED;
  }

  /*!
   * @brief For each FORMAT_DESCRIPTION_EVENT, the checksum parameter needs to be updated
   */
  if (header.get_type_code() == FORMAT_DESCRIPTION_EVENT) {
    FormatDescriptionEvent fd_event = FormatDescriptionEvent();
    fd_event.deserialize(event_buf + 1);
    set_binlog_checksum(static_cast<enum_checksum_flag>(fd_event.get_checksum_flag()));
  }
  skip_record = skip_event(header, event_buf, skip_record);
  _checkpoint.second += event_len;
  if (skip_record) {
    return UNKNOWN_EVENT;
  }
  msg_buf.push_back_copy(reinterpret_cast<char*>(event_buf), event_len + 1);
  /*!
   * @brief Record the latest location of the currently sent event
   */
  mark_metrics(header.get_timestamp(), event_len);
  return header.get_type_code();
}

int BinlogDumper::seek_binlog_end_pos(const std::string& file, uint64_t& end_pos)
{
  if (!is_active(file)) {
    uint64_t file_end_pos = FsUtil::file_size(file);
    /*!
     * @brief Only when the rotate file is sent we consider it has been sent.
     */
    if (_checkpoint.second == file_end_pos && strcmp(file.c_str(), _rotate_file.c_str()) == 0) {
      return OMS_BINLOG_SKIP;
    } else if (file_end_pos < _checkpoint.second) {
      /*
       * The subscribed site is incorrect and exceeds the maximum value of the BINLOG file.
       */
      OMS_ERROR("The subscribed site [{},{}] is incorrect and exceeds the maximum value {} of the BINLOG file.",
          file,
          _checkpoint.second,
          file_end_pos);
      return OMS_FAILED;
    }
    end_pos = file_end_pos;
    return OMS_OK;
  }

  uint64_t file_size = FsUtil::file_size(file);
  if (_checkpoint.second < file_size) {
    end_pos = file_size;
    return OMS_OK;
  }

  // BINLOG_DUMP_NON_BLOCK means that the current event will be terminated immediately after sending
  if (_flag == BINLOG_DUMP_NON_BLOCK) {
    send_packet();
    return OMS_BINLOG_SKIP;
  }

  return wait_event(file, _checkpoint.second, end_pos);
}

bool BinlogDumper::is_active(const std::string& file)
{
  BinlogIndexRecord index_record;
  auto ret = get_index(_meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);
  return ret != OMS_OK || index_record.get_index() == 0 ||
         strcmp(index_record.get_file_name().c_str(), file.c_str()) == 0 || index_record.get_file_name().empty();
}

int BinlogDumper::notify_new_event(const string& file)
{
  int fd = inotify_init();
  int wd = inotify_add_watch(fd, file.c_str(), IN_MODIFY);
  char* buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
  if (wd < 0) {
    cout << "inotify_add_watch failed\n";
    return OMS_FAILED;
  }
  while (is_run()) {
    struct pollfd pfd = {fd, POLLIN, 0};
    int ret = poll(&pfd, 1, 50);  // timeout of 50ms
    if (ret < 0) {
      OMS_STREAM_ERROR << "poll failed:" << strerror(errno);
      break;
    } else if (ret == 0) {
      // Timeout with no events, move on.
      OMS_STREAM_DEBUG << "No new files were detected";
    } else {
      // Process the new event.
      read(fd, buffer, sizeof(struct inotify_event));
      auto* event = (struct inotify_event*)buffer;
      if (event->mask & IN_MODIFY) {
        OMS_STREAM_INFO << "discover new events";
        break;
      }
      OMS_STREAM_INFO << "inotify event:" << event->mask;
    }
  }
  inotify_rm_watch(fd, wd);
  close(fd);
  return OMS_OK;
}

void notify(BinlogDumper& binlog_dumper, const std::string& file, std::condition_variable& condition)
{
  binlog_dumper.notify_new_event(file);
  condition.notify_one();
}

int BinlogDumper::wait_event(const std::string& file, uint64_t pos, uint64_t& new_pos)
{

  Timer timer;
  timer.reset();
  while (is_run()) {
    struct stat file_stat;
    int ret = stat(file.c_str(), &file_stat);
    uint64_t file_size = (ret == 0 ? file_stat.st_size : 0);
    if (file_size > pos) {
      new_pos = file_size;
      OMS_STREAM_DEBUG << "discover new events:" << new_pos;
      return OMS_OK;
    } else {
      if (get_heartbeat_period() != 0 && timer.elapsed() > (get_heartbeat_period() / 1000)) {
        if (send_heartbeat_event(pos) != IoResult::SUCCESS) {
          OMS_ERROR("Failed to send heartbeat:{}", _connection->trace_id());
          return OMS_FAILED;
        }
        if (!is_active(file)) {
          OMS_INFO("{}: The current file has been rotated and needs to be sent out of the heartbeat:{}",
              _connection->trace_id(),
              file);
          break;
        }
        timer.reset();
      }
    }
    usleep(1000);
  }
  return OMS_OK;
}

IoResult BinlogDumper::send_events(uint64_t start_pos, uint64_t end_pos)
{
  IoResult ret = IoResult::SUCCESS;
  bool skip_record = false;
  _checkpoint.second = start_pos;
  OMS_STREAM_DEBUG << "send events from offset:" << _checkpoint.second << " end pos:" << end_pos;
  while (_checkpoint.second < end_pos && is_legal_event(this->_fp, _checkpoint.second, end_pos)) {
    _stage_timer.reset();
    int result = seek_event(this->_fp, _packet, skip_record);

    if (result == OMS_FAILED) {
      return IoResult::FAIL;
    }

    if (result != UNKNOWN_EVENT && result != OMS_FAILED) {
      OMS_STREAM_DEBUG << "sending events,checkpoint:[" << _checkpoint.first << "," << _checkpoint.second << "]"
                       << "[buff size:" << _packet.byte_size() << "]";
      Counter::instance().count_write_io(_packet.byte_size());
      ret = send_packet();
      if (ret != IoResult::SUCCESS) {
        OMS_ERROR("{}: Failed to send packet", _connection->trace_id());
        return ret;
      }
      Counter::instance().count_write(1);
    }
    if (result == ROTATE_EVENT) {
      _rotate_file = _checkpoint.first;
      OMS_INFO("{}: sending rotate event,checkpoint:[{},{}]",
          _connection->trace_id(),
          _checkpoint.first,
          _checkpoint.second);
      break;
    }
  }
  return IoResult::SUCCESS;
}

IoResult BinlogDumper::send_binlog(const string& file, uint64_t start_pos)
{
  // 1.send format description event
  if (send_format_description_event(this->_fp, file) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  OMS_INFO("{}: Send format description event", _connection->trace_id());
  if (start_pos > _checkpoint.second) {
    _checkpoint.second = start_pos;
  }

  while (is_run()) {
    uint64_t end_pos = 0;
    auto ret = seek_binlog_end_pos(file, end_pos);
    if (ret == OMS_BINLOG_SKIP) {
      // the current binlog file has been sent
      OMS_INFO("{}: The current Binlog file has been sent,binlog file:{},offset:{}",
          _connection->trace_id(),
          file,
          _checkpoint.second);
      return IoResult::SUCCESS;
    } else if (ret == OMS_FAILED) {
      OMS_ERROR("The subscribed site [{},{}] is incorrect and exceeds the maximum value of the BINLOG file.",
          file,
          _checkpoint.second);
      return IoResult::FAIL;
    }

    OMS_INFO(
        "{}: binlog file:{},end pos:{},checkpoint pos:{}", _connection->trace_id(), file, end_pos, _checkpoint.second);
    if (send_events(_checkpoint.second, end_pos) != IoResult::SUCCESS) {
      OMS_ERROR("{}: Failed to send events", _connection->trace_id());
      return IoResult::FAIL;
    }

    // Try to detect whether the connection is alive
    // if the connection has been disconnected, can quickly find out
    if (conn_liveness() != OMS_OK) {
      OMS_ERROR("{} The downstream connection has been closed", _connection->trace_id());
      return IoResult::FAIL;
    }
  }

  return IoResult::SUCCESS;
}

int BinlogDumper::conn_liveness()
{
  if (send_heartbeat_event(_checkpoint.second) != IoResult::SUCCESS) {
    OMS_ERROR("Failed to send heartbeat:{}", _connection->trace_id());
    return OMS_FAILED;
  }
  return OMS_OK;
}

bool BinlogDumper::skip_event(OblogEventHeader& header, unsigned char* buff, bool skip_record)
{
  if (_gtid_mod && !_exclude_gtid.empty()) {
    switch (header.get_type_code()) {
      case ROTATE_EVENT:
        return false;
      case GTID_LOG_EVENT:
        return handle_gtid_event(buff);
      default:
        return skip_record;
    }
  }
  return false;
}

bool BinlogDumper::handle_gtid_event(unsigned char* buff)
{
  GtidLogEvent gtid_log_event = GtidLogEvent();
  gtid_log_event.deserialize(buff + 1);
  uint64_t g_no = gtid_log_event.get_gtid_txn_id();
  for (const auto& gtid : _exclude_gtid) {
    string exclude_uuid;
    dumphex(reinterpret_cast<const char*>(gtid.second->get_gtid_uuid()), SERVER_UUID_LEN, exclude_uuid);

    string uuid_current;
    dumphex(reinterpret_cast<const char*>(gtid_log_event.get_gtid_uuid()), SERVER_UUID_LEN, uuid_current);
    if (exclude_uuid == uuid_current) {
      vector<txn_range> txn = gtid.second->get_txn_range();
      for (const txn_range& range : txn) {
        OMS_STREAM_DEBUG << "exclude_uuid range[" << range.first << "," << range.second << "]"
                         << ", gno" << g_no;
        if (g_no < range.second && g_no >= range.first) {
          OMS_INFO("{}: Skip current gtid {} transaction exclude_uuid:{}",
              _connection->trace_id(),
              g_no,
              gtid.second->format_string());
          return true;
        }
      }
    }
  }
  return false;
}

const string& BinlogDumper::get_file() const
{
  return _file;
}

void BinlogDumper::set_file(string file)
{
  _file = _meta.log_bin_prefix + BINLOG_DATA_DIR + file;
  _relative_file = file;
}

uint64_t BinlogDumper::get_start_pos() const
{
  return _start_pos;
}

void BinlogDumper::set_start_pos(uint64_t start_pos)
{
  _start_pos = start_pos;
}

bool BinlogDumper::is_gtid_mod() const
{
  return _gtid_mod;
}

void BinlogDumper::set_gtid_mod(bool gtid_mod)
{
  _gtid_mod = gtid_mod;
}

const pair<std::string, uint64_t>& BinlogDumper::get_checkpoint() const
{
  return _checkpoint;
}

void BinlogDumper::set_checkpoint(pair<string, uint64_t> checkpoint)
{
  _checkpoint = checkpoint;
}

uint32_t BinlogDumper::get_flag() const
{
  return _flag;
}

void BinlogDumper::set_flag(uint32_t flag)
{
  _flag = flag;
}

uint64_t BinlogDumper::get_heartbeat_interval_us() const
{
  return _heartbeat_interval_us;
}

void BinlogDumper::set_heartbeat_interval_us(uint64_t heartbeat_interval_us)
{
  _heartbeat_interval_us = heartbeat_interval_us;
}

const ConvertMeta& BinlogDumper::get_meta() const
{
  return _meta;
}

void BinlogDumper::set_meta(ConvertMeta meta)
{
  _meta = std::move(meta);
}

BinlogDumper::BinlogDumper() : Thread("BinlogDumper")
{}

void BinlogDumper::register_latency()
{
  /*!
   * @brief Registration Latency Counter
   */
  _counter.register_gauge("DumperLatency:" + this->_connection->trace_id() + "[checkpoint",
      [this]() { return this->_metric.checkpoint(); });
  _counter.register_gauge("delay", [this]() { return this->_metric.delay(); });
  _counter.register_gauge("rps", [this]() { return this->_metric.rps(); });
  _counter.register_gauge("iops", [this]() { return this->_metric.iops(); });
  /*!
   * @brief Turn on indicator monitoring
   */
  _counter.start();
}

const std::map<std::string, GtidMessage*>& BinlogDumper::get_exclude_gtid() const
{
  return _exclude_gtid;
}

void BinlogDumper::set_exclude_gtid(std::map<std::string, GtidMessage*> exclude_gtid)
{
  _exclude_gtid = std::move(exclude_gtid);
}

binlog::Connection* BinlogDumper::get_connection() const
{
  return _connection;
}

void BinlogDumper::set_connection(binlog::Connection* connection)
{
  _connection = connection;
}

int BinlogDumper::send_events(
    const std::string& file, uint64_t start_pos, uint64_t end_pos, process_binlog_event process_func)
{
  int ret = OMS_OK;
  bool skip_record = false;
  _checkpoint.second = start_pos;

  FILE* fp = FsUtil::fopen_binary(file);
  if (fp == nullptr) {
    return OMS_FAILED;
  }
  MsgBuf msg_buf;
  OMS_STREAM_INFO << "send events from offset:" << _checkpoint.second << "end pos:" << end_pos;
  while (_checkpoint.second < end_pos) {
    seek_event(fp, msg_buf, skip_record);
    process_func(msg_buf, _connection);
  }
  return ret;
}

BinlogDumper::~BinlogDumper()
{
  if (_counter.is_run()) {
    _counter.stop();
  }

  if (_connection != nullptr) {
    OMS_INFO("Dumper starts to exit:{}", _connection->trace_id());
  }

  for (auto gtid_message : _exclude_gtid) {
    delete (gtid_message.second);
    gtid_message.second = nullptr;
  }

  delete _connection;
  _connection = nullptr;
  if (_fp != nullptr) {
    fclose(_fp);
    _fp = nullptr;
  }
}

const string& BinlogDumper::get_relative_file() const
{
  return _relative_file;
}

void BinlogDumper::set_relative_file(const string& relative_file)
{
  _relative_file = relative_file;
}

void BinlogDumper::mark_metrics(uint64_t ts, uint64_t bytes)
{
  _checkpoint_ts = ts;
  _metric.mark_checkpoint_ts(ts);
  _metric.count_send();
  _metric.count_send_io(bytes);
}

bool BinlogDumper::is_legal_event(FILE* stream, uint64_t offset, uint64_t end_pos) const
{
  if (offset + COMMON_HEADER_LENGTH > end_pos) {
    OMS_WARN("{}: The content of the current binlog event header is incomplete, and the expected file length is {},the "
             "actual value is {}",
        _connection->trace_id(),
        offset + COMMON_HEADER_LENGTH,
        end_pos);
    return false;
  }
  unsigned char buff[COMMON_HEADER_LENGTH];
  // read common header
  size_t ret = FsUtil::read_file(stream, buff, _checkpoint.second, COMMON_HEADER_LENGTH);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "failed to seek event header from offset:" << _checkpoint.second;
    return false;
  }
  OblogEventHeader header = OblogEventHeader();
  header.deserialize(buff);
  uint32_t event_len = header.get_event_length();
  if (offset + event_len > end_pos) {
    OMS_WARN("{}: The content of the current binlog event is incomplete, and the expected file length is {}",
        _connection->trace_id(),
        offset + event_len);
    return false;
  }
  return true;
}

bool BinlogDumper::verify_subscription_offset(uint64_t offset)
{

  /*
   * Verify whether the requested location is legal and intercept it in advance
   */

  auto fp = fopen(_file.c_str(), "rb+");
  if (fp == nullptr) {
    OMS_ERROR(
        "{}: Failed to open binlog file: {},reason:{}", _connection->trace_id(), _file, logproxy::system_err(errno));
    binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR, "failed to open binlog file", "HY000"};
    _connection->send(error_packet);
    return false;
  }
  defer(fclose(fp));
  uint64_t end_pos = FsUtil::file_size(_file);

  if (offset < BINLOG_MAGIC_SIZE) {
    OMS_ERROR("{}: The file offset is invalid.", _connection->trace_id());
    binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
        "Client requested master to start replication \"\n"
        "                    \"from position < 4.",
        "HY000"};
    _connection->send(error_packet);
    return false;
  }

  if (end_pos == offset) {
    // What is subscribed at this time happens to be the latest show master status offset,
    // and no verification is needed at this time.
    return true;
  }

  if (offset + COMMON_HEADER_LENGTH > end_pos) {
    OMS_WARN("{}: The content of the current binlog event header is incomplete, and the expected file length is {},the "
             "actual value is {}",
        _connection->trace_id(),
        offset + COMMON_HEADER_LENGTH,
        end_pos);
    binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
        "The offset of the current subscription is illegal,offset:" + std::to_string(offset),
        "HY000"};
    _connection->send(error_packet);
    return false;
  }
  unsigned char buff[COMMON_HEADER_LENGTH];
  // read common header
  size_t ret = FsUtil::read_file(fp, buff, offset, COMMON_HEADER_LENGTH);
  if (ret != OMS_OK) {
    binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
        "The offset of the current subscription is illegal,offset:" + std::to_string(offset),
        "HY000"};
    _connection->send(error_packet);
    return false;
  }
  OblogEventHeader header = OblogEventHeader();
  header.deserialize(buff);
  uint32_t event_len = header.get_event_length();
  if (offset + event_len > end_pos) {
    OMS_WARN("{}: The content of the current binlog event is incomplete, and the expected file length is {}",
        _connection->trace_id(),
        offset + event_len);
    binlog::ErrPacket error_packet{BINLOG_FATAL_ERROR,
        "The offset of the current subscription is illegal,offset:" + std::to_string(offset),
        "HY000"};
    _connection->send(error_packet);
    return false;
  } else {
    auto* event = static_cast<unsigned char*>(malloc(header.get_event_length()));
    FreeGuard<unsigned char*> free_guard(event);
    ret = FsUtil::read_file(fp, event, offset, header.get_event_length());
    if (ret != OMS_OK) {
      binlog::ErrPacket error_packet{
          BINLOG_FATAL_ERROR, "Failed to read the binlog file, the file name is " + _relative_file, "HY000"};
      _connection->send(error_packet);
      return false;
    }

    if (header.get_type_code() != FORMAT_DESCRIPTION_EVENT) {
      ret = verify_event_crc32(event, header.get_event_length(), _checksum_flag);
      if (ret != OMS_OK) {
        binlog::ErrPacket error_packet{
            BINLOG_FATAL_ERROR, "The currently subscribed binlog event crc32 verification failed", "HY000"};
        _connection->send(error_packet);
        return false;
      }
    }
  }

  return true;
}

void BinlogDumper::wait_rotate_ready(std::string const& file) const
{

  int retry = 0;
  while (!FsUtil::exist(file) && retry < 100) {
    OMS_WARN("{}: The currently rotated file {} is not ready", _connection->trace_id(), file);
    usleep(10000);
    retry++;
  }

  if (!FsUtil::exist(file)) {
    OMS_ERROR("{}: The currently rotated file {} is not exist", _connection->trace_id(), file);
  }
}

int64_t BinlogDumper::get_heartbeat_period()
{
  int64_t heartbeat_period = logproxy::Config::instance().binlog_heartbeat_interval_us.val();
  std::string hb_period = _connection->get_session_var("master_heartbeat_period");
  if (!hb_period.empty()) {
    heartbeat_period = atoll(hb_period.c_str());
  }
  return heartbeat_period > 0 ? heartbeat_period
                              : logproxy::Config::instance().binlog_heartbeat_interval_us.val() * 1000;
}

enum_checksum_flag BinlogDumper::get_binlog_checksum()
{
  return _checksum_flag;
}

void BinlogDumper::init_binlog_checksum()
{
  /*!
   * @brief When the master binlog checksum cannot be obtained in the session,
   * logproxy is configured by default, leaving an opening for operation and maintenance
   */
  std::string checksum_flag = _connection->get_session_var("master_binlog_checksum");
  if (checksum_flag.empty()) {
    OMS_INFO("The master_binlog_checksum at the session level is set to empty");
    _checksum_flag = Config::instance().binlog_checksum.val() ? CRC32 : OFF;
    return;
  }

  if (!checksum_flag.empty()) {
    transform(checksum_flag.begin(), checksum_flag.end(), checksum_flag.begin(), ::toupper);
    if (std::equal(checksum_flag.begin(), checksum_flag.end(), "CRC32")) {
      OMS_INFO("The master_binlog_checksum at the session level is set to {}", CRC32);
      _checksum_flag = CRC32;
      return;
    }
  }
  OMS_INFO("The master_binlog_checksum at the session level is set to {}", OFF);
  _checksum_flag = OFF;
  return;
}

void BinlogDumper::set_binlog_checksum(enum_checksum_flag checksum_flag)
{
  this->_checksum_flag = checksum_flag;
}

void DumperMetric::mark_checkpoint_ts(int64_t ts)
{
  this->_checkpoint_ts = ts;
}

void DumperMetric::count_send(uint64_t count)
{
  _send_count.fetch_add(count);
}

void DumperMetric::count_send_io(uint64_t bytes)
{
  _send_io.fetch_add(bytes);
}

std::uint64_t DumperMetric::rps()
{
  uint64_t count = _send_count.load();
  uint64_t rps = interval_s == 0 ? count : (count / interval_s);
  _send_count.fetch_sub(count);
  return rps;
}

std::uint64_t DumperMetric::iops()
{
  uint64_t io = _send_io.load();
  uint64_t iops = interval_s == 0 ? io : (io / interval_s);
  _send_io.fetch_sub(_send_io);
  return iops;
}

std::uint64_t DumperMetric::delay()
{
  return Timer::now_s() - _checkpoint_ts;
}

std::uint64_t DumperMetric::checkpoint()
{
  return _checkpoint_ts;
}

}  // namespace logproxy
}  // namespace oceanbase