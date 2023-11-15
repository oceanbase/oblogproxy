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

#include "connection.h"

#include <map>
#include <shared_mutex>

namespace oceanbase {
namespace binlog {
class ConnectionManager {
public:
  ConnectionManager() = default;
  ~ConnectionManager() = default;

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager(const ConnectionManager&&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&&) = delete;

  void add(Connection* conn)
  {
    std::unique_lock<std::shared_mutex> exclusive_lock{shared_mutex_};
    while (connections_.find(next_thread_id_) != connections_.end()) {
      ++next_thread_id_;
      if (next_thread_id_ == 0) {
        ++next_thread_id_;
      }
    }
    conn->set_thread_id(next_thread_id_);
    connections_[next_thread_id_] = conn;
    ++next_thread_id_;
  }

  void remove(Connection* conn)
  {
    std::unique_lock<std::shared_mutex> exclusive_lock{shared_mutex_};
    connections_.erase(conn->get_thread_id());
  }

private:
  std::shared_mutex shared_mutex_;
  uint32_t next_thread_id_ = 1;
  std::map<uint32_t, Connection*> connections_;
};

}  // namespace binlog
}  // namespace oceanbase
