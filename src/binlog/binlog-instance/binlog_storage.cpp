/**
 * Copyright (c) 2024 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "binlog_storage.h"

#include "fs_util.h"
#include "config.h"
#include "common_util.h"
#include "counter.h"
#include "env.h"
#include "binlog_converter.h"

#include <ThreadPerTaskScheduler.h>

using namespace oceanbase::logproxy;
namespace oceanbase::binlog {
void SerializeEvent::reset()
{
  buffer.reset_offset();
}

void SerializeEvent::update_index_record(const BinlogIndexRecord& index_record)
{
  if (index_records.size() <= index_pos) {
    index_records.push_back(index_record);
  } else {
    assert(index_pos == index_records.size() - 1);
    index_records[index_pos] = index_record;
  }
}

void SerializeHandler::onEvent(SerializeEvent& data, std::int64_t sequence)
{
  data.reset();
  // If this batch contains rotate event, it needs to be serialized to the specified data location.
  for (const auto rotate_event : data.rotate_events) {
    data.rotate_offsets.push_back(
        rotate_event->get_header()->get_next_position() - rotate_event->get_header()->get_event_length());
  }

  int index = 0;
  for (const auto event : data.events) {
    if (event->is_filter()) {
      continue;
    }
    Counter::instance().count_write(1);
    if (data.is_rotation && data.rotate_events.size() > index &&
        event->get_header()->get_next_position() == data.rotate_offsets[index]) {
      const auto len =
          event->get_header()->get_event_length() + data.rotate_events[index]->get_header()->get_event_length();
      data.buffer.ensure_capacity(len);
      event->flush_to_buff(data.buffer.data_cur());
      data.buffer.publish(event->get_header()->get_event_length());
      data.rotate_events[index]->flush_to_buff(data.buffer.data_cur());
      data.buffer.publish(data.rotate_events[index]->get_header()->get_event_length());
      data.rotate_offsets[index] = data.buffer.offset();
      index++;
      continue;
    }
    const auto len = event->get_header()->get_event_length();
    data.buffer.ensure_capacity(len);
    event->flush_to_buff(data.buffer.data_cur());
    data.buffer.publish(len);
  }

  auto seq = g_disruptor->ringBuffer()->next();
  auto ringBuffer = g_disruptor->ringBuffer();
  (*ringBuffer)[seq].events.swap(data.events);
  ringBuffer->publish(seq);
}

void AllocateHandler::onEvent(SerializeEvent& data, std::int64_t sequence, bool endOfBatch)
{
  _binlog_storage->global_allocation_of_backfill_events(data);
}

void StorageHandler::onEvent(SerializeEvent& data, std::int64_t sequence, bool endOfBatch)
{
  if (data.is_rotation) {
    uint64_t cur_offset = 0;
    uint64_t index = 0;
    for (const auto rotate_index : data.rotate_offsets) {
      assert(rotate_index > cur_offset);
      if (_binlog_storage->persistent_binlog(
              data.buffer.data() + cur_offset, rotate_index - cur_offset, data.index_records[index]) != OMS_OK) {
        OMS_FATAL("placement failed !!!");
        throw std::runtime_error("placement failed !!!");
      }
      cur_offset = rotate_index;
      if (_binlog_storage->init_next_binlog_file(
              dynamic_cast<RotateEvent*>(data.rotate_events[index]), data.index_records[index]) != OMS_OK) {
        OMS_ERROR("Failed to initialize next binlog file");
        throw std::runtime_error("Failed to initialize next binlog file");
      }
      g_purge_binlog_executor->submit(purge_expired_binlog);
      index++;
    }

    /*
     * The remaining data after this batch rotation is written
     */
    if (cur_offset < data.buffer.offset()) {
      if (data.index_records.size() != index + 1) {
        OMS_FATAL("index record is missing, unexpected");
        throw std::runtime_error("index record is missing, unexpected !!!");
      }
      if (_binlog_storage->persistent_binlog(data.buffer.data() + cur_offset,
              data.buffer.offset() - cur_offset,
              data.index_records[index]) != OMS_OK) {
        throw std::runtime_error("placement failed !!!");
      }
    }

  } else {
    if (data.index_records.size() != 1) {
      OMS_FATAL("index record is missing, unexpected");
      throw std::runtime_error("index record is missing, unexpected !!!");
    }
    if (_binlog_storage->persistent_binlog(data.buffer.data(), data.buffer.offset(), data.index_records[0]) != OMS_OK) {
      OMS_FATAL("placement failed !!!");
      throw std::runtime_error("placement failed !!!");
    }
  }

  release_vector(data.rotate_events);
  data.rotate_offsets.clear();
  data.index_pos = 0;
  data.index_records.clear();
  Counter::instance().count_write_io(data.buffer.offset());
  data.is_rotation = false;
}

void SerializeExceptionHandler::handleEventException(
    const std::exception& ex, std::int64_t sequence, SerializeEvent& evt)
{
  OMS_ERROR("Handle event exception: {},sequence :{}, trace :{}", ex.what(), sequence, CommonUtils::get_stack_trace());
  _binlog_storage->stop();
}

void SerializeExceptionHandler::handleOnStartException(const std::exception& ex)
{
  OMS_ERROR("Serialize handle startup failed : {}, trace :{}", ex.what(), CommonUtils::get_stack_trace());
  _binlog_storage->stop();
}

void SerializeExceptionHandler::handleOnShutdownException(const std::exception& ex)
{
  OMS_ERROR("Serialize handle shutdown failed: {}, trace :{}", ex.what(), CommonUtils::get_stack_trace());
  _binlog_storage->stop();
}

void SerializeExceptionHandler::handleOnTimeoutException(const std::exception& ex, std::int64_t sequence)
{
  OMS_ERROR("Serialize handle timeout exception: {}, sequence: {}, trace :{}", ex.what(), sequence, CommonUtils::get_stack_trace());
  _binlog_storage->stop();
}

int BinlogStorage::init()
{
  _serialize_task_scheduler = std::make_shared<Disruptor::RoundRobinThreadAffinedTaskScheduler>();

  if (s_meta.binlog_config()->binlog_serialize_ring_buffer_size() < 1) {
    OMS_ERROR("binlog_serialize_ring_buffer_size must not be less than 1");
    return OMS_FAILED;
  }

  if (!Disruptor::Util::isPowerOf2(s_meta.binlog_config()->binlog_serialize_ring_buffer_size())) {
    OMS_ERROR("binlog_serialize_ring_buffer_size must be a power of 2");
    return OMS_FAILED;
  }

  _serialize_disruptor = std::make_shared<Disruptor::disruptor<SerializeEvent>>(
      create_serialize_event, s_meta.binlog_config()->binlog_serialize_ring_buffer_size(), _serialize_task_scheduler);
  _allocate_handler = std::make_shared<AllocateHandler>(this);
  _storage_handler = std::make_shared<StorageHandler>(this);
  for (int i = 0; i < s_meta.binlog_config()->binlog_serialize_parallel_size(); ++i) {
    _serialize_handlers.emplace_back(std::make_shared<SerializeHandler>());
  }
  auto exception_handle = std::make_shared<SerializeExceptionHandler>(this);
  _serialize_disruptor->handleExceptionsWith(exception_handle);
  _serialize_disruptor->handleEventsWith(_allocate_handler)
      ->thenHandleEventsWithWorkerPool(_serialize_handlers)
      ->then(_storage_handler);

  auto number_of_threads = std::max(s_meta.binlog_config()->binlog_serialize_thread_size(),
      s_meta.binlog_config()->binlog_serialize_parallel_size() + 2);
  _serialize_task_scheduler->start(number_of_threads);
  _serialize_disruptor->start();

  Counter::instance().register_gauge("SerializeRingBufferQ", [this]() {
    return _serialize_disruptor->bufferSize() - _serialize_disruptor->ringBuffer()->getRemainingCapacity();
  });

  Counter::instance().register_gauge("ReleaseRingBufferQ",
      [this]() { return g_disruptor->bufferSize() - g_disruptor->ringBuffer()->getRemainingCapacity(); });

  int file_index = 0;
  string binlog_dir = string(".") + BINLOG_DATA_DIR;

  // find mysql-bin.index
  BinlogIndexRecord record;
  g_index_manager->get_latest_index(record);
  std::string bin_path;
  std::string bin_name;
  if (record.get_file_name().empty()) {
    OMS_ATOMIC_INC(file_index);
    bin_name = binlog::CommonUtils::fill_binlog_file_name(file_index);
    bin_path = binlog_dir + bin_name;
    std::pair<std::string, uint64_t> current("", 0);
    std::pair<std::string, uint64_t> before("", 0);

    // If the gtid mapping is specified, we should start the binlog service from the specified gtid mapping
    if (!s_meta.binlog_config()->initial_ob_txn_id().empty() || s_meta.binlog_config()->initial_ob_txn_gtid_seq() > 0) {
      current.first = s_meta.binlog_config()->initial_ob_txn_id();
      current.second = current.first.empty() ? s_meta.binlog_config()->initial_ob_txn_gtid_seq() - 1
                                             : s_meta.binlog_config()->initial_ob_txn_gtid_seq();
    }
    BinlogIndexRecord index_record(bin_path, file_index);
    index_record.set_current_mapping(current);
    index_record.set_before_mapping(before);
    index_record.set_checkpoint(s_meta.cdc_config()->start_timestamp());
    index_record.set_position(0);
    g_index_manager->add_index(index_record);
  } else {
    bin_path = record.get_file_name();
  }
  this->_file_name = bin_path;
  OMS_INFO("[storage] Successfully init the current binlog file: {}", _file_name);

  if (recover_safely() != OMS_OK) {
    OMS_ERROR("Failed to recover binlog storage");
    return OMS_FAILED;
  }
  OMS_INFO("Successfully recover binlog storage");
  return OMS_OK;
}

void BinlogStorage::run()
{
  std::vector<ObLogEvent*> events;
  g_index_manager->get_latest_index(_index_record);
  if (_index_record.get_file_name().empty()) {
    OMS_ERROR("Failed to get latest index record.");
    stop();
  }

  while (is_run()) {
    // _stage_timer.reset();
    events.reserve(s_meta.binlog_config()->storage_wait_num());
    _stage_timer.reset();
    while (!_event_queue.poll(events, s_meta.binlog_config()->storage_timeout_us()) || events.empty()) {
      if (!is_run()) {
        OMS_ERROR("Binlog storage thread has been stopped.");
        break;
      }
      OMS_DEBUG("Empty log event queue put by convert thread, retry...");
    }
    auto seq = _serialize_disruptor->ringBuffer()->next();
    auto ringBuffer = _serialize_disruptor->ringBuffer();
    (*ringBuffer)[seq].events.swap(events);
    ringBuffer->publish(seq);
  }
  OMS_ERROR("Storage thread exits");
  _converter.stop_converter();
}

int BinlogStorage::finishing_touches(MsgBuf& buffer, size_t& buffer_pos) const
{
  buffer.reset();
  buffer_pos = 0;
  int64_t poll_us = _stage_timer.elapsed();
  Counter::instance().count_key(Counter::SENDER_SEND_US, poll_us);
  return OMS_OK;
}

void BinlogStorage::stop()
{
  if (is_run()) {
    Thread::stop();
  }
  OMS_INFO("Begin to stop binlog storage thread...");
}

BinlogStorage::BinlogStorage(BinlogConverter& reader, BlockingQueue<ObLogEvent*>& event_queue)
    : _event_queue(event_queue), _converter(reader)
{
  _rate_limiter.update_throttle_rps(s_meta.binlog_config()->throttle_convert_rps());
  _rate_limiter.update_throttle_iops(s_meta.binlog_config()->throttle_convert_iops());
}

int64_t BinlogStorage::init_binlog_file(RotateEvent* rotate_event)
{
  MsgBuf content;
  auto* event_data = static_cast<unsigned char*>(malloc(1024));
  int64_t buff_pos = 0;
  // add magic
  for (int i = 0; i < BINLOG_MAGIC_SIZE; ++i) {
    int1store(event_data + i, binlog_magic[i]);
  }
  buff_pos += BINLOG_MAGIC_SIZE;

  // add FormatDescriptionEvent
  FormatDescriptionEvent format_description_event = FormatDescriptionEvent(rotate_event->get_header()->get_timestamp());
  buff_pos += format_description_event.flush_to_buff(event_data + buff_pos);
  // add PreviousGtidsLogEvent
  std::vector<GtidMessage*> gtid_messages;
  rotate_event->get_next_previous_gtids(gtid_messages);
  PreviousGtidsLogEvent previous_gtids_log_event =
      PreviousGtidsLogEvent(gtid_messages.size(), gtid_messages, rotate_event->get_header()->get_timestamp());

  uint32_t next_pos = format_description_event.get_header()->get_next_position() +
                      previous_gtids_log_event.get_header()->get_event_length();

  previous_gtids_log_event.get_header()->set_next_position(next_pos);
  buff_pos += previous_gtids_log_event.flush_to_buff(event_data + buff_pos);
  content.push_back_copy(reinterpret_cast<char*>(event_data), buff_pos);
  OMS_INFO(
      "Successfully init binlog file(add FormatDescriptionEvent + PreviousGtidsLogEvent), current pos: {}", buff_pos);
  free(event_data);
  _file_name = string(".") + BINLOG_DATA_DIR + rotate_event->get_next_binlog_file_name();
  if (FsUtil::append_file(_file_name, content) != OMS_OK) {
    return OMS_FAILED;
  }
  return OMS_OK;
}

int64_t BinlogStorage::init_next_binlog_file(RotateEvent* rotate_event, BinlogIndexRecord& index_record)
{
  auto* flags = static_cast<unsigned char*>(malloc(FLAGS_LEN));
  int2store(flags, 0);
  FILE* fp = FsUtil::fopen_binary(
      string(".") + BINLOG_DATA_DIR + CommonUtils::fill_binlog_file_name(rotate_event->get_index() - 1), "rb+");
  FsUtil::rewrite(fp, flags, BINLOG_MAGIC_SIZE + FLAGS_OFFSET, FLAGS_LEN);
  free(flags);
  FsUtil::fclose_binary(fp);

  auto* event_data = static_cast<unsigned char*>(malloc(1024));
  defer(free(event_data););
  int64_t buff_pos = 0;
  // add magic
  for (int i = 0; i < BINLOG_MAGIC_SIZE; ++i) {
    int1store(event_data + i, binlog_magic[i]);
  }
  buff_pos += BINLOG_MAGIC_SIZE;

  // add FormatDescriptionEvent
  FormatDescriptionEvent format_description_event = FormatDescriptionEvent(rotate_event->get_header()->get_timestamp());
  buff_pos += format_description_event.flush_to_buff(event_data + buff_pos);
  // add PreviousGtidsLogEvent
  std::vector<GtidMessage*> gtid_messages;
  rotate_event->get_next_previous_gtids(gtid_messages);
  PreviousGtidsLogEvent previous_gtids_log_event =
      PreviousGtidsLogEvent(gtid_messages.size(), gtid_messages, rotate_event->get_header()->get_timestamp());

  uint32_t next_pos = format_description_event.get_header()->get_next_position() +
                      previous_gtids_log_event.get_header()->get_event_length();

  previous_gtids_log_event.get_header()->set_next_position(next_pos);
  buff_pos += previous_gtids_log_event.flush_to_buff(event_data + buff_pos);
  if (FsUtil::append_file(
          string(".") + BINLOG_DATA_DIR + rotate_event->get_next_binlog_file_name(), event_data, buff_pos) != OMS_OK) {
    OMS_ERROR("Write next binlog file failed: {}", rotate_event->get_next_binlog_file_name());
    return OMS_FAILED;
  }
  OMS_INFO(
      "Successfully init binlog file(add FormatDescriptionEvent + PreviousGtidsLogEvent), current pos: {}", buff_pos);
  return add_next_binlog_index(index_record, rotate_event);
}

int BinlogStorage::global_allocation_of_backfill_events(SerializeEvent& event_batch)
{
  for (auto event : event_batch.events) {
    if (event == nullptr) {
      OMS_ERROR("event is an unexpected NULL value");
      continue;
    }
    bool is_rotate = false;
    _rate_limiter.in_event_with_alarm(event->get_header()->get_event_length());
    switch (event->get_header()->get_type_code()) {
      case HEARTBEAT_LOG_EVENT: {
        if (!_filter_util_checkpoint_trx) {
          g_gtid_manager->mark_event(true, event->get_checkpoint(), _index_record.get_current_mapping());
          g_gtid_manager->compress_and_save_gtid_seq(_index_record.get_current_mapping());
        }
        continue;
      }
      case GTID_LOG_EVENT: {
        auto gtid_log_event = (GtidLogEvent*)event;
        gtid_log_event->set_gtid_txn_id(_txn_gtid_seq);
        if (_filter_util_checkpoint_trx) {
          if (!_meet_initial_trx && (gtid_log_event->get_ob_txn() != _txn_mapping.first ||
                                        gtid_log_event->get_gtid_txn_id() != _txn_mapping.second)) {
            OMS_INFO("Skip current transaction: {} <-> {}, which is earlier than checkpoint transaction: {} <-> {}",
                gtid_log_event->get_ob_txn(),
                gtid_log_event->get_gtid_txn_id(),
                _txn_mapping.first,
                _txn_mapping.second);
            continue;
          }

          if (gtid_log_event->get_ob_txn() == _txn_mapping.first &&
              gtid_log_event->get_gtid_txn_id() == _txn_mapping.second) {
            OMS_INFO("Meet the checkpoint transaction: {} <-> {}(checkpoint: {}), and the checkpoint transaction: {} "
                     "<-> {}, "
                     "filter current transaction: {}, next trx gtid seq: {}",
                gtid_log_event->get_ob_txn(),
                gtid_log_event->get_gtid_txn_id(),
                gtid_log_event->get_checkpoint(),
                _txn_mapping.first,
                _txn_mapping.second,
                _filter_checkpoint_trx,
                _filter_checkpoint_trx ? _txn_gtid_seq + 1 : _txn_gtid_seq);

            _meet_initial_trx = true;
            if (_filter_checkpoint_trx) {
              _txn_gtid_seq += 1;
              _filter_util_checkpoint_trx = true;
              continue;
            } else {
              OMS_INFO(
                  "No longer skip current transaction: {} due to filter checkpoint trx: [false]", _txn_mapping.first);
              _filter_util_checkpoint_trx = false;
            }
          } else if (_meet_initial_trx || (!_meet_initial_trx && gtid_log_event->get_checkpoint() >
                                                                     s_meta.cdc_config()->start_timestamp())) {
            OMS_INFO("No longer skip current transaction: {} <-> {}, since the checkpoint transaction [{} <-> {}] has "
                     "already been skipped or is later than checkpoint transaction: {} > {}.",
                gtid_log_event->get_ob_txn(),
                gtid_log_event->get_gtid_txn_id(),
                _txn_mapping.first,
                _txn_mapping.second,
                gtid_log_event->get_checkpoint(),
                s_meta.cdc_config()->start_timestamp());
            _filter_util_checkpoint_trx = false;
          }
        }
        _valid_events_coming = true;
        _index_record.set_before_mapping(_index_record.get_current_mapping());
        if (_first_gtid_seq == 0) {
          _first_gtid_seq = gtid_log_event->get_gtid_txn_id();
        }

        this->_txn_gtid_seq = gtid_log_event->get_gtid_txn_id() + 1;
        std::pair<std::string, int64_t> current_mapping(
            gtid_log_event->get_ob_txn(), gtid_log_event->get_gtid_txn_id());
        _index_record.set_current_mapping(current_mapping);
        g_gtid_manager->mark_event(false, event->get_checkpoint(), _index_record.get_current_mapping());

        _index_record.set_checkpoint(event->get_checkpoint() / 1000000);
        Counter::instance().mark_checkpoint(event->get_checkpoint());
        Counter::instance().mark_timestamp(
            gtid_log_event->get_last_committed() * 1000000 + gtid_log_event->get_sequence_number());
        OMS_DEBUG("current ob txn: {}, gtid txn id: {}, checkpoint: {}",
            event->get_ob_txn(),
            _index_record.get_current_mapping().second,
            _index_record.get_checkpoint());
        break;
      }
      case XID_EVENT: {
        if (_filter_util_checkpoint_trx) {
          if (!_meet_initial_trx) {
            continue;
          }
          _filter_util_checkpoint_trx = false;
          OMS_INFO(
              "No longer skip record due to meet commit record for checkpoint transaction: {}", _txn_mapping.first);
          continue;
        }
        auto xid_event = dynamic_cast<XidEvent*>(event);
        OMS_ATOMIC_INC(this->_xid);
        xid_event->set_xid(this->_xid);
        if (this->_cur_pos + event->get_header()->get_event_length() > s_meta.binlog_config()->max_binlog_size()) {
          is_rotate = true;
        }
        break;
      }
      case QUERY_EVENT: {
        if (_filter_util_checkpoint_trx) {
          OMS_DEBUG("Skip record with type [query event]");
          continue;
        }
        auto query_event = dynamic_cast<QueryEvent*>(event);
        if (query_event->is_ddl() &&
            this->_cur_pos + event->get_header()->get_event_length() > s_meta.binlog_config()->max_binlog_size()) {
          is_rotate = true;
        }
        break;
      }
      default: {
        if (_filter_util_checkpoint_trx) {
          OMS_INFO("Skip record with type [update] in transaction: {}", event->get_ob_txn());
          continue;
        }
        // do nothing
      }
    }

    const uint64_t len = event->get_header()->get_event_length();
    this->_cur_pos += len;
    event->get_header()->set_next_position(this->_cur_pos);
    event->set_filter(false);

    event_batch.update_index_record(_index_record);
    if (is_rotate) {
      rotate_binlog_file(_last_record_ts, event_batch);
      event_batch.index_pos++;
    }
  }
  event_batch.update_index_record(_index_record);
  return OMS_OK;
}

int64_t BinlogStorage::persistent_binlog(unsigned char* buffer, size_t size, BinlogIndexRecord& index_record)
{
  if (FsUtil::append_file(_file_name, buffer, size) == OMS_FAILED) {
    return OMS_FAILED;
  }
  update_index_record(index_record);
  return OMS_OK;
}

int64_t BinlogStorage::rotate(ObLogEvent* event, MsgBuf& buffer, std::size_t size, BinlogIndexRecord& index_record)
{
  RotateEvent* rotate_event = ((RotateEvent*)event);
  if (rotate_event->get_op() != RotateEvent::INIT) {
    // purge expired binlog according to BINLOG_EXPIRE_LOGS_SECONDS and BINLOG_EXPIRE_LOGS_SIZE
    purge_expired_binlog(index_record);

    if (rotate_current_binlog(buffer, size, rotate_event, index_record) != OMS_OK) {
      return OMS_FAILED;
    }
    // The rotation file must be placed on disk before the index file.
    int64_t buff_pos = init_binlog_file(rotate_event);
    if (OMS_OK != add_next_binlog_index(index_record, rotate_event)) {
      return OMS_FAILED;
    }
    return buff_pos;
  }
  int64_t buff_pos = init_binlog_file(rotate_event);
  return buff_pos;
}

int BinlogStorage::add_next_binlog_index(BinlogIndexRecord& index_record, const RotateEvent* rotate_event)
{
  const std::string& next_file = rotate_event->get_next_binlog_file_name();
  string bin_path = string(".") + BINLOG_DATA_DIR + next_file;
  index_record.set_index(rotate_event->get_index());
  index_record.set_file_name(bin_path);
  uint64_t file_size = FsUtil::file_size(index_record.get_file_name());
  index_record.set_position(file_size);
  if (OMS_OK != g_index_manager->add_index(index_record)) {
    return OMS_FAILED;
  }

  _file_name = bin_path;
  OMS_INFO("Rotate to next binlog file: {}", next_file);
  return OMS_OK;
}

int BinlogStorage::rotate_current_binlog(
    MsgBuf& buffer, size_t size, RotateEvent* rotate_event, BinlogIndexRecord& index_record)
{
  if (!rotate_event->is_existed()) {
    // When the rotate event is not generated, the rotate event information should be supplemented
    auto* data = static_cast<unsigned char*>(malloc(rotate_event->get_header()->get_event_length()));
    // this->_cur_pos += rotate_event->get_header()->get_event_length();
    // rotate_event->get_header()->set_next_position(4);
    size_t ret = rotate_event->flush_to_buff(data);
    buffer.push_back(reinterpret_cast<char*>(data), ret);
    size += ret;
    if (FsUtil::append_file(_file_name, buffer) != OMS_OK) {
      free(data);
      data = nullptr;
      return OMS_FAILED;
    }
    // update offset
    update_index_record(index_record);
    Counter::instance().count_write_io(buffer.byte_size());
    OMS_INFO("Rotate event: {}, offset: {}", index_record.get_file_name(), index_record.get_position());
    buffer.reset();
  }
  auto* flags = static_cast<unsigned char*>(malloc(FLAGS_LEN));
  int2store(flags, 0);
  FILE* fp = FsUtil::fopen_binary(_file_name, "rb+");
  FsUtil::rewrite(fp, flags, BINLOG_MAGIC_SIZE + FLAGS_OFFSET, FLAGS_LEN);
  free(flags);
  FsUtil::fclose_binary(fp);
  return OMS_OK;
}

void BinlogStorage::update_index_record(BinlogIndexRecord& index_record)
{
  _offset = FsUtil::file_size(_file_name);
  index_record.set_position(_offset);
  g_index_manager->update_index(index_record);
  g_gtid_manager->compress_and_save_gtid_seq(index_record.get_current_mapping());

  if (_valid_events_coming) {
    g_gtid_manager->update_gtid_executed(index_record.get_current_mapping().second);
  }
}

void BinlogStorage::purge_expired_binlog(BinlogIndexRecord& index_record)
{
  std::vector<BinlogIndexRecord*> index_records;
  g_index_manager->fetch_index_vector(index_records);
  defer(release_vector(index_records));

  OMS_INFO("Begin to check and clear expired binlog, total number of binlog: {}, binlog range: [{}, {}]",
      index_records.size(),
      index_records.front()->get_file_name(),
      index_records.back()->get_file_name());
  std::set<std::string> need_purged_binlog_files;
  uint64_t last_gtid_seq = 0;
  // 1. purge by binlog_expire_logs_seconds
  uint64_t now_s = Timer::now_s();
  OMS_INFO("Begin to purge binlog based on expiration time: {}, now: {}, and checkpoint range: [{}, {}]",
      s_meta.binlog_config()->binlog_expire_logs_seconds(),
      now_s,
      index_records.front()->get_checkpoint(),
      index_records.back()->get_checkpoint());
  if (s_meta.binlog_config()->binlog_expire_logs_seconds() > 0) {
    uint64_t minimum_checkpoint = index_record.get_checkpoint();

    for (auto iter = index_records.begin(); iter != index_records.end();) {
      if (index_record.equal_to(*(*iter))) {
        ++iter;
        continue;
      }
      if (now_s - (*iter)->get_checkpoint() < s_meta.binlog_config()->binlog_expire_logs_seconds()) {
        minimum_checkpoint = (*iter)->get_checkpoint();
        break;
      }

      OMS_INFO("Binlog [{}] with last gtid [{}] exceeds binlog expiration time: {} - {} > {}",
          (*iter)->get_file_name(),
          (*iter)->get_current_mapping().second,
          now_s,
          (*iter)->get_checkpoint(),
          s_meta.binlog_config()->binlog_expire_logs_seconds());
      need_purged_binlog_files.insert((*iter)->get_file_name());
      if (last_gtid_seq < (*iter)->get_current_mapping().second) {
        last_gtid_seq = (*iter)->get_current_mapping().second;
      }
      delete *iter;
      iter = index_records.erase(iter);
    }
    OMS_INFO("Minimum binlog checkpoint after expiration purging: {}", minimum_checkpoint);
  } else {
    OMS_INFO("Automatically purge binlog files based on [expiration time] is turned off.");
  }

  // 2. purged by binlog_expire_logs_size
  OMS_INFO(
      "Begin to purge binlog based on expiration logs size: {}", s_meta.binlog_config()->binlog_expire_logs_size());
  uint64_t total_binlog_size = 0;
  if (s_meta.binlog_config()->binlog_expire_logs_size() > 0) {
    for (auto r_iter = index_records.rbegin(); r_iter != index_records.rend();) {
      if (index_record.equal_to(*(*r_iter))) {
        ++r_iter;
        continue;
      }

      if (total_binlog_size < s_meta.binlog_config()->binlog_expire_logs_size()) {
        total_binlog_size += (*r_iter)->get_position();
        ++r_iter;
      } else {
        OMS_INFO("Binlog [{}] with last gtid [{}] exceeds binlog expiration logs size: {} + {} > {}",
            (*r_iter)->get_file_name(),
            (*r_iter)->get_current_mapping().second,
            total_binlog_size,
            (*r_iter)->get_position(),
            s_meta.binlog_config()->binlog_expire_logs_size());
        need_purged_binlog_files.insert((*r_iter)->get_file_name());
        if (last_gtid_seq < (*r_iter)->get_current_mapping().second) {  // todo
          last_gtid_seq = (*r_iter)->get_current_mapping().second;
        }
        delete *r_iter;
        r_iter = std::vector<BinlogIndexRecord*>::reverse_iterator(index_records.erase((++r_iter).base()));
      }
    }
    OMS_INFO("Total binlog size after expiration purging: {}", total_binlog_size);
  } else {
    OMS_INFO("Automatically purge binlog files based on [expiration logs size] is turned off.");
  }

  // 3. purge tenant_gtid_seq.meta before last_gtid_seq
  if (0 != last_gtid_seq) {
    g_gtid_manager->purge_gtid_before(last_gtid_seq);
  }

  // 4. rewrite mysql-bin.index
  if (!need_purged_binlog_files.empty()) {
    std::string error_msg;
    if (OMS_OK != g_index_manager->rewrite_index_file(error_msg, index_records)) {
      OMS_ERROR("Failed to rewrite mysql-bin.index, error: {}", error_msg);
      return;
    }
  }

  // 5. remove binlog logs
  // todo check if binlog file is being consumed by dump connection
  for (const std::string& purge_file : need_purged_binlog_files) {
    std::error_code error_code;
    if (std::filesystem::remove(purge_file, error_code) && !error_code) {
      OMS_INFO("Successfully remove binlog log: {}.", purge_file);
    } else {
      OMS_ERROR("Failed to remove binlog log: {}", purge_file);
    }
  }
  OMS_INFO("Number of expired binlog cleaned: {}, and number of remaining binlog: {}, range: [{}, {}]",
      need_purged_binlog_files.size(),
      index_records.size(),
      index_records.front()->get_file_name(),
      index_records.back()->get_file_name());
}
int BinlogStorage::init_restart_point(const BinlogIndexRecord& index_record, GtidLogEvent* event)
{
  bool latest_heartbeat_checkpoint = false;
  uint64_t trx_ts = (event == nullptr) ? 0 : event->get_last_committed();
  GtidSeq gtid_seq;
  int has_last_gtid = g_gtid_manager->get_latest_gtid_seq(gtid_seq);

  // If there is no event for a long time(default 60min), the last heartbeat checkpoint will be used as start_timestamp
  int interval = gtid_seq.get_commit_version_start() - index_record.get_checkpoint();
  if (OMS_OK == has_last_gtid &&
      (interval > s_config.gtid_heartbeat_duration_s.val() ||
          interval > s_meta.binlog_config()->binlog_expire_logs_seconds()) &&
      gtid_seq.get_commit_version_start() > trx_ts &&
      gtid_seq.get_gtid_start() == index_record.get_current_mapping().second && gtid_seq.get_xid_start().empty() &&
      gtid_seq.get_trxs_num() == 0) {
    latest_heartbeat_checkpoint = true;
    _filter_util_checkpoint_trx = false;
    _filter_checkpoint_trx = false;
    s_meta.cdc_config()->set_start_timestamp(gtid_seq.get_commit_version_start());
    _txn_gtid_seq = gtid_seq.get_gtid_start() + 1;
  } else {
    if (_txn_gtid_seq == index_record.get_current_mapping().second) {
      _txn_mapping.first = index_record.get_current_mapping().first;
      _txn_mapping.second = _txn_gtid_seq;
    } else if (_txn_gtid_seq == index_record.get_before_mapping().second) {
      _txn_mapping.first = index_record.get_before_mapping().first;
      _txn_mapping.second = _txn_gtid_seq;
    } else {
      OMS_ERROR("Failed to match gtid seq for the last gtid event(transaction): {}", _txn_gtid_seq);
      return OMS_FAILED;
    }

    uint64_t timestamp;
    if (_txn_mapping.first.empty()) {
      timestamp = index_record.get_checkpoint();
      _txn_gtid_seq = _txn_gtid_seq + 1;
    } else {
      if (has_last_gtid == OMS_FAILED) {
        OMS_INFO("No gtid seq, using checkpoint in index as start timestamp");
        timestamp = index_record.get_checkpoint();
      } else {
        if (OMS_OK != g_gtid_manager->get_first_le_gtid_seq(_txn_gtid_seq, gtid_seq)) {
          OMS_ERROR("Failed to obtain checkpoint less than or equal to last gtid: {}", _txn_gtid_seq);
          return OMS_FAILED;
        }
        timestamp = gtid_seq.get_commit_version_start();
      }
    }
    s_meta.cdc_config()->set_start_timestamp(timestamp);
  }

  OMS_INFO("Init restarting point based on latest heartbeat checkpoint: {}, info:\n"
           "the last index record: {}"
           "the based on gtid seq: {}\n"
           "the last gtid event: gtid_txn_id: {}, timestamp: {}",
      latest_heartbeat_checkpoint,
      index_record.to_string(),
      gtid_seq.serialize(),
      _txn_gtid_seq,
      trx_ts);
  return OMS_OK;
}

std::string previous_gtids_string(const std::vector<GtidMessage*>& previous_gtids)
{
  std::string previous_gtids_str;
  for (auto pre_gtid : previous_gtids) {
    previous_gtids_str.append(pre_gtid->format_string()).append(",");
  }
  return previous_gtids_str;
}

int BinlogStorage::recover_safely()
{
  BinlogIndexRecord index_record;
  g_index_manager->get_latest_index(index_record);
  uint64_t file_size = FsUtil::file_size(index_record.get_file_name());

  uint64_t next_index = index_record.get_index() + 1;
  auto* rotate_event = new RotateEvent(
      BINLOG_MAGIC_SIZE, binlog::CommonUtils::fill_binlog_file_name(next_index), Timer::now() / 1000000, file_size);
  bool rotate_existed = false;
  uint8_t checksum_flag = Config::instance().binlog_checksum.val() ? CRC32 : OFF;

  /*
   * !!! Scenario 1: Starting binlog instance for the first time
   */
  if (index_record.get_file_name().empty() || index_record.get_position() == 0) {
    // If the gtid mapping relationship is specified, we should start the binlog instance from the specified mapping
    bool is_specified_gtid = false;
    if (s_meta.binlog_config()->initial_ob_txn_gtid_seq() > 0 || !s_meta.binlog_config()->initial_ob_txn_id().empty()) {
      is_specified_gtid = true;
      _txn_mapping.first = s_meta.binlog_config()->initial_ob_txn_id();
      _txn_mapping.second = s_meta.binlog_config()->initial_ob_txn_gtid_seq();
      _txn_gtid_seq = _txn_mapping.second;
    }
    _filter_util_checkpoint_trx = !(_txn_mapping.first.empty());
    _filter_checkpoint_trx = false;

    // If started for the first time and no start_timestamp is specified, set to the current time
    if (s_meta.cdc_config()->start_timestamp() == 0) {
      s_meta.cdc_config()->set_start_timestamp(Timer::now_s());
    }
    rotate_event->set_op(RotateEvent::INIT);
    OMS_INFO(
        "First start with specified gtid mapping: {}, start_timestamp: {}, initial transaction: {} <> {}, filter: {}, ",
        is_specified_gtid,
        s_meta.cdc_config()->start_timestamp(),
        _txn_mapping.first,
        _txn_mapping.second,
        _filter_util_checkpoint_trx);
  } else {
    std::vector<GtidLogEvent*> gtid_events;
    defer(release_vector(gtid_events));
    int64_t offset = 0;
    if (OMS_OK != seek_gtid_event(index_record.get_file_name(),
                      gtid_events,
                      rotate_existed,
                      checksum_flag,
                      _xid,
                      _previous_gtid_messages,
                      offset)) {
      delete rotate_event;
      return OMS_FAILED;
    }
    if (offset >= 0 && offset != index_record.get_position() && offset != file_size) {
      OMS_ERROR("The next position of the last binlog event is not equal to file [{}] size: {} != {}",
          index_record.get_file_name(),
          offset,
          index_record.get_position());
      delete rotate_event;
      return OMS_FAILED;
    }
    OMS_INFO("Seeked [{}] gtid events(transactions) from the last binlog file: {}, rotate existed: {}",
        gtid_events.size(),
        index_record.to_string(),
        rotate_existed);

    // !!! Scenario 2: After last starting/rotating the binlog file, no ddl or dml has come yet
    if (gtid_events.empty()) {
      _txn_gtid_seq = index_record.get_current_mapping().second;
      if (OMS_OK != init_restart_point(index_record, nullptr)) {
        delete rotate_event;
        return OMS_FAILED;
      }

      if (_filter_util_checkpoint_trx) {
        _filter_util_checkpoint_trx = !(_txn_mapping.first.empty());
        _filter_checkpoint_trx = !(index_record.get_before_mapping().first.empty());
      }
    } else {
      // !!! Scenario 3: The last transaction in the current binlog file are complete
      GtidLogEvent* event = gtid_events.back();
      _txn_gtid_seq = event->get_gtid_txn_id();
      if (OMS_OK != init_restart_point(index_record, event)) {
        delete rotate_event;
        return OMS_FAILED;
      }
    }
    rotate_event->set_op(RotateEvent::ROTATE);
    OMS_INFO("Safely recover, start timestamp: {}, current transaction: {} <> {}, start gtid: {}, filter: {}, filter "
             "checkpoint trx: {}",
        s_meta.cdc_config()->start_timestamp(),
        _txn_mapping.first,
        _txn_mapping.second,
        _txn_gtid_seq,
        _filter_util_checkpoint_trx,
        _filter_checkpoint_trx);
  }

  std::vector<GtidMessage*> previous_gtids;
  uint64_t last_gtid = (_filter_util_checkpoint_trx && _filter_checkpoint_trx) ? _txn_gtid_seq : _txn_gtid_seq - 1;
  merge_previous_gtids(last_gtid, previous_gtids);
  std::string previous_gtids_str = join_vector_str(previous_gtids, gtid_str_generator);
  std::string gtid_executed_str;
  g_sys_var->get_global_var("gtid_executed", gtid_executed_str);
  if (strcmp(previous_gtids_str.c_str(), gtid_executed_str.c_str()) != 0) {
    OMS_ERROR("[Unexpected behaviour] previous gtids inited by convert is not equal to gtid_executed: {} != {}",
        previous_gtids_str,
        gtid_executed_str);
    delete rotate_event;
    return OMS_FAILED;
  }
  rotate_event->set_next_previous_gtids(previous_gtids);

  rotate_event->get_header()->set_timestamp(s_meta.cdc_config()->start_timestamp());
  rotate_event->set_existed(rotate_existed);
  rotate_event->set_index(rotate_event->get_op() == RotateEvent::INIT ? next_index - 1 : next_index);
  rotate_event->set_next_binlog_file_name(
      CommonUtils::fill_binlog_file_name(rotate_event->get_op() == RotateEvent::INIT ? next_index - 1 : next_index));

  if (checksum_flag == OFF && rotate_event->get_checksum_flag() == CRC32) {
    // todo why?
    rotate_event->get_header()->set_event_length(
        rotate_event->get_header()->get_event_length() - rotate_event->get_checksum_len());

    rotate_event->get_header()->set_next_position(
        rotate_event->get_header()->get_next_position() - rotate_event->get_checksum_len());
    rotate_event->set_checksum_flag(OFF);
  }

  auto next_previous_gtids = previous_gtids_string(previous_gtids);
  OMS_INFO("Rotate current binlog: {}(type: {}), next previous gtids: {}",
      rotate_event->print_event_info(),
      rotate_event->get_op(),
      next_previous_gtids);

  _first_gtid_seq = 0;
  _binlog_file_index = rotate_event->get_op() == RotateEvent::INIT ? next_index - 1 : next_index;
  set_start_pos(previous_gtids);
  g_gtid_manager->init_start_timestamp(s_meta.cdc_config()->start_timestamp());

  OMS_INFO("Binlog convert init binlog file index: {}, start pos: {}", _binlog_file_index, this->_cur_pos);
  MsgBuf buffer;
  size_t buffer_offset = 0;
  defer(delete rotate_event);
  if (rotate(rotate_event, buffer, buffer_offset, index_record) != OMS_OK) {
    OMS_ERROR("Failed to rotate binlog file: {}(type: {}), next previous gtids: {}",
        rotate_event->print_event_info(),
        rotate_event->get_op(),
        next_previous_gtids);
    return OMS_FAILED;
  }
  return OMS_OK;
}

void BinlogStorage::merge_previous_gtids(uint64_t last_gtid, std::vector<GtidMessage*>& previous_gtids)
{
  OMS_INFO("Previous gtid before merging: [{}], and current executed gtid range: [{}, {}]",
      join_vector_str(_previous_gtid_messages, gtid_str_generator),
      _first_gtid_seq,
      last_gtid);
  merge_gtid_before(_previous_gtid_messages, txn_range(_first_gtid_seq, last_gtid));
  OMS_INFO("Next previous gtid after merging: {}", join_vector_str(_previous_gtid_messages, gtid_str_generator));

  for (auto const& pre_gtid : _previous_gtid_messages) {
    auto* gtid_msg = new GtidMessage();
    gtid_msg->copy_from(*(pre_gtid));
    previous_gtids.emplace_back(gtid_msg);
  }
}
void BinlogStorage::set_start_pos(const std::vector<GtidMessage*>& previous_gtids)
{
  std::uint32_t checksum = 0;
  if (Config::instance().binlog_checksum.val()) {
    checksum = COMMON_CHECKSUM_LENGTH;
  }
  _cur_pos = BINLOG_START_FIXED_POS + checksum;
  for (auto pre_gtid : previous_gtids) {
    _cur_pos += pre_gtid->get_gtid_length();
  }
  OMS_INFO("Start pos after set: {}", _cur_pos);
}

void purge_expired_binlog()
{
  // purge expired binlogs according to BINLOG_EXPIRE_LOGS_SECONDS and BINLOG_EXPIRE_LOGS_SIZE
  OMS_INFO("Asynchronous recycling of binlog files");
  BinlogIndexRecord index_record;
  g_index_manager->get_latest_index(index_record);
  oceanbase::binlog::BinlogStorage::purge_expired_binlog(index_record);
}

int64_t BinlogStorage::rotate_binlog_file(uint64_t timestamp, SerializeEvent& event)
{
  if (this->_cur_pos > s_meta.binlog_config()->max_binlog_size()) {
    this->_binlog_file_index++;
    _index_record.set_index(_binlog_file_index);
    _index_record.set_file_name(
        string(".") + BINLOG_DATA_DIR + binlog::CommonUtils::fill_binlog_file_name(this->_binlog_file_index));
    OMS_DEBUG("Next binlog file index rotation:{}", this->_binlog_file_index);
    auto* rotate_event = new RotateEvent(BINLOG_MAGIC_SIZE,
        binlog::CommonUtils::fill_binlog_file_name(this->_binlog_file_index),
        timestamp,
        this->_cur_pos);
    rotate_event->set_op(RotateEvent::ROTATE);
    rotate_event->set_index(this->_binlog_file_index);

    std::vector<GtidMessage*> previous_gtids;
    merge_previous_gtids(_txn_gtid_seq - 1, previous_gtids);

    // set start pos after rotate
    uint32_t pos_before_rotate = _cur_pos;
    set_start_pos(previous_gtids);

    OMS_INFO("[convert] Rotate to next binlog file: {}, pos_before_rotate: {}, max_binlog_size_bytes: {}, next start "
             "pos: {}, next previous gtids: {}",
        _binlog_file_index,
        pos_before_rotate,
        s_meta.binlog_config()->max_binlog_size(),
        _cur_pos,
        previous_gtids_string((previous_gtids)));

    rotate_event->set_next_previous_gtids(previous_gtids);
    _first_gtid_seq = 0;
    event.rotate_events.push_back(rotate_event);
    event.is_rotation = true;
    return OMS_OK;
  }
  return OMS_OK;
}
}  // namespace oceanbase::binlog
