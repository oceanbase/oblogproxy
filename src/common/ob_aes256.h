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

#include <openssl/aes.h>
#include <openssl/evp.h>

namespace oceanbase {
namespace logproxy {

class AES {
public:
  AES();
  ~AES();

  int reset();

  int encrypt(const char* plain_text, int plain_text_len, char** encrypted, int& encrypted_len);
  int encrypt(const char* key, const char* plain_text, int plain_text_len, char** encrypted, int& encrypted_len);
  int decrypt(const char* encrypted, int encrypted_len, char** plain_text, int& plain_text_len);
  int decrypt(const char* key, const char* encrypted, int encrypted_len, char** plain_text, int& plain_text_len);

private:
  EVP_CIPHER_CTX* _cipher_ctx;
};
}  // namespace logproxy
}  // namespace oceanbase
