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

#include <utility>
#include "counter.h"

#include "binlog_converter.h"

namespace oceanbase {
namespace logproxy {
BinlogConverter::~BinlogConverter()
{
  stop();
}
int BinlogConverter::init(MessageVersion packet_version, OblogConfig& config)
{
  Counter::instance().register_gauge("RecordQueueSize", [this]() { return _queue.size(); });
  OMS_STREAM_INFO << "config:" << config.generate_config();
  int ret;

  std::string server_uuid = config.server_uuid.val();
  if (server_uuid.empty()) {
    ret = query_server_uuid(config);
    if (ret != OMS_OK) {
      return ret;
    }
  } else {
    /*!
     * @brief When the server uuid is specified, the specified one shall prevail.
     */
    this->get_meta().server_uuid = server_uuid;
  }
  OMS_STREAM_INFO << "server uuid:" << this->get_meta().server_uuid;

  // load different so library according to ob version
  ret = ObCdcAccessFactory::load(config, _oblog);
  if (OMS_OK != ret) {
    return ret;
  }

  if (this->get_meta().first_start_timestamp == 0) {
    // if no time point is specified, initialize the current time point
    this->get_meta().first_start_timestamp = Timer::now();
  }
  ret = _storage.init(this->get_meta(), _oblog, config);
  if (ret != OMS_OK) {
    return ret;
  }
  OMS_STREAM_INFO << "Storage initialized successfully";

  ret = _convert.init(get_meta(), config, _oblog);
  if (ret != OMS_OK) {
    return ret;
  }
  OMS_STREAM_INFO << "Convert initialized successfully";
  config.start_timestamp_us.set(get_meta().first_start_timestamp);
  OMS_STREAM_INFO << "init liboblog start_timestamp_us:" << config.start_timestamp_us.val();
  return _reader.init(config, _oblog);
}

int BinlogConverter::query_server_uuid(OblogConfig& config)
{
  string tenant = config.tenant.val();
  int ret = _ob_access.init(config, config.password_sha1, config.sys_password_sha1);
  if (ret != OMS_OK) {
    return ret;
  }
  MysqlProtocol sys_auther;
  _ob_access.fetch_connection(sys_auther);

  MySQLResultSet rs;
  ret = sys_auther.query("SELECT value FROM oceanbase.__all_virtual_sys_variable WHERE tenant_id = (SELECT tenant_id "
                         "FROM oceanbase.__all_tenant WHERE tenant_name = '" +
                             tenant + "') AND name = 'server_uuid';",
      rs);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to query server uuid, ret:" << ret;
    return OMS_FAILED;
  }

  if (rs.rows.empty()) {
    OMS_STREAM_ERROR << "Failed to query server uuid, result is empty";
    return OMS_FAILED;
  }

  if (rs.rows.at(0).col_count() != 1) {
    OMS_STREAM_ERROR << "Failed to query server uuid, result is empty";
    return OMS_FAILED;
  }
  get_meta().server_uuid = rs.rows.at(0).fields().at(0);
  return OMS_OK;
}
int BinlogConverter::stop()
{
  _reader.stop();
  _convert.stop();
  _storage.stop();
  ObCdcAccessFactory::unload(_oblog);
  Counter::instance().stop();
  return OMS_OK;
}
void BinlogConverter::join()
{
  _reader.detach();
  _convert.join();
  _storage.join();
}
int BinlogConverter::start()
{
  Counter::instance().start();
  _storage.start();
  _convert.start();
  _reader.start();
  return OMS_OK;
}

ConvertMeta& BinlogConverter::get_meta()
{
  return _meta;
}

void BinlogConverter::set_meta(ConvertMeta meta)
{
  _meta = std::move(meta);
}

void BinlogConverter::cancel()
{
  _reader.set_run(false);
  _convert.set_run(false);
  _storage.set_run(false);
}

}  // namespace logproxy
}  // namespace oceanbase
