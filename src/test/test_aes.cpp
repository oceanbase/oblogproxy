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
#include "ob_aes256.h"
#include "common.h"
#include "log.h"

using namespace oceanbase::logproxy;

void test(const char* key, const char* plain_text, int plain_text_len)
{
  AES aes;

  char* encrypted = nullptr;
  int encrypted_len = 0;
  int ret = aes.encrypt(key, plain_text, plain_text_len, &encrypted, encrypted_len);
  ASSERT_EQ(ret, OMS_OK);

  std::string hex;
  dumphex(encrypted, plain_text_len, hex);
  OMS_STREAM_INFO << "\"" << plain_text << "\" encrypt to: " << hex;

  aes.reset();
  char* decrypted = nullptr;
  int decrypted_len = 0;
  ret = aes.decrypt(key, encrypted, encrypted_len, &decrypted, decrypted_len);
  ASSERT_EQ(ret, OMS_OK);
  ASSERT_EQ(decrypted_len, plain_text_len);
  ASSERT_EQ(0, memcmp(decrypted, plain_text, plain_text_len));

  free(encrypted);
}

TEST(AES, short_)
{
  const char* plain_text = "short text";
  test("LogProxy", plain_text, strlen(plain_text));
}
TEST(AES, long_)
{
  const char* plain_text = "this is a very very long text";
  test("LogProxy", plain_text, strlen(plain_text));

  const char* plain_text_a = "The quick brown fox jumps over the lazy dog";
  test("LogProxy", plain_text_a, strlen(plain_text_a));
}
