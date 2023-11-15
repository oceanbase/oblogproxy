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

#include <cassert>

#include "logmsg_buf.h"
#include "log_record.h"

#include "log.h"
#include "config.h"
#include "counter.h"
#include "codec/encoder.h"
#include "communication/comm.h"
#include "oblogreader/oblogreader.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_config = Config::instance();

// #ifndef COMMUNITY_BUILD
static __thread LogMsgBuf* _t_s_lmb = nullptr;
#define LogMsgLocalInit                                         \
  if ((_t_s_lmb = new (std::nothrow) LogMsgBuf()) == nullptr) { \
    OMS_ERROR("Failed to alloc LogMsgBuf");                     \
    stop();                                                     \
    return;                                                     \
  }
#define LogMsgLocalDestroy delete _t_s_lmb
// #endif

SenderRoutine::SenderRoutine(ObLogReader& reader, BlockingQueue<ILogRecord*>& rqueue)
    : Thread("SenderRoutine"), _reader(reader), _obcdc(nullptr), _rqueue(rqueue)
{}

int SenderRoutine::init(MessageVersion packet_version, const Peer& peer, IObCdcAccess* obcdc)
{
  _obcdc = obcdc;
  _packet_version = packet_version;
  _client_peer = peer;

  if (_s_config.readonly.val()) {
    return OMS_OK;
  }

  // init comm
  int ret = ChannelFactory::instance().init(Config::instance());
  if (OMS_OK != ret) {
    OMS_ERROR("Failed to init channel factory");
    return OMS_FAILED;
  }

  ret = _comm.init();
  if (OMS_OK != ret) {
    OMS_ERROR("Failed to init Sender Routine caused by failed to init communication, ret: {}", ret);
    return ret;
  }

  //  _comm.set_write_callback();
  ret = _comm.add(peer);
  if (ret == OMS_FAILED) {
    OMS_ERROR(
        "Failed to init Sender Routine caused by failed to add channel, ret: {}, peer: {}", ret, _client_peer.id());
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
    _comm.stop();
  }
}

void SenderRoutine::run()
{
  LogMsgLocalInit;

  std::vector<ILogRecord*> records;
  records.reserve(_s_config.read_wait_num.val());

  while (is_run()) {
    if (!_s_config.readonly.val()) {
      int ret = _comm.poll();
      if (ret != OMS_OK) {
        OMS_ERROR("Failed to run Sender Routine caused by failed to poll communication, ret: {}", ret);
        stop();
        break;
      }
    }

    _stage_timer.reset();
    records.clear();
    while (!_rqueue.poll(records, _s_config.read_timeout_us.val()) || records.empty()) {
      OMS_INFO("Send transfer queue empty, retry...");
    }
    int64_t poll_us = _stage_timer.elapsed();
    Counter::instance().count_key(Counter::SENDER_POLL_US, poll_us);

    if (_s_config.readonly.val()) {
      for (auto record : records) {
        assert(record != nullptr);
        Counter::instance().count_write(1);
        Counter::instance().mark_timestamp(record->getTimestamp() * 1000000 + record->getRecordUsec());
        Counter::instance().mark_checkpoint(record->getCheckpoint1() * 1000000 + record->getCheckpoint2());
        _obcdc->release(record);
      }
      continue;
    }

    size_t packet_size = 0;
    size_t offset = 0;
    size_t i = 0;
    for (i = 0; i < records.size(); ++i) {
      ILogRecord* r = records[i];
      size_t size = 0;
      // #ifdef COMMUNITY_BUILD
      const char* rbuf = r->toString(&size, _t_s_lmb, true);
      // #else
      //       const char* rbuf = r->toString(&size, true);
      // #endif
      if (rbuf == nullptr) {
        OMS_ERROR("Failed to parse logmsg Record, !!!EXIT!!!");
        stop();
        break;
      }

      if (packet_size + size > _s_config.max_packet_bytes.val()) {
        if (packet_size == 0) {
          OMS_WARN("Huge package occurred with size of: {}, exceed max_packet_bytes: {}, try to send directly.",
              size,
              _s_config.max_packet_bytes.val());
          if (do_send(records, i, 1) != OMS_OK) {
            OMS_ERROR("Failed to write LogMessage to client: {}", _client_peer.to_string());
            stop();
            break;
          }
          offset = i + 1;
        } else {
          if (do_send(records, offset, i - offset) != OMS_OK) {
            OMS_ERROR("Failed to write LogMessage to client: {}", _client_peer.to_string());
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

    if (is_run() && packet_size > 0) {
      if (do_send(records, offset, i - offset) != OMS_OK) {
        OMS_ERROR("Failed to write LogMessage to client: {}", _client_peer.to_string());
        stop();
      }
    }

    for (ILogRecord* r : records) {
      _obcdc->release(r);
    }
  }

  LogMsgLocalDestroy;
  _reader.stop();
}

int SenderRoutine::do_send(std::vector<ILogRecord*>& records, size_t offset, size_t count)
{
  if (_s_config.verbose.val()) {
    OMS_DEBUG("send record range[{}, {}]", offset, offset + count);
  }

  RecordDataMessage msg(records, offset, count);
  msg.set_version(_packet_version);
  msg.compress_type = CompressType::LZ4;
  msg.idx = _msg_seq;
  int ret = _comm.send_message(_client_peer, msg, true);

  _msg_seq += count;

  if (ret == OMS_OK) {
    ILogRecord* last = records[offset + count - 1];
    Counter::instance().count_write(count);
    Counter::instance().mark_timestamp(last->getTimestamp() * 1000000 + last->getRecordUsec());
    Counter::instance().mark_checkpoint(last->getCheckpoint1() * 1000000 + last->getCheckpoint2());
  } else {
    OMS_WARN("Failed to send record data message to client, peer: {}", _client_peer.id());
  }
  return ret;
}

}  // namespace logproxy
}  // namespace oceanbase
