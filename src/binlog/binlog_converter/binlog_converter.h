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

#include "common.h"
#include "oblog_config.h"
#include "obcdcaccess/obcdc_factory.h"
#include "obaccess/ob_access.h"
#include "binlog_storage.h"
#include "binlog_convert.h"
#include "clog_reader_routine.h"
#include "convert_meta.h"

namespace oceanbase {
namespace logproxy {
class BinlogConverter {
public:
  virtual ~BinlogConverter();

  int init(MessageVersion packet_version, OblogConfig& config);

  int stop();

  void join();

  int start();

  ConvertMeta& get_meta();

  void set_meta(ConvertMeta meta);

  void cancel();

  /*!
   * @brief Query server uuid information from observer
   * @param config
   * @return
   */
  int query_server_uuid(OblogConfig& config);

private:
  IObCdcAccess* _oblog = nullptr;
  BlockingQueue<ILogRecord*> _queue{Config::instance().record_queue_size.val()};
  BlockingQueue<ObLogEvent*> _event_queue{Config::instance().record_queue_size.val()};
  ClogReaderRoutine _reader{*this, _queue};
  BinlogConvert _convert{*this, _queue, _event_queue};
  BinlogStorage _storage{*this, _event_queue};
  ConvertMeta _meta;
  ObAccess _ob_access;
};
}  // namespace logproxy
}  // namespace oceanbase
