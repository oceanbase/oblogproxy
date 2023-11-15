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

#include "obaccess/clog_meta_routine.h"
#include "config.h"
#include "log.h"
#include "timer.h"

namespace oceanbase {
namespace logproxy {
#define FETCH_CLOG_MIN_TS_SQL \
  "SELECT svr_min_log_timestamp FROM oceanbase.__all_virtual_server_clog_stat WHERE zone_status='ACTIVE';"

void ClogMetaRoutine::stop()
{
  if (is_run()) {
    Thread::stop();
  }
}

int ClogMetaRoutine::init(const OblogConfig& config)
{
  if (!Config::instance().check_clog_enable.val()) {
    OMS_STREAM_WARN << "check clog is in the disable state, so exit the clog timing verification program";
    _available = false;
    return OMS_OK;
  }
  _oblog_config = config;
  int ret = _ob_access.init(config, config.password_sha1, config.sys_password_sha1);
  if (ret != OMS_OK) {
    return ret;
  }

  ret = _ob_access.fetch_connection(_sys_auther);
  if (ret != OMS_OK) {
    return ret;
  }

  // Check if the svr_min_log_timestamp column exists in __all_virtual_server_clog_stat
  MySQLResultSet rs;
  ret = _sys_auther.query(FETCH_CLOG_MIN_TS_SQL, rs);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to check the existence of svr_min_log_timestamp column in __all_virtual_server_clog_stat, "
                 "disable clog check";
    _available = false;
  }
  return OMS_OK;
}

void ClogMetaRoutine::run()
{
  Timer timer;
  timer.reset();
  while (is_run() && _available) {
    uint64_t min_clog_timestamp_us = 0;
    int ret = fetch_once(min_clog_timestamp_us);
    if (ret == OMS_CONNECT_FAILED) {
      // When a connection error occurs, it means that there is already a problem with the connection and needs to be
      // re-established, so the previous connection needs to be closed here
      _sys_auther.close();
      // Re-request the config server to obtain the corresponding available observer address
      this->init(_oblog_config);
      // try to fetch new connection
      ret = _ob_access.fetch_connection(_sys_auther);
      OMS_STREAM_WARN << "Try to fetch new connection:" << ret;
      if (ret != OMS_OK) {
        _sys_auther.close();
      }
      timer.sleep(Config::instance().ob_clog_fetch_interval_s.val() * 1000000);
      continue;
    }
    if (min_clog_timestamp_us != 0) {
      _min_clog_timestamp_us = min_clog_timestamp_us;
      OMS_STREAM_INFO << "min clog timestamp in us: " << _min_clog_timestamp_us;
    }
    timer.sleep(Config::instance().ob_clog_fetch_interval_s.val() * 1000000);
  }
}

int ClogMetaRoutine::fetch_once(uint64_t& min_clog_timestamp_us)
{
  if (!_available) {
    return OMS_OK;
  }

  MySQLResultSet rs;
  int ret = _sys_auther.query(FETCH_CLOG_MIN_TS_SQL, rs);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to fetch clog timestamps, code: " << rs.code << ", error: " << rs.message;
    return ret;
  }

  min_clog_timestamp_us = 0;
  for (const MySQLRow& row : rs.rows) {
    int64_t tmp = atoll(row.fields().front().c_str());
    // The query result will contain -1, indicating that there is currently no clog related information. So when the
    // result value is less than 0, the clog start point is considered to be 0
    if (tmp < 0) {
      tmp = 0;
    }
    min_clog_timestamp_us = std::max(min_clog_timestamp_us, (uint64_t)tmp);
  }
  return OMS_OK;
}

bool ClogMetaRoutine::check(uint64_t clog_timestamp_us)
{
  uint64_t now_time = Timer::now();
  bool ret = !_available || clog_timestamp_us == 0 || _min_clog_timestamp_us == 0 ||
             clog_timestamp_us >= _min_clog_timestamp_us;
  if (ret) {
    _last_check_timestamp_us = now_time;
  } else {
    if (now_time - _last_check_timestamp_us >= (Config::instance().ob_clog_expr_s.val() * 1000000)) {
      return false;
    }
  }
  return true;
}

bool ClogMetaRoutine::check(const OblogConfig& config, std::string& errmsg)
{
  fetch_once(_min_clog_timestamp_us);
  bool ret = (config.start_timestamp_us.val() != 0) ? check(config.start_timestamp_us.val())
                                                    : check(config.start_timestamp.val() * 1000000);
  if (!ret) {
    errmsg =
        "Invalid start timestamp while current min clog timestamp in us: " + std::to_string(_min_clog_timestamp_us);
  }
  return ret;
}

}  // namespace logproxy
}  // namespace oceanbase
