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

#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <sys/inotify.h>
#include "thread.h"
#include "msg_buf.h"
#include "obaccess/ob_mysql_packet.h"
#include "convert_meta.h"
#include "connection.h"
#include "ob_log_event.h"
#include "timer.h"
#include "binlog_index.h"
#include "counter.h"

namespace oceanbase {
namespace logproxy {
#define BINLOG_FATAL_ERROR 1236
#define BINLOG_ERROR_WHEN_EXECUTING_COMMAND 1220

typedef void (*process_binlog_event)(MsgBuf, void*);

using IoResult = binlog::Connection::IoResult;

class DumperMetric {

public:
  /*!
   * @brief Update the latest timestamp of the sent event, in s
   * @param ts
   */
  void mark_checkpoint_ts(int64_t ts);

  std::uint64_t rps();

  std::uint64_t iops();

  std::uint64_t delay();

  std::uint64_t checkpoint();

  void count_send(uint64_t count = 1);

  void count_send_io(uint64_t bytes);

private:
  std::atomic<uint64_t> _send_count{0};
  std::atomic<uint64_t> _send_io{0};
  volatile uint64_t _checkpoint_ts = 0;
  int64_t interval_s = Config::instance().counter_interval_s.val();
};

class BinlogDumper : public Thread {
public:
  const uint32_t BINLOG_DUMP_NON_BLOCK = 1 << 0;
  ~BinlogDumper() override;

  BinlogDumper();

  void stop() override;

  void run() override;

  /*
   * @params
   * @returns
   * @description send a fake rotate event to the client
   * @date 2022/9/21 16:05
   */
  IoResult send_fake_rotate_event(const std::string& file_name, uint64_t start_pos);

  /*
   * @params pos the pos of the currently sent event
   * @returns
   * @description
   * @date 2022/9/21 20:30
   */
  IoResult send_heartbeat_event(uint64_t pos);

  /*
   * @params file binlog file to be sent
   * @returns
   * @description get the format_description_event in the file and send it to the client
   * @date 2022/9/21 21:01
   */
  IoResult send_format_description_event(FILE* stream, const std::string& file);

  /*
   * @params
   * @returns EventType
   * @description read an event from the Binlog file and fill it into msg_buf
   * @date 2022/9/21 21:47
   */
  int seek_event(FILE* stream, MsgBuf& msg_buf, bool& skip_record);

  /*
   * @params
   * @returns
   * @description
   * @date 2022/9/21 16:39
   */
  IoResult send_packet();

  /*
   * @params
   * @returns
   * @description get the latest pos of the current binlog file
   * @date 2022/9/23 16:07
   */
  int seek_binlog_end_pos(const std::string& file, uint64_t& end_pos);

  /*
   * @params file name
   * @returns
   * @description determine whether the current binlog file will continue to be written
   * @date 2022/9/23 17:16
   */
  bool is_active(const std::string& file);

  /*
   * @params
   * @returns
   * @description block waiting for a new event to be generated
   * @date 2022/9/26 15:33
   */
  int notify_new_event(const std::string& file);

  /*
   * @params
   * @returns
   * @description
   * wait for a new event to be generated, and send heartbeat events during each interval if heartbeat is turned on.
   * If the heartbeat is not turned on, it will block and wait until a new event is generated
   * @date 2022/9/26 16:09
   */
  int wait_event(const std::string& file, uint64_t pos, uint64_t& new_pos);

  /*
   * @params
   * @returns
   * @description send events in the range from start_pos to end_pos
   * @date 2022/9/26 17:11
   */
  IoResult send_events(uint64_t start_pos, uint64_t end_pos);

  /*
   * @params
   * @returns
   * @description send events in the range from start_pos to end_pos
   * @date 2022/9/26 17:11
   */
  int send_events(const std::string& file, uint64_t start_pos, uint64_t end_pos, process_binlog_event process_func);

  /*
   * @params
   * @returns
   * @description send binlog file from start_pos to client
   * @date 2022/9/26 17:12
   */
  IoResult send_binlog(const std::string& file, uint64_t start_pos);

  /*
   * @params skip_record, if the current event is a non-rotate event and a gtid event,
   * judge whether to skip the record according to skip_record
   * @returns event need to skip
   * @description
   * @date 2022/9/27 20:40
   */
  bool skip_event(OblogEventHeader& header, unsigned char* buff, bool skip_record);

  /*!
   * @brief Subscribe to the s-level timestamp of the record and record size
   * @param ts
   * @param bytes
   */
  void mark_metrics(uint64_t ts, uint64_t bytes);

  int seek_next_binlog();

  const string& get_file() const;

  void set_file(string file);

  uint64_t get_start_pos() const;

  void set_start_pos(uint64_t start_pos);

  bool is_gtid_mod() const;

  void set_gtid_mod(bool gtid_mod);

  const std::map<std::string, GtidMessage*>& get_exclude_gtid() const;

  void set_exclude_gtid(std::map<std::string, GtidMessage*> exclude_gtid);

  const pair<std::string, uint64_t>& get_checkpoint() const;

  void set_checkpoint(pair<string, uint64_t> checkpoint);

  uint32_t get_flag() const;

  void set_flag(uint32_t flag);

  uint64_t get_heartbeat_interval_us() const;

  void set_heartbeat_interval_us(uint64_t heartbeat_interval_us);

  const ConvertMeta& get_meta() const;

  void set_meta(ConvertMeta meta);

  binlog::Connection* get_connection() const;

  void set_connection(binlog::Connection* connection);

  const string& get_relative_file() const;

  void set_relative_file(const string& relative_file);

  void is_subset_executed_gtid(
      map<std::string, GtidMessage*>& gtids, bool& is_subset, BinlogIndexRecord* p_index_record);

  int seek_first_binlog_file();

  void register_latency();

  /*!
   * @brief Determine whether the current binlog event that has been placed on the disk is complete
   * @param stream
   * @param offset
   * @return
   */
  bool is_legal_event(FILE* stream, uint64_t offset, uint64_t end_pos) const;

  /*!
   * \brief Verify whether the subscribed offset is legal
   * \param file
   * \param offset
   * \return
   */
  bool verify_subscription_offset(uint64_t offset);

  /*!
   * @brief Whether the rotation file is ready
   * @param file
   * @return
   */
  void wait_rotate_ready(const std::string& file) const;
  /*!
   * @brief Get the cycle of binlog sending heartbeat,Unit is nanosecond
   * @return
   */
  int64_t get_heartbeat_period();

  /*!
   * @brief Initialize checksum parameters by the current session
   */
  void init_binlog_checksum();

  /*!
   * @brief Get the checksum parameter
   * @return
   */
  enum_checksum_flag get_binlog_checksum();

  /*!
   * @brief set the checksum parameter
   * @return
   */
  void set_binlog_checksum(enum_checksum_flag checksum_flag);

  /*!
   * @brief Check if the connection is alive
   * @return
   */
  int conn_liveness();

  /*!
   * @brief Compare the gtid event to determine
   * whether the transaction data corresponding to the gtid should be skipped
   * @param buff
   * @return
   */
  bool handle_gtid_event(unsigned char* buff);

private:
  std::string _file;
  std::string _relative_file;
  uint64_t _start_pos = 0;
  bool _gtid_mod = false;
  std::map<std::string, GtidMessage*> _exclude_gtid;
  //<binlog_file,pos>
  std::pair<std::string, uint64_t> _checkpoint;
  // BINLOG_DUMP_NON_BLOCK
  uint32_t _flag{};
  MsgBuf _packet;
  uint64_t _heartbeat_interval_us = 1000000;
  std::ifstream _stream;
  ConvertMeta _meta;
  std::string _error_message;
  FILE* _fp = nullptr;
  binlog::Connection* _connection;
  Timer _stage_timer;
  CounterStatistics _counter;
  DumperMetric _metric;
  int64_t _checkpoint_ts;
  enum_checksum_flag _checksum_flag = UNDEF;
  std::string _rotate_file = "";
};
}  // namespace logproxy
}  // namespace oceanbase
