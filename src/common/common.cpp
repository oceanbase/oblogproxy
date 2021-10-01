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

#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include "common.h"

namespace oceanbase {
namespace logproxy {

static char* localhost()
{
  static char* buf = nullptr;
  static size_t buf_len = 0;
  int myerror = 0;
  if (!buf) {
    do {
      errno = 0;

      if (buf) {
        buf_len += buf_len;
        char* buftmp = static_cast<char*>(realloc(buf, buf_len));
        if (buftmp) {
          buf = buftmp;
        } else {
          free(buf);
          buf = nullptr;
        }
      } else {
        buf_len = 128; /* Initial guess */
        buf = static_cast<char*>(malloc(buf_len));
      }

      if (!buf) {
        errno = ENOMEM;
        return nullptr;
      }
    } while (((myerror = gethostname(buf, buf_len)) == 0 && !memchr(buf, '\0', buf_len)) || errno == ENAMETOOLONG);

    if (myerror) { /* gethostname failed, abort.  */
      free(buf);
      buf = nullptr;
    }
  }
  return buf;
}

bool localhostip(std::string& hostname, std::string& ip)
{
  struct hostent* hp;

  char* hname = localhost();
  if (hname == nullptr) {
    return false;
  }

  hostname = hname;

  if (!(hp = gethostbyname(hname))) {
    return false;
  }

  while (hp->h_addr_list[0]) {
    ip = inet_ntoa(*(struct in_addr*)*hp->h_addr_list++);
  }

  return true;
}

void dumphex(const char data[], size_t size, std::string& hex)
{
  hex.resize(size * 2);
  for (size_t i = 0; i < size; i++) {
    char c = data[i];
    char h = (c >> 4) & 0x0F;
    char l = c & 0x0F;

    if (h >= 0x0A) {
      h = h + 'A' - 10;
    } else {
      h = h + '0';
    }
    if (l >= 0x0A) {
      l = l + 'A' - 10;
    } else {
      l = l + '0';
    }

    hex[i * 2] = h;
    hex[i * 2 + 1] = l;
  }
}

void hex2bin(const char data[], size_t size, std::string& bin)
{
  bin.reserve(size / 2);
  size_t count = 0;
  char last_char = 0;
  for (size_t i = 0; i < size; i++) {
    char c = data[i];
    if (c >= '0' && c <= '9') {
      c -= '0';
    } else if (c >= 'A' && c <= 'F') {
      c = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      c = c - 'a' + 10;
    } else {
      continue;
    }

    if (count % 2 == 0) {
      last_char = c << 4;
    } else {
      bin.append(1, (char)(last_char | c));
    }
    count++;
  }
}

}  // namespace logproxy
}  // namespace oceanbase
