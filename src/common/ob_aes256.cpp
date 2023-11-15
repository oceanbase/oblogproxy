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

#include "ob_aes256.h"
#include "common.h"
#include "log.h"

#include <cstring>

namespace oceanbase {
namespace logproxy {
static const char* default_encrypt_key = "LogProxy*";

static const unsigned char iv[] = "0123456789012345";

AES::AES()
{
  _cipher_ctx = EVP_CIPHER_CTX_new();
}

AES::~AES()
{
  EVP_CIPHER_CTX_free(_cipher_ctx);
}

int AES::reset()
{
  EVP_CIPHER_CTX_cleanup(_cipher_ctx);
  EVP_CIPHER_CTX_init(_cipher_ctx);
  return OMS_OK;
}

int AES::encrypt(const char* plain_text, int plain_text_len, char** encrypted, int& encrypted_len)
{
  return encrypt(default_encrypt_key, plain_text, plain_text_len, encrypted, encrypted_len);
}

int AES::encrypt(const char* key, const char* plain_text, int plain_text_len, char** encrypted, int& encrypted_len)
{
  unsigned char iv_copy[sizeof(iv)];
  memcpy(iv_copy, iv, sizeof(iv));
  int ret = EVP_EncryptInit_ex(_cipher_ctx, EVP_aes_256_cbc(), nullptr, (const unsigned char*)key, iv_copy);
  if (ret != 1) {
    OMS_STREAM_ERROR << "Failed to init encrypt, return=" << ret;
    return OMS_FAILED;
  }

  const int buffer_len = plain_text_len + EVP_MAX_BLOCK_LENGTH;
  unsigned char* buffer = (unsigned char*)malloc(buffer_len);
  if (nullptr == buffer) {
    OMS_STREAM_ERROR << "Failed to alloc memory. size=" << buffer_len;
    return OMS_FAILED;
  }

  int used_buffer_len = 0;
  ret = EVP_EncryptUpdate(_cipher_ctx, buffer, &used_buffer_len, (const unsigned char*)plain_text, plain_text_len);
  if (ret != 1) {
    OMS_STREAM_ERROR << "Failed to encrypt. returned=" << ret;
    free(buffer);
    return OMS_FAILED;
  }

  int final_buffer_len = 0;
  ret = EVP_EncryptFinal_ex(_cipher_ctx, buffer + used_buffer_len, &final_buffer_len);
  if (ret != 1) {
    OMS_STREAM_ERROR << "Failed to encrypt(final). returned=" << ret;
    free(buffer);
    return OMS_FAILED;
  }

  *encrypted = (char*)buffer;
  encrypted_len = used_buffer_len + final_buffer_len;

  return OMS_OK;
}

int AES::decrypt(const char* encrypted, int encrypted_len, char** plain_text, int& plain_text_len)
{
  return decrypt(default_encrypt_key, encrypted, encrypted_len, plain_text, plain_text_len);
}

int AES::decrypt(const char* key, const char* encrypted, int encrypted_len, char** plain_text, int& plain_text_len)
{
  unsigned char iv_copy[sizeof(iv)];
  memcpy(iv_copy, iv, sizeof(iv));
  int ret = EVP_DecryptInit_ex(_cipher_ctx, EVP_aes_256_cbc(), nullptr, (const unsigned char*)key, iv_copy);
  if (ret != 1) {
    OMS_ERROR("Failed to init encrypt, return= {}", ret);
    return OMS_FAILED;
  }

  unsigned char* buffer = (unsigned char*)malloc(encrypted_len);
  if (nullptr == buffer) {
    OMS_ERROR("Failed to alloc memory. size= {}", encrypted_len);
    return OMS_FAILED;
  }
  memset(buffer, 0, encrypted_len);

  int used_buffer_len = 0;
  ret = EVP_DecryptUpdate(_cipher_ctx, buffer, &used_buffer_len, (const unsigned char*)encrypted, encrypted_len);
  if (ret != 1) {
    OMS_ERROR("Failed to decrypt. return= {}", ret);
    free(buffer);
    return OMS_FAILED;
  }

  int final_buffer_len = 0;
  ret = EVP_DecryptFinal_ex(_cipher_ctx, buffer + used_buffer_len, &final_buffer_len);
  if (ret != 1) {
    OMS_ERROR("Failed to decrypt(final), ret:{}", ret);
    free(buffer);
    return OMS_FAILED;
  }

  *plain_text = (char*)buffer;
  plain_text_len = used_buffer_len + final_buffer_len;
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
