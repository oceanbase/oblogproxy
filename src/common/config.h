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

  std::string debug_str(bool formatted = false) const;

public:
  OMS_CONFIG_UINT16(service_port, 2983);
  OMS_CONFIG_UINT32(encode_threadpool_size, 8);
  OMS_CONFIG_UINT32(encode_queue_size, 20000);
  OMS_CONFIG_UINT32(max_packet_bytes, 1024 * 1024 * 64);  // 64MB
  OMS_CONFIG_UINT32(command_timeout_s, 10);
  OMS_CONFIG_UINT64(accept_interval_us, 500000);

  OMS_CONFIG_UINT32(record_queue_size, 20000);
  OMS_CONFIG_UINT64(read_timeout_us, 2000000);
  OMS_CONFIG_UINT64(read_fail_interval_us, 1000000);
  OMS_CONFIG_UINT32(read_wait_num, 20000);

  OMS_CONFIG_UINT64(send_timeout_us, 2000000);
  OMS_CONFIG_UINT64(send_fail_interval_us, 1000000);

  OMS_CONFIG_BOOL(check_quota_enable, false);

  OMS_CONFIG_BOOL(log_to_stdout, false);
  OMS_CONFIG_UINT32(log_quota_size_mb, 5120);
  OMS_CONFIG_UINT32(log_quota_day, 30);
  OMS_CONFIG_UINT32(log_gc_interval_s, 43200);

  OMS_CONFIG_STR(oblogreader_path, "./run");
  OMS_CONFIG_UINT32(oblogreader_path_retain_hour, 168);  // 7 Days
  OMS_CONFIG_UINT32(oblogreader_lease_s, 300);           // 5 mins
  OMS_CONFIG_UINT32(oblogreader_max_count, 100);

  OMS_CONFIG_UINT32(max_cpu_ratio, 0);
  OMS_CONFIG_UINT64(max_mem_quota_mb, 0);

  OMS_CONFIG_BOOL(allow_all_tenant, false);
  OMS_CONFIG_BOOL(auth_user, true);
  OMS_CONFIG_BOOL(auth_use_rs, false);
  OMS_CONFIG_BOOL(auth_allow_sys_user, false);

  OMS_CONFIG_ENCRYPT(ob_sys_username, "");
  OMS_CONFIG_ENCRYPT(ob_sys_password, "");

  OMS_CONFIG_UINT64(ob_clog_fetch_interval_s, 600);  // 10 mins
  OMS_CONFIG_UINT64(ob_clog_expr_s, 43200);          // 12 h

  OMS_CONFIG_UINT32(counter_interval_s, 2);  // 2s
  OMS_CONFIG_BOOL(metric_enable, true);
  OMS_CONFIG_UINT32(metric_interval_s, 120);  // 2mins

  // when builtin_cluster_url_prefix not empty, we read cluster_id in handshake to make an complete cluster_url
  OMS_CONFIG_STR(builtin_cluster_url_prefix, "");

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

  OMS_CONFIG_UINT32(node_mem_limit_minimum_mb, 2048);
  OMS_CONFIG_UINT32(node_mem_limit_threshold_percent, 85);
  OMS_CONFIG_UINT32(node_cpu_limit_threshold_percent, 90);
  OMS_CONFIG_UINT32(node_disk_limit_threshold_percent, 85);
};

}  // namespace logproxy
}  // namespace oceanbase
