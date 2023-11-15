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

#pragma once

#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"

namespace oceanbase {
namespace binlog {
struct evconnlistener* evconnlistener_new(struct event_base* ev_base, uint16_t port, evconnlistener_cb cb,
    evconnlistener_errorcb error_cb = nullptr, int backlog = -1);

}  // namespace binlog
}  // namespace oceanbase
