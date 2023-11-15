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

#include "test/test_common.cpp"
#include "test/test_conf.cpp"
#include "test/test_queue.cpp"
#include "test/test_aes.cpp"
#include "test/test_message_buffer.cpp"
#include "test/test_ob_sha1.cpp"
#include "test/test_net.cpp"

#include "test/test_ob_mysql.cpp"
// #include "test/test_codec.cpp"
#include "test/test_http.cpp"
#include "test/test_compress.cpp"

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
