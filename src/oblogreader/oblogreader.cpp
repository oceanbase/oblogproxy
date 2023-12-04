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

#include "log.h"
#include "counter.h"
#include "client_meta.h"
#include "oblogreader/oblogreader.h"

namespace oceanbase {
namespace logproxy {

ObLogReader::~ObLogReader()
{
  //  stop();
  //  join();
}

int ObLogReader::init(
    const std::string& id, MessageVersion packet_version, const ClientMeta& meta, const OblogConfig& config)
{
  Counter::instance().register_gauge("NRecordQ", [this]() { return _queue.size(); });

  // load different so library according to ob version
  int ret = ObCdcAccessFactory::load(config, _obcdc);
  if (ret != OMS_OK) {
    return ret;
  }

  ret = _sender.init(packet_version, meta.peer, _obcdc);
  if (ret != OMS_OK) {
    return ret;
  }
  return _reader.init(config, _obcdc);
}

int ObLogReader::stop()
{
  _reader.stop();
  _sender.stop();
  ObCdcAccessFactory::unload(_obcdc);
  Counter::instance().stop();
  return OMS_OK;
}

void ObLogReader::join()
{
  OMS_DEBUG("<<< Joining ObLogReader");
  _reader.join();
  _sender.join();
  Counter::instance().join();
  OMS_DEBUG(">>> Joined ObLogReader");
}

int ObLogReader::start()
{
  Counter::instance().start();
  _sender.start();
  _reader.start();
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
