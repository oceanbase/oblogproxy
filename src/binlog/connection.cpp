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

#include "connection.h"

#include <utility>
#include "connection_manager.h"

#include "cmd_processor.h"
#include "config.h"

#include "log.h"
#include "communication/io.h"
#include "common_util.h"

namespace oceanbase {
namespace binlog {
using IoResult = Connection::IoResult;

const std::regex Connection::ob_full_user_name_pattern{R"((.*)@(.*)#(.*))"};  // user@tenant#cluster

Connection::Connection(int sock_fd, std::string local_ip, std::string peer_ip, uint16_t local_port, uint16_t peer_port,
    ConnectionManager& conn_mgr, const SysVar& sys_var)
    : sock_fd_(sock_fd),
      local_ip_(std::move(local_ip)),
      peer_ip_(std::move(peer_ip)),
      local_port_(local_port),
      peer_port_(peer_port),
      conn_mgr_(conn_mgr),
      client_capabilities_(ob_binlog_server_capabilities) /* for handshake */,
      thread_id_(0),
      ev_(nullptr),
      pkt_buf_(sys_var.net_buffer_length, sys_var.max_allowed_packet),
      seq_no_(0),
      net_buffer_length_(sys_var.net_buffer_length),
      max_allowed_packet_(sys_var.max_allowed_packet),
      net_read_timeout_(sys_var.net_read_timeout),
      net_write_timeout_(sys_var.net_write_timeout),
      net_retry_count_(sys_var.net_retry_count),
      _trace_id(CommonUtils::generate_trace_id())
{
  conn_mgr_.add(this);
}

Connection::~Connection()
{
  conn_mgr_.remove(this);
  if (ev_ != nullptr) {
    event_free(ev_);
    ev_ = nullptr;
  }
  close(sock_fd_);
  OMS_STREAM_INFO << "Closed connection " << endpoint();
}

void Connection::register_event(short events, event_callback_fn cb, struct event_base* ev_base)
{
  if (ev_ == nullptr) {
    assert(ev_base != nullptr);
    ev_ = event_new(ev_base, sock_fd_, events, cb, this);
    event_add(ev_, nullptr);
  } else {
    event_callback_fn old_cb = event_get_callback(ev_);
    short old_events = event_get_events(ev_);
    if (old_cb == cb && old_events == events) {
      if (!(old_events & EV_PERSIST)) {
        event_add(ev_, nullptr);
      }
    } else {
      ev_base = (ev_base == nullptr) ? event_get_base(ev_) : ev_base;
      event_free(ev_);
      ev_ = event_new(ev_base, sock_fd_, events, cb, this);
      event_add(ev_, nullptr);
    }
  }
}

IoResult Connection::send(const DataPacket& data_packet)
{
  pkt_buf_.clear();
  data_packet.serialize(pkt_buf_, client_capabilities_);
  return send_data_packet();
}

IoResult Connection::read_data_packet()
{
  pkt_buf_.clear();
  uint32_t payload_length;
  do {
    if (read_mysql_packet(payload_length) != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
  } while (payload_length == mysql_pkt_max_length);
  return IoResult::SUCCESS;
}

IoResult Connection::read_mysql_packet(uint32_t& payload_length)
{
  uint8_t header[mysql_pkt_header_length];
  if (logproxy::readn(sock_fd_, header, mysql_pkt_header_length) < 0) {
    OMS_ERROR("Can not read response from client. Expected to read 4 bytes, read 0 bytes before connection was "
              "unexpectedly lost.");
    return IoResult::FAIL;
  }
  uint8_t sequence = 0;
  uint32_t read_index = 0;
  read_le24toh(header, read_index, payload_length);
  read_le8toh(header, read_index, sequence);
  if (sequence != seq_no_) {
    // TODO: set malformed packet connection error
    OMS_ERROR("Unexpected seq num, expected value is {}, actual value is {}", seq_no_, sequence);
    return IoResult::FAIL;
  }

  if (!pkt_buf_.ensure_writable(payload_length)) {
    // TODO: set max allowed packet error
    return IoResult::FAIL;
  }

  if (logproxy::readn(sock_fd_, pkt_buf_.get_buf() + pkt_buf_.get_write_index(), static_cast<int>(payload_length)) <
      0) {
    return IoResult::FAIL;
  }
  pkt_buf_.set_write_index(pkt_buf_.get_write_index() + payload_length);

  ++seq_no_;
  return IoResult::SUCCESS;
}

IoResult Connection::send_data_packet()
{
  const uint8_t* payload = nullptr;
  while (pkt_buf_.readable_bytes() >= mysql_pkt_max_length) {
    pkt_buf_.read_bytes(payload, mysql_pkt_max_length);
    if (send_mysql_packet(payload, mysql_pkt_max_length) != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
  }
  uint32_t payload_length = pkt_buf_.read_remaining_bytes(payload);
  return send_mysql_packet(payload, payload_length);
}

IoResult Connection::send_mysql_packet(const uint8_t* payload, uint32_t payload_length)
{
  assert(payload_length <= mysql_pkt_max_length);
  uint8_t header[mysql_pkt_header_length];
  uint32_t write_index = 0;
  write_htole24(header, write_index, payload_length);
  write_htole8(header, write_index, seq_no_++);
  assert(write_index == mysql_pkt_header_length);
  if (logproxy::writen(sock_fd_, header, mysql_pkt_header_length) < 0 ||
      logproxy::writen(sock_fd_, payload, static_cast<int>(payload_length)) < 0) {
    return IoResult::FAIL;
  }
  return IoResult::SUCCESS;
}

IoResult Connection::send_handshake_packet()
{
  uint8_t character_set_nr = 33;  // TODO: utf8_general_ci
  HandshakePacket handshake_pkt{thread_id_, character_set_nr};
  return send(handshake_pkt);
}

// https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_response.html
IoResult Connection::process_handshake_response()
{
  if (read_data_packet() != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  uint32_t client_flags = 0;
  pkt_buf_.read_uint4(client_flags);
  auto client_capabilities = static_cast<Capability>(client_flags);
  if (!has_capability(client_capabilities, Capability::client_protocol_41)) {
    // TODO: only support Protocol::HandshakeResponse41
    return IoResult::FAIL;
  }
  // set the connection client capabilities
  client_capabilities_ = client_capabilities_ & client_capabilities;

  uint32_t max_packet_size;
  pkt_buf_.read_uint4(max_packet_size);

  uint8_t character_set_nr;
  pkt_buf_.read_uint1(character_set_nr);

  constexpr uint8_t zero_filter_length = 23;
  pkt_buf_.read_skip(zero_filter_length);

  // set the connection user
  pkt_buf_.read_null_terminated_string(user_);
  OMS_STREAM_INFO << "Received handshake response on connection " << endpoint() << ", user: " << get_user();
  std::smatch sm;
  if (std::regex_match(user_, sm, ob_full_user_name_pattern)) {
    assert(sm.size() == 4);
    ob_user_ = sm[1].str();
    ob_tenant_ = sm[2].str();
    ob_cluster_ = sm[3].str();
  } else {
    OMS_STREAM_INFO << "The user " << user_ << " on connection " << endpoint() << " is not a valid ob username";
  }

  std::string auth_response;
  if (has_capability(client_capabilities, Capability::client_plugin_auth_lenenc_client_data)) {
    pkt_buf_.read_length_encoded_string(auth_response);
  } else {
    uint8_t auth_response_length = 0;
    pkt_buf_.read_uint1(auth_response_length);
    pkt_buf_.read_fixed_length_string(auth_response, auth_response_length);
  }

  std::string database;
  if (has_capability(client_capabilities, Capability::client_connect_with_db)) {
    pkt_buf_.read_null_terminated_string(database);
  }

  std::string client_plugin_name;
  if (has_capability(client_capabilities, Capability::client_plugin_auth)) {
    pkt_buf_.read_null_terminated_string(client_plugin_name);
  }

  return send_ok_packet();
}

IoResult Connection::do_cmd()
{
  seq_no_ = 0;  // reset seq_no_ on each command
  if (read_data_packet() != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  uint8_t command = 0;
  pkt_buf_.read_uint1(command);

  auto server_command = static_cast<ServerCommand>(command);
  auto p_cmd_processor = cmd_processor(server_command);
  if (p_cmd_processor != nullptr) {
    return p_cmd_processor->process(this, pkt_buf_);
  }
  return UnsupportedCmdProcessor(server_command).process(this, pkt_buf_);
}

IoResult Connection::send_ok_packet()
{
  return send_ok_packet(0, 0, ob_binlog_server_status_flags, 0, "");
}

IoResult Connection::send_ok_packet(uint64_t affected_rows, uint64_t last_insert_id, ServerStatus status_flags,
    uint16_t warnings, const std::string& info)
{
  OkPacket ok_packet{affected_rows, last_insert_id, status_flags, warnings, info};
  return send(ok_packet);
}

IoResult Connection::send_eof_packet()
{
  return send_eof_packet(0, ob_binlog_server_status_flags);
}

IoResult Connection::send_eof_packet(uint16_t warnings, ServerStatus status_flags)
{
  EofPacket eof_packet{warnings, status_flags};
  return send(eof_packet);
}

IoResult Connection::send_err_packet(uint16_t err_code, std::string err_msg, const std::string& sql_state)
{
  ErrPacket err_packet{err_code, std::move(err_msg), sql_state};
  return send(err_packet);
}

IoResult Connection::send_binlog_event(const uint8_t* event_buf, uint32_t len)
{
  while (len >= mysql_pkt_max_length) {
    if (send_mysql_packet(event_buf, mysql_pkt_max_length) != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
    event_buf += mysql_pkt_max_length;
    len -= mysql_pkt_max_length;
  }
  return send_mysql_packet(event_buf, len);
}

std::string Connection::get_full_binlog_path() const
{
  return logproxy::Config::instance().binlog_log_bin_basename.val() + "/" + get_ob_cluster() + "/" + get_ob_tenant();
}

IoResult Connection::send_result_metadata(const std::vector<ColumnPacket>& column_packets)
{
  ColumnCountPacket column_count_packet{static_cast<uint16_t>(column_packets.size())};
  if (send(column_count_packet) != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }

  for (const auto& column_packet : column_packets) {
    if (send(column_packet) != IoResult::SUCCESS) {
      return IoResult::FAIL;
    }
  }

  return send_eof_packet();
}

void Connection::set_ob_cluster(const string& ob_cluster)
{
  ob_cluster_ = ob_cluster;
}

void Connection::set_ob_tenant(const string& ob_tenant)
{
  ob_tenant_ = ob_tenant;
}

std::string Connection::get_session_var(const std::string& var_name)
{
  if (_session_vars.find(var_name) != _session_vars.end()) {
    return _session_vars[var_name];
  }
  return "";
}

void Connection::set_session_var(const std::string& var_name, std::string var_value)
{
  _session_vars[var_name] = std::move(var_value);
}

std::map<std::string, std::string>& Connection::get_session_var()
{
  return _session_vars;
}

}  // namespace binlog
}  // namespace oceanbase
