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

#include "event_wrapper.h"
#include "mysql-protocol/mysql_protocol_new.h"
#include "sys_var.h"
#include "convert_meta.h"
#include "obaccess/ob_access.h"

#include <cassert>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include <unistd.h>

namespace oceanbase {
namespace binlog {
class ConnectionManager;

class Connection {
public:
  enum class IoResult {
    SUCCESS,
    FAIL,
    BINLOG_DUMP,
    QUIT,
  };

public:
  Connection(int sock_fd, std::string local_ip, std::string peer_ip, uint16_t local_port, uint16_t peer_port,
      ConnectionManager& conn_mgr, const SysVar& sys_var);

  ~Connection();

  Connection(const Connection&) = delete;
  Connection(const Connection&&) = delete;
  Connection& operator=(const Connection&) = delete;
  Connection& operator=(const Connection&&) = delete;

  int get_sock_fd() const
  {
    return sock_fd_;
  }

  uint32_t get_thread_id() const
  {
    return thread_id_;
  }

  const std::string& get_local_ip() const
  {
    return local_ip_;
  }

  const std::string& get_peer_ip() const
  {
    return peer_ip_;
  }

  uint16_t get_local_port() const
  {
    return local_port_;
  }

  uint16_t get_peer_port() const
  {
    return peer_port_;
  }

  struct event* get_event() const
  {
    return ev_;
  }

  const std::string& get_user() const
  {
    return user_;
  }

  const std::string& get_ob_cluster() const
  {
    return ob_cluster_;
  }

  const std::string& get_ob_tenant() const
  {
    return ob_tenant_;
  }

  string get_session_var(const std::string& var_name);

  std::map<std::string, std::string>& get_session_var();

  void set_session_var(const std::string& var_name, std::string var_value);

  const std::string& get_ob_user() const
  {
    return ob_user_;
  }

  void set_ob_cluster(const string& ob_cluster);

  void set_ob_tenant(const string& ob_tenant);

  std::string endpoint() const
  {
    return "[" + get_user() + "," + get_ob_cluster() + "," + get_ob_tenant() + "]" + local_ip_ + ":" +
           std::to_string(local_port_) + "-" + peer_ip_ + ":" + std::to_string(peer_port_) + "/" +
           std::to_string(sock_fd_);
  }

  std::string trace_id() const
  {
    return "[" + _trace_id + "]" + endpoint();
  }

  void register_event(short events, event_callback_fn cb, struct event_base* ev_base = nullptr);

  void set_client_capabilities(Capability client_capabilities)
  {
    client_capabilities_ = client_capabilities;
  }

  void set_thread_id(uint32_t thread_id)
  {
    thread_id_ = thread_id;
  }

  IoResult send(const DataPacket& data_packet);

  IoResult send_handshake_packet();

  IoResult send_result_metadata(const std::vector<ColumnPacket>& column_packets);

  IoResult process_handshake_response();

  IoResult do_cmd();

  IoResult send_ok_packet();

  IoResult send_ok_packet(uint64_t affected_rows, uint64_t last_insert_id, ServerStatus status_flags, uint16_t warnings,
      const std::string& info = "");

  IoResult send_eof_packet();

  IoResult send_eof_packet(uint16_t warnings, ServerStatus status_flags);

  IoResult send_err_packet(uint16_t err_code, std::string err_msg, const std::string& sql_state);

  IoResult send_binlog_event(const uint8_t* event_buf, uint32_t len);

  std::string get_full_binlog_path() const;

  void start_row()
  {
    pkt_buf_.clear();
  }

  ssize_t store_string(const std::string& value, bool prefix_with_encoded_len = true)
  {
    return pkt_buf_.write_string(value, prefix_with_encoded_len);
  }

  ssize_t store_uint64(uint64_t value)
  {
    return store_string(std::to_string(value));
  }

  ssize_t store_null()
  {
    return pkt_buf_.write_uint1(0xFB);
  };

  IoResult send_row()
  {
    return send_data_packet();
  }

private:
  IoResult read_data_packet();

  IoResult send_data_packet();

  IoResult read_mysql_packet(uint32_t& payload_length);

  IoResult send_mysql_packet(const uint8_t* payload, uint32_t payload_length);

private:
  static constexpr uint8_t mysql_pkt_header_length = 4;
  static constexpr uint32_t mysql_pkt_max_length = (1U << 24) - 1;
  static const std::regex ob_full_user_name_pattern;

private:
  const int sock_fd_;
  const std::string local_ip_;
  const std::string peer_ip_;
  const uint16_t local_port_;
  const uint16_t peer_port_;
  ConnectionManager& conn_mgr_;
  uint32_t thread_id_;

  // set on handshake response phase
  Capability client_capabilities_;
  std::string user_;  // full username
  std::string ob_cluster_;
  std::string ob_tenant_;
  std::string ob_user_;

  struct event* ev_;

  PacketBuf pkt_buf_;
  uint8_t seq_no_;

  std::string _trace_id;

  /* session variables */
  uint32_t net_buffer_length_;
  uint32_t max_allowed_packet_;
  uint32_t net_read_timeout_;
  uint32_t net_write_timeout_;
  uint64_t net_retry_count_;
  std::map<std::string, std::string> _session_vars;
};

}  // namespace binlog
}  // namespace oceanbase
