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

#include <vector>

#include "obaccess/ob_mysql_auth.h"
#include "obaccess/ob_sha1.h"
#include "gtest/gtest.h"

using namespace oceanbase::logproxy;
using namespace std;

void auth(const char* host, int port, const char* user, const char* passwd, const char* db)
{
  SHA1 sha1;
  SHA1::ResultCode ret = sha1.input((const unsigned char*)passwd, strlen(passwd));
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);

  std::vector<char> passwd_sha1(SHA1::SHA1_HASH_SIZE);
  ret = sha1.get_result((unsigned char*)passwd_sha1.data());
  ASSERT_EQ(ret, SHA1::SHA_SUCCESS);

  ObMysqlAuth auther(host, port, user, passwd_sha1, db);
  int auth_result = auther.auth();
  ASSERT_EQ(auth_result, 0);
}
TEST(mysql_auth, auth)
{
  auth("127.0.0.1", 2881, "test", "123456", "");
}

TEST(mysql_auth, auth_oracle)
{
  auth("10.101.167.9", 2883, "wangyunlai_test@oracle_tenant#aloha", "123456", "");
}
