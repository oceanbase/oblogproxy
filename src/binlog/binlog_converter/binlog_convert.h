/**
 * Copyright (c) 2022 OceanBase
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

#ifndef OMS_LOGPROXY_BINLOG_CONVERT_H
#define OMS_LOGPROXY_BINLOG_CONVERT_H

#include "thread.h"
#include "timer.h"
#include "blocking_queue.hpp"
#include "str_array.h"
#include "oblog_config.h"
#include "codec/message.h"
#include "obcdcaccess/obcdc_factory.h"

#include "ob_log_event.h"
#include "convert_meta.h"
#include "binlog_index.h"

namespace oceanbase {
namespace logproxy {
#define RECOVER_BACKUP_PATH "/recover_backup/"
class BinlogConverter;
class MsgBuf;
class BinlogConvert : public Thread {
public:
  BinlogConvert(
      BinlogConverter& converter, BlockingQueue<ILogRecord*>& rqueue, BlockingQueue<ObLogEvent*>& event_queue);

  int init(ConvertMeta& meta, OblogConfig& config, IObCdcAccess* oblog);

  void stop() override;

  void run() override;

  const ConvertMeta& get_meta() const;

  void set_meta(const ConvertMeta& meta);

  void append_event(BlockingQueue<ObLogEvent*>& queue, ObLogEvent* event);

  void convert_gtid_log_event(ILogRecord* record);

  void convert_query_event(ILogRecord* record);

  void convert_xid_event(ILogRecord* record);

  void convert_table_map_event(ILogRecord* record);

  void convert_write_rows_event(ILogRecord* record);

  void convert_delete_rows_event(ILogRecord* record);

  void convert_update_rows_event(ILogRecord* record);

  void do_convert(const std::vector<ILogRecord*>& records);

  void get_before_images(ILogRecord* record, int col_count, MsgBuf& col_data) const;

  void get_after_images(ILogRecord* record, int col_count, MsgBuf& col_data) const;

  int consume_exactly_once(const ConvertMeta& meta, OblogConfig& config);

  /*!
   * Perform recovery processing when the binlog service goes down. The detailed action is: open the last binlog in the
   *index file. If the file is not closed normally (LOG_EVENT_BINLOG_IN_USE_F is set), recover it . Start from scratch
   *and scan each Binlog Event one by one. As long as a Binlog transaction is found to be incomplete, the Binlog and the
   *Binlog following it will be truncated.
   * @param meta
   * @param config
   * @return
   */
  int recover(const ConvertMeta& meta, OblogConfig& config);

  /*!
   * When recovering and trimming binlog files, back up the last binlog file and index file.
   * @param binlog
   * @param binlog_index
   * @param config
   * @return index binlog file suffix
   */
  int backup(const std::string& binlog, const std::string& binlog_index, const OblogConfig& config, uint16_t index);

private:
  BinlogConverter& _converter;
  IObCdcAccess* _oblog;
  BlockingQueue<ILogRecord*>& _rqueue;
  BlockingQueue<ObLogEvent*>& _event_queue;
  MessageVersion _packet_version;
  Timer _stage_timer;
  uint64_t _txn_id = 0;
  uint64_t _xid = 0;
  uint32_t _cur_pos;
  uint64_t _binlog_file_index = 1;
  std::pair<uint64_t, std::string> _txn_mapping;
  ConvertMeta _meta;
  bool _filter = true;
  bool _specified_gtids = false;
  /*!
   * Whether it is within the last complete transaction, that is, the last complete transaction is found and filtering
   * starts
   */
  bool _within_filtered_transactions = false;
  int64_t _skip_record_num = 0;
  uint64_t _start_timestamp = 0;
};

void fill_bitmap(int col_count, int col_bytes, unsigned char* bitmap);

}  // namespace logproxy
}  // namespace oceanbase

#endif  // OMS_LOGPROXY_BINLOG_CONVERT_H
