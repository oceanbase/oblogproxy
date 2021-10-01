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
#include "log.h"
#include "blocking_queue.hpp"

using oceanbase::logproxy::BlockingQueue;

TEST(BlockingQueue, offset_get)
{
  BlockingQueue<int> bq(1);

  uint64_t timeout_us = 1000;

  bool ret = bq.offer(1, timeout_us);
  ASSERT_EQ(ret, true);
  ret = bq.offer(2, timeout_us);
  ASSERT_EQ(ret, false);

  int element = -1;
  ret = bq.poll(element, timeout_us);
  ASSERT_EQ(ret, true);
  ASSERT_EQ(element, 1);

  ret = bq.poll(element, timeout_us);
  ASSERT_EQ(ret, false);
}
