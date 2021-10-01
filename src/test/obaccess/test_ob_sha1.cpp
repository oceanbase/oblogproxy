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

#include "common/common.h"
#include "obaccess/ob_sha1.h"
#include "gtest/gtest.h"

using namespace oceanbase::logproxy;

TEST(sha1, get_result)
{
  SHA1 sha1;

  unsigned char c1[] = "123456";
  SHA1::ResultCode ret = sha1.input(c1, sizeof(c1) - 1);
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);

  unsigned char result[SHA1::SHA1_HASH_SIZE];
  ret = sha1.get_result(result);
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);

  std::string hex_result;

  dumphex((const char*)result, sizeof(result), hex_result);
  ASSERT_EQ(hex_result, std::string("7C4A8D09CA3762AF61E59520943DC26494F8941B"));

  unsigned char c2[] = "alibaba-inc@antgroup.com";
  ret = sha1.reset();
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);
  ret = sha1.input(c2, sizeof(c2) - 1);
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);
  ret = sha1.get_result(result);
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);

  dumphex((const char*)result, sizeof(result), hex_result);
  ASSERT_EQ(hex_result, std::string("0DE3F2872F421723ADB4EF32E11450565C0D55C7"));
}
