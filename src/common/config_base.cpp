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

#include <sstream>
#include "config_base.h"
#include "log.h"
#include "ob_aes256.h"

namespace oceanbase {
namespace logproxy {

void ConfigBase::add_item(const std::string& key, ConfigItemBase* item)
{
  _configs.emplace(key, item);
}

void EncryptedConfigItem::from_str(const std::string& val)
{
  if (val.empty()) {
    return;
  }

  const char* encrypt_key = nullptr;
  if (!_encrypt_key.empty()) {
    encrypt_key = _encrypt_key.c_str();
  }

  std::string bin_val;
  hex2bin(val.data(), val.size(), bin_val);

  char* decrypted = nullptr;
  int decrypted_len = 0;

  AES aes;
  int ret = encrypt_key == nullptr
                ? aes.decrypt(bin_val.data(), bin_val.size(), &decrypted, decrypted_len)
                : aes.decrypt(encrypt_key, bin_val.data(), bin_val.size(), &decrypted, decrypted_len);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to decrypt: " << val;
    exit(-1);
  }

  _val.assign(decrypted, decrypted_len);
  free(decrypted);
}

}  // namespace logproxy
}  // namespace oceanbase
