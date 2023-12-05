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

#include "fs_util.h"
#include "config.h"
#include "common_util.h"
#include "counter.h"

#include "binlog_storage.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

int BinlogStorage::init(ConvertMeta& meta, IObCdcAccess* oblog, OblogConfig& config)
{
  _oblog = oblog;
  this->set_meta(meta);
  int file_index = 0;
  string binlog_dir = this->get_meta().log_bin_prefix + BINLOG_DATA_DIR;
  string binlog_index = this->get_meta().log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME;

  // find mysql-bin.index
  BinlogIndexRecord record;
  get_index(binlog_index, record);
  std::string bin_path;
  std::string bin_name;
  if (record._file_name.empty()) {
    OMS_ATOMIC_INC(file_index);
    bin_name = binlog::CommonUtils::fill_binlog_file_name(file_index);
    bin_path = binlog_dir + bin_name;
    std::pair<std::string, int64_t> current("", 0);
    std::pair<std::string, int64_t> before("", 0);

    /*!
     * @brief If the gtid mapping relationship is specified,
     *        we should start the binlog service from the specified mapping relationship
     */
    if (!config.initial_trx_xid.val().empty()) {
      current.first = config.initial_trx_xid.val();
      current.second = config.initial_trx_gtid_seq.val();
    }
    BinlogIndexRecord idx = {bin_path, file_index};
    idx.set_current_mapping(current);
    idx.set_before_mapping(before);
    idx.set_checkpoint(meta.first_start_timestamp);
    idx.set_position(0);
    add_index(binlog_index, idx);
  } else {
    bin_path = record.get_file_name();
    meta.first_start_timestamp = record.get_checkpoint();
  }
  this->_file_name = bin_path;

  // init gtid
  bool rotate_existed = false;
  uint64_t complete_transaction_pos;
  uint64_t last_complete_txn_id = 0;
  uint64_t start_complete_txn_id = 0;
  get_the_last_complete_txn(
      _file_name, complete_transaction_pos, last_complete_txn_id, start_complete_txn_id, rotate_existed);
  // Traverse all gtid events before closing the current binlog file
  if (start_complete_txn_id != 0) {
    this->_range.first = start_complete_txn_id;
    this->_range.second = last_complete_txn_id;
  } else {
    this->_range.first = (config.initial_trx_xid.val().empty() || config.initial_trx_gtid_seq.val() == 1) ? 0 : 1;
    this->_range.second = config.initial_trx_xid.val().empty() ? 0 : config.initial_trx_gtid_seq.val() - 1;
  }

  OMS_STREAM_INFO << "Success to init binlog:" << this->_file_name;
  return OMS_OK;
}

void BinlogStorage::run()
{
  std::vector<ObLogEvent*> records;
  records.reserve(_s_config.read_wait_num.val());
  MsgBuf buffer;
  size_t buffer_pos = 0;
  std::string index_file_name = _meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME;
  BinlogIndexRecord index_record;
  get_index(index_file_name, index_record);
  while (is_run()) {
    _stage_timer.reset();
    while (!_event_queue.poll(records, _s_config.read_timeout_us.val()) || records.empty()) {
      OMS_STREAM_INFO << "storage binlog queue empty, retry...";
    }
    size_t record_count = records.size();

    if (storage_binlog_event(records, index_file_name, buffer, buffer_pos, index_record) != OMS_OK) {
      OMS_STREAM_ERROR << "Failed to load binlog file to disk";
      return;
    }

    finishing_touches(index_file_name, index_record, record_count, buffer, buffer_pos);
  }
}

int BinlogStorage::finishing_touches(const string& index_file_name, BinlogIndexRecord& index_record,
    size_t record_count, MsgBuf& buffer, size_t& buffer_pos)
{
  buffer.reset();
  buffer_pos = 0;
  int64_t poll_us = _stage_timer.elapsed();
  Counter::instance().count_write(int(record_count));
  Counter::instance().count_key(Counter::SENDER_SEND_US, poll_us);
  return OMS_OK;
}

int BinlogStorage::storage_binlog_event(vector<ObLogEvent*>& records, const string& index_file_name, MsgBuf& buffer,
    size_t& buffer_pos, BinlogIndexRecord& index_record)
{
  Timer cache_time;
  cache_time.reset();
  for (auto record : records) {
    if (record == nullptr) {
      OMS_STREAM_ERROR << "event is an unexpected NULL value";
      continue;
    }

    OMS_DEBUG(record->str_format());

    if (record->get_header()->get_type_code() == ROTATE_EVENT) {
      buffer_pos = rotate(record, buffer, buffer_pos, index_record, index_file_name);
      if (buffer_pos == OMS_FAILED) {
        OMS_ERROR("Failed to rotate binlog file:{}", ((RotateEvent*)record)->get_next_binlog_file_name());
        return OMS_FAILED;
      }
      _range.first = 0;
      continue;
    }

    if (record->get_header()->get_type_code() == GTID_LOG_EVENT) {
      index_record.set_before_mapping(index_record.get_current_mapping());
      index_record._current_mapping.first = record->get_ob_txn();
      index_record._current_mapping.second = ((GtidLogEvent*)record)->get_gtid_txn_id();

      if (_range.first == 0) {
        _range.first = index_record._current_mapping.second;
      }
      _range.second = index_record._current_mapping.second;

      index_record.set_checkpoint(record->get_checkpoint());
      Counter::instance().mark_checkpoint(record->get_checkpoint());
      Counter::instance().mark_timestamp(
          ((GtidLogEvent*)record)->get_last_committed() * 1000000 + ((GtidLogEvent*)record)->get_sequence_number());
      OMS_STREAM_DEBUG << "current ob txn:" << record->get_ob_txn()
                       << ",gtid txn id:" << index_record._current_mapping.second
                       << ", checkpoint:" << index_record.get_checkpoint();
    }

    auto* data = static_cast<unsigned char*>(malloc(record->get_header()->get_event_length()));
    size_t ret = record->flush_to_buff(data);

    if ((buffer_pos + record->get_header()->get_event_length()) >= _s_config.binlog_max_event_buffer_bytes.val() ||
        (cache_time.elapsed() > _s_config.binlog_convert_timeout_us.val() && buffer_pos != 0)) {
      if (FsUtil::append_file(_file_name, buffer) != OMS_OK) {
        return OMS_FAILED;
      }
      // update index record
      // update offset
      struct stat file_stat;
      int file_size = stat(_file_name.c_str(), &file_stat);
      _offset = (file_size == 0 ? file_stat.st_size : 0);
      index_record.set_position(_offset);
      update_index(index_file_name, index_record);
      buffer.reset();
      Counter::instance().count_write_io(buffer_pos);
      buffer_pos = 0;
      cache_time.reset();
    }
    buffer.push_back(reinterpret_cast<char*>(data), ret);
    buffer_pos += ret;
  }

  release_vector(records);

  if (buffer_pos > 0) {
    if (FsUtil::append_file(_file_name, buffer) != OMS_OK) {
      return OMS_FAILED;
    }
    // update offset
    struct stat file_stat;
    int file_size = stat(_file_name.c_str(), &file_stat);
    _offset = (file_size == 0 ? file_stat.st_size : 0);
    index_record.set_position(_offset);
    // update index record
    update_index(index_file_name, index_record);
    Counter::instance().count_write_io(buffer_pos);
  }
  return OMS_OK;
}

void BinlogStorage::stop()
{
  Thread::stop();
}

BinlogStorage::BinlogStorage(BinlogConverter& reader, BlockingQueue<ObLogEvent*>& event_queue)
    : _event_queue(event_queue), _converter(reader), _oblog(nullptr)
{
  std::uint32_t checksum = 0;
  if (Config::instance().binlog_checksum.val()) {
    checksum = COMMON_CHECKSUM_LENGTH;
  }
  _offset = BINLOG_START_POS + checksum;
}

const ConvertMeta& BinlogStorage::get_meta() const
{
  return _meta;
}

void BinlogStorage::set_meta(const ConvertMeta& meta)
{
  _meta = meta;
}

size_t init_binlog_file(MsgBuf& content, RotateEvent* rotate_event, vector<GtidMessage*>& gtid_messages)
{
  auto* event_data = static_cast<unsigned char*>(malloc(1024));
  size_t buff_pos = 0;
  // add magic
  for (int i = 0; i < BINLOG_MAGIC_SIZE; ++i) {
    int1store(event_data + i, binlog_magic[i]);
  }
  buff_pos += BINLOG_MAGIC_SIZE;

  // add FormatDescriptionEvent
  FormatDescriptionEvent format_description_event =
      FormatDescriptionEvent(rotate_event->get_header()->get_timestamp(), SERVER_ID);
  buff_pos += format_description_event.flush_to_buff(event_data + buff_pos);
  PreviousGtidsLogEvent previous_gtids_log_event =
      PreviousGtidsLogEvent(1, gtid_messages, rotate_event->get_header()->get_timestamp());

  uint32_t next_pos = format_description_event.get_header()->get_next_position() +
                      previous_gtids_log_event.get_header()->get_event_length();

  previous_gtids_log_event.get_header()->set_next_position(next_pos);
  buff_pos += previous_gtids_log_event.flush_to_buff(event_data + buff_pos);
  content.push_back_copy(reinterpret_cast<char*>(event_data), buff_pos);
  free(event_data);
  return buff_pos;
}

void fetch_pre_gtid_event(const BinlogIndexRecord& index_record, GtidMessage* gtid_message,
    pair<uint64_t, uint64_t> pair, vector<logproxy::ObLogEvent*>& log_events)
{
  seek_events(index_record._file_name, log_events, PREVIOUS_GTIDS_LOG_EVENT, true);
  if (!log_events.empty()) {
    auto* previous_gtids_log_event = dynamic_cast<PreviousGtidsLogEvent*>(log_events.at(0));
    map<string, GtidMessage*> gtids = previous_gtids_log_event->get_gtid_messages();
    // There will only be one server uuid for the same tenant by default
    for (auto const& gtid_it : gtids) {
      GtidMessage* previous_gtids = gtid_it.second;
      if (!previous_gtids->get_txn_range().empty()) {
        if (pair.first > previous_gtids->get_txn_range().back().second + 1) {
          gtid_message->get_txn_range().assign(
              previous_gtids->get_txn_range().begin(), previous_gtids->get_txn_range().end());
          gtid_message->get_txn_range().emplace_back(pair);
        } else {
          gtid_message->get_txn_range().assign(
              previous_gtids->get_txn_range().begin(), previous_gtids->get_txn_range().end());
          gtid_message->get_txn_range().back().second = pair.second;
        }
      } else {
        gtid_message->get_txn_range().emplace_back(pair);
      }
    }
  }
  OMS_STREAM_INFO << "rotate gtid message: " << gtid_message->format_string();
}

size_t BinlogStorage::rotate(ObLogEvent* event, MsgBuf& content, std::size_t size, BinlogIndexRecord& index_record,
    const string& index_file_name)
{
  RotateEvent* rotate_event = ((RotateEvent*)event);

  std::vector<GtidMessage*> gtid_messages;
  auto* gtid_message = new GtidMessage();
  OMS_STREAM_DEBUG << "server uuid: " << this->_meta.server_uuid;
  gtid_message->set_gtid_uuid(this->_meta.server_uuid);
  gtid_message->set_gtid_txn_id_intervals(1);
  std::pair<uint64_t, uint64_t> pair;

  pair.first = this->_range.first;
  pair.second = this->_range.second + 1;

  OMS_INFO("Executed transaction id information:[{},{})", pair.first, pair.second);

  if (rotate_event->get_op() != RotateEvent::INIT) {
    if (previous_rotation(content, size, rotate_event, index_record, index_file_name) != OMS_OK) {
      delete (gtid_message);
      return OMS_FAILED;
    }

    std::vector<logproxy::ObLogEvent*> log_events;
    fetch_pre_gtid_event(index_record, gtid_message, pair, log_events);
    release_vector(log_events);

    int ret = current_rotation(index_record, rotate_event);
    if (ret != OMS_OK) {
      delete (gtid_message);
      return ret;
    }
  } else {
    if (pair.first != 0) {
      gtid_message->get_txn_range().emplace_back(pair);
    }
  }

  gtid_message->set_gtid_txn_id_intervals(gtid_message->get_txn_range().size());
  gtid_messages.emplace_back(gtid_message);
  OMS_INFO("gtid message :{}", gtid_message->format_string());

  size_t buff_pos = init_binlog_file(content, rotate_event, gtid_messages);

  return buff_pos;
}

int BinlogStorage::current_rotation(BinlogIndexRecord& index_record, const RotateEvent* rotate_event)
{
  const string& next_file = rotate_event->get_next_binlog_file_name();
  string bin_path = get_meta().log_bin_prefix + BINLOG_DATA_DIR + next_file;
  string binlog_index = get_meta().log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME;
  index_record._index = rotate_event->get_index();
  index_record._file_name = bin_path;
  index_record.set_position(0);
  if (add_index(binlog_index, index_record) != OMS_OK) {
    OMS_ERROR("Failed to add binlog index record:{}", index_record.to_string());
    return OMS_FAILED;
  }
  _file_name = bin_path;
  OMS_STREAM_INFO << "rotate file:" << next_file;
  return OMS_OK;
}

int BinlogStorage::previous_rotation(MsgBuf& content, size_t size, RotateEvent* rotate_event,
    BinlogIndexRecord& index_record, const string& index_file_name)
{
  if (!rotate_event->is_existed()) {
    // When the rotate event is not generated, the rotate event information should be supplemented
    auto* data = static_cast<unsigned char*>(malloc(rotate_event->get_header()->get_event_length()));
    size_t ret = rotate_event->flush_to_buff(data);
    content.push_back(reinterpret_cast<char*>(data), ret);
    size += ret;
    if (FsUtil::append_file(_file_name, content) != OMS_OK) {
      free(data);
      data = nullptr;
      return OMS_FAILED;
    }
    // update offset
    _offset = FsUtil::file_size(_file_name);
    index_record.set_position(_offset);
    // update index record
    update_index(index_file_name, index_record);
    Counter::instance().count_write_io(content.byte_size());
    OMS_STREAM_INFO << "rotate event:" << index_record._file_name << " [offset]" << index_record.get_position();
    content.reset();
  }
  auto* flags = static_cast<unsigned char*>(malloc(FLAGS_LEN));
  int2store(flags, 0);
  FILE* fp = FsUtil::fopen_binary(_file_name, "rb+");
  FsUtil::rewrite(fp, flags, BINLOG_MAGIC_SIZE + FLAGS_OFFSET, FLAGS_LEN);
  free(flags);
  FsUtil::fclose_binary(fp);
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
