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

#include <stdio.h>
#include "md5.h"

namespace oceanbase {
namespace logproxy {
Md5::Md5()
{
  MD5_Init(&_ctx);
}

Md5::Md5(const char* data, size_t size)
{
  MD5_Init(&_ctx);
  update(data, size);
}

void Md5::update(const char* data, size_t size)
{
  MD5_Update(&_ctx, data, size);
}

std::string Md5::done()
{
  unsigned char buf[MD5_DIGEST_LENGTH] = "\0";
  MD5_Final(buf, &_ctx);

  size_t idx = 0;
  char md5[33] = "\0";
  for (unsigned char i : buf) {
    idx += snprintf(md5 + idx, 32, "%02x", i);
  }
  return {md5};
}

}  // namespace logproxy
}  // namespace oceanbase
