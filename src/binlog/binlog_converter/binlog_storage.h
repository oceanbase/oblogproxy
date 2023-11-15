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

#pragma once
#ifndef OMS_LOGPROXY_BINLOG_STORAGE_H
#define OMS_LOGPROXY_BINLOG_STORAGE_H

#include <cassert>
#include "thread.h"
#include "timer.h"
#include "log.h"
#include "ob_log_event.h"
#include "blocking_queue.hpp"
#include "obcdcaccess/obcdc/obcdc_entry.h"
#include "convert_meta.h"
#include "binlog_index.h"
#include "data_type.h"
#include "oblog_config.h"

namespace oceanbase {
namespace logproxy {
class BinlogConverter;

class BinlogStorage : public Thread {
public:
  int init(ConvertMeta& meta, IObCdcAccess* oblog, OblogConfig& config);

  void stop() override;

  void run() override;

  BinlogStorage(BinlogConverter& reader, BlockingQueue<ObLogEvent*>& event_queue);

  const ConvertMeta& get_meta() const;

  void set_meta(const ConvertMeta& meta);

  size_t rotate(ObLogEvent* event, MsgBuf& content, std::size_t size, BinlogIndexRecord& index_record,
      const string& index_file_name);

  int storage_binlog_event(vector<ObLogEvent*>& records, const string& index_file_name, MsgBuf& buffer,
      size_t& buffer_pos, BinlogIndexRecord& index_record);

  int finishing_touches(const string& index_file_name, BinlogIndexRecord& index_record, size_t record_count,
      MsgBuf& buffer, size_t& buffer_pos);
  /*
   The rotation action is for the previous binlog file
   */
  int previous_rotation(MsgBuf& content, size_t size, RotateEvent* rotate_event, BinlogIndexRecord& index_record,
      const string& index_file_name);

  /*
   *The rotation action is for the current binlog file
   */
  int current_rotation(BinlogIndexRecord& index_record, const RotateEvent* rotate_event);

private:
  BlockingQueue<ObLogEvent*>& _event_queue;
  //  FILE* _file;
  std::string _file_name;
  Timer _stage_timer;
  BinlogConverter& _converter;
  IObCdcAccess* _oblog;
  ConvertMeta _meta;
  uint64_t _offset;
  txn_range _range;
};
}  // namespace logproxy
}  // namespace oceanbase

#endif  // OMS_LOGPROXY_BINLOG_STORAGE_H
