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

#include <assert.h>

#include "LogMsgBuf.h"
#include "LogRecord.h"

#include "common/log.h"
#include "common/config.h"
#include "common/counter.h"
#include "codec/encoder.h"
#include "communication/communicator.h"
#include "oblogreader/oblogreader.h"
#include "oblogreader/sender_routine.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

SenderRoutine::SenderRoutine(OblogAccess& oblog, BlockingQueue<ILogRecord*>& rqueue)
    : Thread("SenderRoutine"), _oblog(oblog), _rqueue(rqueue)
{}

int SenderRoutine::init(MessageVersion packet_version, Channel* ch)
{
  _packet_version = packet_version;
  _client_peer = ch->peer();

  if (_s_config.readonly.val()) {
    return OMS_OK;
  }

  int ret = _comm.init();
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to init Sender Routine caused by failed to init communication, ret: " << ret;
    return ret;
  }

  ret = _comm.add_channel(_client_peer, ch);
  if (ret == OMS_FAILED) {
    OMS_ERROR << "Failed to init Sender Routine caused by failed to add channel, ret: " << ret
              << ", peer: " << _client_peer.id();
    return OMS_FAILED;
  }
  return OMS_OK;
}

void SenderRoutine::stop()
{
  if (is_run()) {
    Thread::stop();
    if (_s_config.readonly.val()) {
      return;
    }
    _comm.clear_channels();
    _comm.stop();
  }
}

void SenderRoutine::run()
{
  LogMsgLocalInit();

  std::vector<ILogRecord*> records;
  records.reserve(_s_config.read_wait_num.val());

  while (is_run()) {

    if (!_s_config.readonly.val()) {
      int ret = _comm.poll();
      if (ret != OMS_OK) {
        OMS_ERROR << "Failed to run Sender Routine caused by failed to poll communication, ret: " << ret;
        stop();
        break;
      }
    }

    _stage_timer.reset();
    records.clear();
    while (!_rqueue.poll(records, _s_config.read_timeout_us.val()) || records.empty()) {
      OMS_INFO << "send transfer queue empty, retry...";
    }
    int64_t poll_us = _stage_timer.elapsed();
    Counter::instance().count_key(Counter::SENDER_POLL_US, poll_us);

    for (size_t i = 0; i < records.size(); ++i) {
      ILogRecord* record = records[i];
      assert(record != nullptr);

      if (_s_config.verbose_packet.val()) {
        OMS_INFO << "fetch a records(" << (i + 1) << "/" << records.size() << ") from liboblog: "
                 << "size:" << record->getRealSize() << ", record_type:" << record->recordType()
                 << ", timestamp:" << record->getTimestamp() << ", checkpoint:" << record->getCheckpoint1()
                 << ", dbname:" << record->dbname() << ", tbname:" << record->tbname()
                 << ", queue size:" << _rqueue.size(false);
      }
      if (_s_config.readonly.val()) {
        Counter::instance().count_write(1);
        Counter::instance().mark_timestamp(record->getTimestamp());
        Counter::instance().mark_checkpoint(record->getCheckpoint1());
        _oblog.release(record);
      }
    }

    if (_s_config.readonly.val()) {
      continue;
    }

    size_t packet_size = 0;
    size_t offset = 0;
    size_t i = 0;
    for (i = 0; i < records.size(); ++i) {
      ILogRecord* r = records[i];
      size_t size = 0;
      const char* rbuf = r->toString(&size, true);
      if (rbuf == nullptr) {
        OMS_ERROR << "failed parse logmsg Record, !!!EXIT!!!";
        stop();
        break;
      }

      if (packet_size + size > _s_config.max_packet_bytes.val()) {
        if (packet_size == 0) {
          OMS_WARN << "Huge package occured, size of: " << size
                   << " just exceed max_packet_bytes: " << _s_config.max_packet_bytes.val() << ", try to send";
          if (do_send(records, i, 1) != OMS_OK) {
            OMS_ERROR << "Failed to write LogMessage to client: " << _client_peer.to_string();
            stop();
            break;
          }
          offset = i + 1;

        } else {

          if (do_send(records, offset, i - offset) != OMS_OK) {
            OMS_ERROR << "Failed to write LogMessage to client: " << _client_peer.to_string();
            stop();
            break;
          }

          offset = i;
        }
        packet_size = 0;
        continue;
      }
      packet_size += size;
    }

    if (packet_size > 0) {
      if (do_send(records, offset, i - offset) != OMS_OK) {
        OMS_ERROR << "Failed to write LogMessage to client: " << _client_peer.to_string();
        stop();
      }
    }

    for (ILogRecord* r : records) {
      _oblog.release(r);
    }
  }

  LogMsgLocalDestroy();
  ObLogReader::instance().stop();
}

int SenderRoutine::do_send(const std::vector<ILogRecord*>& records, size_t offset, size_t count)
{
  if (_s_config.verbose.val()) {
    OMS_DEBUG << "send record range[" << offset << ", " << offset + count << ")";
  }

  _stage_timer.reset();
  RecordDataMessage msg(records, offset, count);
  msg.set_version(_packet_version);
  msg.compress_type = CompressType::LZ4;
  int ret = _comm.send_message(_client_peer, msg, true);
  Counter::instance().count_key(Counter::SENDER_SEND_US, _stage_timer.elapsed());

  if (ret == OMS_OK) {
    ILogRecord* last = records[offset + count - 1];
    Counter::instance().count_write(count);
    Counter::instance().mark_timestamp(last->getTimestamp());
    Counter::instance().mark_checkpoint(last->getCheckpoint1());
  } else {
    OMS_WARN << "Failed to send record data message to client. peer=" << _client_peer.id();
  }
  return ret;
}

}  // namespace logproxy
}  // namespace oceanbase
