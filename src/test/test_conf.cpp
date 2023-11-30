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

#include <fstream>
#include "gtest/gtest.h"
#include "log.h"
#include "config.h"
#include "oblog_config.h"

using oceanbase::logproxy::Config;

TEST(Config, load)
{
  std::string test_conf = R"({
    "service_port": 2983,
    "encode_threadpool_size": 8,
    "encode_queue_size": 20000,
    "max_packet_bytes": 67108864,
    "record_queue_size": 512,
    "read_timeout_us": 2000000,
    "read_fail_interval_us": 1000000,
    "read_wait_num": 20000,
    "send_timeout_us": 2000000,
    "send_fail_interval_us": 1000000,
    "command_timeout_s": 10,
    "log_quota_size_mb": 5120,
    "log_quota_day": 30,
    "log_gc_interval_s": 43200,
    "oblogreader_path_retain_hour": 168,
    "oblogreader_lease_s": 300,
    "oblogreader_path": "./run",
    "allow_all_tenant": true,
    "auth_user": true,
    "auth_use_rs": false,
    "auth_allow_sys_user": true,
    "ob_sys_username": "",
    "ob_sys_password": "",
    "counter_interval_s": 2,
    "metric_interval_s": 120,
    "debug": false,
    "verbose": false,
    "verbose_packet": false,
    "readonly": false,
    "count_record": false
  })";

  std::string file = "./test_conf.json";
  std::fstream ofs(file, std::ios::out);
  ASSERT_TRUE(ofs.good());
  ofs.write(test_conf.c_str(), test_conf.size());
  ASSERT_TRUE(ofs.good());
  ofs.flush();
  ASSERT_TRUE(ofs.good());
  ofs.close();

  Config& config = Config::instance();
  int ret = config.load(file);
  OMS_STREAM_INFO << "load: " << file << ", ret: " << ret;
  ASSERT_EQ(ret, OMS_OK);
  ASSERT_EQ(config.service_port.val(), 2983);

  OblogConfig oblog_config;
  oblog_config.initial_trx_xid.set(
      "{hash:1380121015845354198, inc:16474501, addr:\"127.0.0.1:10000\", t:1694412306958599}");
  OMS_STREAM_INFO << oblog_config.initial_trx_xid.val();
  OMS_INFO("result:{}", oblog_config.initial_trx_xid.val());
}

TEST(OblogConfig, serialize_configs)
{
  OblogConfig oblog_config;
  oblog_config.initial_trx_xid.set(
      "{hash:1380121015845354198, inc:16474501, addr:\"127.0.0.1:10000\", t:1694412306958599}");
  OMS_STREAM_INFO << oblog_config.initial_trx_xid.val();
  OMS_INFO("result:{}", oblog_config.initial_trx_xid.val());

  std::string serialize = oblog_config.serialize_configs();
  OMS_INFO("result:{}", serialize);

  OblogConfig de_oblog_config;
  de_oblog_config.deserialize_configs(serialize);
  OMS_INFO("result:{}", de_oblog_config.initial_trx_xid.val());
}
