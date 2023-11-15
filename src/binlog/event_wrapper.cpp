/**
 * Copyright (c) 2023 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include <cstring>

#include "event_wrapper.h"
#include "log.h"
#include "common.h"

namespace oceanbase {
namespace binlog {
struct evconnlistener* evconnlistener_new(
    struct event_base* ev_base, uint16_t port, evconnlistener_cb cb, evconnlistener_errorcb error_cb, int backlog)
{
  struct evconnlistener* listener = nullptr;

  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  listener = evconnlistener_new_bind(ev_base,
      cb,
      nullptr,
      LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC,
      (backlog == 0) ? -1 : backlog,
      reinterpret_cast<const struct sockaddr*>(&sin),
      sizeof(sin));
  if (listener == nullptr) {
    OMS_STREAM_ERROR << "Failed to listen socket with port: " << port << ". error=" << strerror(errno);
    return listener;
  }

  OMS_STREAM_INFO << "Succeed to listen socket with port: " << port;
  evconnlistener_set_error_cb(listener, error_cb);
  return listener;
}

}  // namespace binlog
}  // namespace oceanbase
