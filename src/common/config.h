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

#pragma once

#include <string>
#include <stdint.h>
#include "common/common.h"
#include "common/config_base.h"

namespace oceanbase {
namespace logproxy {

enum LogType {
  /**
   * OceanBase LogReader
   */
  OCEANBASE = 0,

};

class Config : protected ConfigBase {
  OMS_SINGLETON(Config);
  OMS_AVOID_COPY(Config);

public:
  virtual ~Config() = default;

  int load(const std::string& file);

  /**
   * add a config KV , if no structure key exist, add given KV to extras_.
   */
  void add(const std::string& key, const std::string& value);

  std::string debug_str(bool formatted = false) const;

public:
  OMS_CONFIG_UINT16(service_port, 2983);
  OMS_CONFIG_UINT32(encode_threadpool_size, 8);
  OMS_CONFIG_UINT32(encode_queue_size, 50000);
  OMS_CONFIG_UINT32(max_packet_bytes, 1024 * 1024 * 8);  // 8MB
  OMS_CONFIG_UINT32(command_timeout_s, 10);

  OMS_CONFIG_UINT32(record_queue_size, 1024);
  OMS_CONFIG_UINT64(read_timeout_us, 2000000);
  OMS_CONFIG_UINT64(read_fail_interval_us, 1000000);
  OMS_CONFIG_UINT32(read_wait_num, 20000);

  OMS_CONFIG_UINT64(send_timeout_us, 2000000);
  OMS_CONFIG_UINT64(send_fail_interval_us, 1000000);

  OMS_CONFIG_BOOL(log_to_stdout, false);
  OMS_CONFIG_UINT32(log_quota_size_mb, 5120);
  OMS_CONFIG_UINT32(log_quota_day, 30);
  OMS_CONFIG_UINT32(log_gc_interval_s, 43200);

  OMS_CONFIG_STR(oblogreader_path, "/home/ds/logreader");
  OMS_CONFIG_UINT32(oblogreader_path_retain_hour, 168);  // 7 Days
  OMS_CONFIG_UINT32(oblogreader_lease_s, 300);           // 5 mins

  OMS_CONFIG_BOOL(allow_all_tenant, false);
  OMS_CONFIG_BOOL(auth_user, true);
  OMS_CONFIG_BOOL(auth_use_rs, false);
  OMS_CONFIG_BOOL(auth_allow_sys_user, false);

  OMS_CONFIG_ENCRYPT(ob_sys_username, "");
  OMS_CONFIG_ENCRYPT(ob_sys_password, "");

  OMS_CONFIG_UINT32(counter_interval_s, 2);   // 2s
  OMS_CONFIG_UINT32(metric_interval_s, 120);  // 2mins

  OMS_CONFIG_STR(communication_mode, "server");  // server mode or client mode

  // plain, tls refer ChannelFactory::init
  OMS_CONFIG_STR(channel_type, "plain");
  OMS_CONFIG_STR(tls_ca_cert_file, "");
  OMS_CONFIG_STR(tls_cert_file, "");
  OMS_CONFIG_STR(tls_key_file, "");
  OMS_CONFIG_BOOL(tls_verify_peer, true);

  // tls between observer and liboblog
  OMS_CONFIG_BOOL(liboblog_tls, true);
  OMS_CONFIG_STR(liboblog_tls_cert_path, "");

  // debug related
  OMS_CONFIG_BOOL(debug, false);           // enable debug mode
  OMS_CONFIG_BOOL(verbose, false);         // print more log
  OMS_CONFIG_BOOL(verbose_packet, false);  // print data packet info
  OMS_CONFIG_BOOL(readonly, false);        // only read from LogReader, use for test
  OMS_CONFIG_BOOL(count_record, false);

  // for inner use
  OMS_CONFIG_UINT64(process_name_address, 0);
  OMS_CONFIG_BOOL(packet_magic, true);

protected:
  // string KV extra configs
  std::map<std::string, std::string> extras_;
};

}  // namespace logproxy
}  // namespace oceanbase
