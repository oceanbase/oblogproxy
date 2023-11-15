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

#include "gtest/gtest.h"
#include "common.h"
#include "metric/sys_metric.h"

using namespace oceanbase::logproxy;

TEST(SysMetric, get_network_stat)
{
  NetworkStatus network_status;
  oceanbase::logproxy::get_network_stat(network_status);
  printf("%lu\n", network_status.network_rx_bytes);
  printf("%lu\n", network_status.network_wx_bytes);
  ASSERT_TRUE(network_status.network_rx_bytes >= 0 && network_status.network_wx_bytes >= 0);
}