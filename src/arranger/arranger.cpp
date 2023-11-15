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

#include <sys/wait.h>
#include <arpa/inet.h>
#include <csignal>
#include <cmath>
#include "log.h"
#include "file_gc.h"
#include "source_invoke.h"
#include "arranger.h"
#include "communication/channel_factory.h"
#include "codec/message.h"
#include "obaccess/ob_access.h"
#include "metric/status_thread.h"
#include "metric/sys_metric.h"
#include "obaccess/clog_meta_routine.h"

namespace oceanbase {
namespace logproxy {
static Config& _s_conf = Config::instance();

int Arranger::init()
{
  if (!localhostip(_localhost, _localip) || _localhost.empty() || _localip.empty()) {
    OMS_STREAM_ERROR << "Failed to fetch localhost name or localip";
    return OMS_FAILED;
  }

  int ret = ChannelFactory::instance().init(_s_conf);
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to init channel factory";
    return OMS_FAILED;
  }

  ret = _accepter.init();
  if (ret == OMS_FAILED) {
    return ret;
  }
  _accepter.set_read_callback([this](const Peer& peer, const Message& msg) { return on_handshake(peer, msg); });
  _accepter.set_routine_callback([this] { gc_pid_routine(); });
  _accepter.set_close_callback([this](const Peer& peer) { on_close(peer); });
  return OMS_OK;
}

int Arranger::run_foreground()
{
  int ret = _accepter.listen(_s_conf.service_port.val());
  if (ret != OMS_OK) {
    return ret;
  }
  StatusThread status_thread(logproxy::g_metric, logproxy::g_proc_metric);
  status_thread.register_gauge("NREADER", [this] { return _client_peers.size(); });
  status_thread.register_gauge("NCHANNEL", [this] { return _accepter.channel_count(); });
  if (_s_conf.metric_enable.val()) {
    status_thread.start();
    status_thread.detach();
  }

  std::set<std::string> prefixs{
      "logproxy.2", "liboblog.log.2", "binlog_converter.2", "trace_record_read.2", "logproxy_", "binlog.converter"};
  FileGcRoutine log_gc("log", prefixs);
  log_gc.start();
  log_gc.detach();

  return _accepter.start();
}

EventResult Arranger::on_handshake(const Peer& peer, const Message& msg)
{
  OMS_STREAM_INFO << "Arranger on_msg fired: " << peer.to_string();
  if (msg.type() != MessageType::HANDSHAKE_REQUEST_CLIENT) {
    OMS_STREAM_WARN << "Unknown message type: " << (int)msg.type();
    return EventResult::ER_CLOSE_CHANNEL;
  }

  auto& handshake = (ClientHandshakeRequestMessage&)msg;
  OMS_STREAM_INFO << "Handshake request from peer: " << peer.to_string() << ", msg: " << handshake.to_string();

  ClientMeta client = ClientMeta::from_handshake(peer, handshake);
  client.packet_version = msg.version();

  std::string errmsg;
  OblogConfig oblog_config(client.configuration);
  if (resolve(oblog_config, errmsg) != OMS_OK) {
    response_error(peer, msg.version(), ErrorCode::NO_AUTH, errmsg);
    return EventResult::ER_CLOSE_CHANNEL;
  }

  OMS_STREAM_INFO << "ObConfig from peer: " << peer.to_string() << " after resolve: " << oblog_config.debug_str();

  if (auth(oblog_config, errmsg) != OMS_OK) {
    response_error(peer, msg.version(), ErrorCode::NO_AUTH, errmsg);
    return EventResult::ER_CLOSE_CHANNEL;
  }

  // resolve sys user
  if (!oblog_config.sys_user.empty()) {
    oblog_config.user.set(oblog_config.sys_user.val());
  } else {
    oblog_config.user.set(Config::instance().ob_sys_username.val());
  }

  if (check_quota() != OMS_OK) {
    // close connect directly to make client reconnect
    return EventResult::ER_CLOSE_CHANNEL;
  }

  if (check_clog(oblog_config, errmsg) != OMS_OK) {
    response_error(peer, msg.version(), ErrorCode::NO_AUTH, errmsg);
    return EventResult::ER_CLOSE_CHANNEL;
  }

  ClientHandshakeResponseMessage resp(0, _localip, __OMS_VERSION__);
  resp.set_version(msg.version());
  int ret = _accepter.send_message(peer, resp, true);
  if (ret != OMS_OK) {
    OMS_STREAM_WARN << "Failed to send handshake response message. peer: " << peer.to_string();
    return EventResult::ER_CLOSE_CHANNEL;
  }

  client.peer = peer;
  ret = create(client, oblog_config);
  if (ret != OMS_OK) {
    response_error(peer, msg.version(), E_INNER, "Failed to create oblogreader");
    return EventResult::ER_CLOSE_CHANNEL;
  }
  return EventResult::ER_SUCCESS;
}

int Arranger::resolve(OblogConfig& hs_config, std::string& errmsg)
{
  config_password(hs_config, false);

  if (!hs_config.root_servers.empty()) {
    // do nothing
  } else if (!Config::instance().builtin_cluster_url_prefix.val().empty()) {
    // resolve builtin cluster url
    if (hs_config.cluster_url.empty()) {
      if (hs_config.cluster_id.empty()) {
        errmsg = "Refuse connection caused by no cluster id or cluster url";
        OMS_STREAM_ERROR << errmsg;
        return OMS_FAILED;
      }
      hs_config.cluster_url.set(Config::instance().builtin_cluster_url_prefix.val() + hs_config.cluster_id.val());
      hs_config.cluster_id.set("");
    }
  } else if (hs_config.cluster_url.empty()) {
    errmsg = "Refuse connection caused by no cluster url";
    OMS_STREAM_ERROR << errmsg;
    return OMS_FAILED;
  }

  return OMS_OK;
}

int Arranger::auth(const OblogConfig& oblog_config, std::string& errmsg)
{
  if (_s_conf.auth_user.val()) {
    ObAccess ob_access;
    int ret = ob_access.init(oblog_config, oblog_config.password_sha1, oblog_config.sys_password_sha1);
    if (ret != OMS_OK) {
      errmsg = "Failed to parse configuration";
      return ret;
    }

    ret = ob_access.auth();
    if (ret != OMS_OK) {
      errmsg = "Failed to auth";
      return ret;
    }
  }
  return OMS_OK;
}

int Arranger::check_quota()
{
  if (!_s_conf.check_quota_enable.val()) {
    return OMS_OK;
  }

  if (_client_peers.size() >= _s_conf.oblogreader_max_count.val()) {
    OMS_STREAM_ERROR << "Exceed max oblogreader count, current: " << _client_peers.size();
    return OMS_FAILED;
  }

  int max_cpu_ratio = _s_conf.max_cpu_ratio.val();
  int current_cpu_ratio = ::floor(g_metric.cpu_status.cpu_used_ratio);
  if (max_cpu_ratio != 0 && current_cpu_ratio >= max_cpu_ratio) {
    OMS_STREAM_ERROR << "Exceed max cpu ratio: " << max_cpu_ratio << ", current: " << current_cpu_ratio;
    return OMS_FAILED;
  }

  uint64_t max_mem_quota_mb = _s_conf.max_mem_quota_mb.val();
  uint64_t current_mem_mb = g_metric.memory_status.mem_used_size_mb;
  if (max_mem_quota_mb != 0 && current_mem_mb >= max_mem_quota_mb) {
    OMS_STREAM_ERROR << "Exceed max mem quota in MB: " << max_mem_quota_mb << ", current: " << current_mem_mb;
    return OMS_FAILED;
  }

  return OMS_OK;
}

int Arranger::create(ClientMeta& client, OblogConfig& oblog_config)
{
  OMS_STREAM_INFO << "Client connecting: " << client.to_string();

  const std::string& client_id = client.id;
  const auto& fd_entry = _client_peers.find(client_id);
  if (fd_entry != _client_peers.end()) {
    OMS_STREAM_WARN << "Duplication exist clientId: " << client_id << ", close last one: " << fd_entry->second.peer.id()
                    << " with pid: " << fd_entry->second.pid;
    return OMS_FAILED;
    //    close_client_force(fd_entry->second, "Duplication exist client_id");
  }

  int ret = SourceInvoke::invoke(_accepter, client, oblog_config);
  if (ret <= 0) {
    OMS_STREAM_ERROR << "Failed to start source of client:" << client.to_string();
    return OMS_FAILED;
  }

  _accepter.del(client.peer);  // remove fd event for parent;
  OMS_STREAM_INFO << "Remove peer: " << client.peer.to_string()
                  << " after source invoked, current channel count:" << _accepter.channel_count();

  client.pid = ret;
  _client_peers.emplace(client_id, client);
  OMS_STREAM_INFO << "Client connected: " << client_id << " with peer: " << client.peer.to_string();
  return OMS_OK;
}

void Arranger::response_error(const Peer& peer, MessageVersion version, ErrorCode code, const std::string& errmsg)
{
  ErrorMessage error(code, errmsg);
  error.set_version(version);
  int ret = _accepter.send_message(peer, error, true);
  if (ret != OMS_OK) {
    OMS_STREAM_WARN << "Failed to send error response message to peer:" << peer.to_string()
                    << " for message:" << errmsg;
  }
}

void Arranger::on_close(const Peer& peer)
{
  for (auto iter = _client_peers.begin(); iter != _client_peers.end(); ++iter) {
    if (iter->second.peer.id() == peer.id()) {
      OMS_STREAM_WARN << "On close peer fd: " << peer.fd << " with client: " << iter->second.id;

      int fd = iter->second.peer.fd;
      OMS_STREAM_WARN << "Try to shutdown fd: " << fd;
      shutdown(fd, SHUT_RDWR);
      OMS_STREAM_WARN << "Shutdown fd: " << fd;

      _client_peers.erase(iter);
      break;
    }
  }
}

int Arranger::close_client_force(const ClientMeta& client, const std::string& msg)
{
  ErrorMessage err(OMS_FAILED, msg);
  err.set_version(client.packet_version);
  _accepter.trigger_del(client.peer, err);

  int fd = client.peer.fd;
  OMS_STREAM_WARN << "Try to shutdown fd: " << fd;
  shutdown(fd, SHUT_RDWR);
  OMS_STREAM_WARN << "Shutdown fd: " << fd;

  auto entry = _client_peers.find(client.id);
  if (entry != _client_peers.end()) {
    int pid = entry->second.pid;

    if (pid != 0) {
      // make sure last oblogreader was exit
      // trigger child_waiter later
      // then GC by gc_pid_routine and call close_by_pid
      // as we called close() in trigger_del already, time window wthin that
      // there would be new client conneted again
      // causing current client with same fd closed by later GC,
      // we avoid this phantom scenario by check peer id
      ::kill(pid, SIGKILL);
      close_by_pid(pid, client);
    }
    _client_peers.erase(entry);
  }
  return OMS_OK;
}

void Arranger::gc_pid_routine()
{
  for (auto iter = _client_peers.begin(); iter != _client_peers.end();) {
    int pid = iter->second.pid;
    // detect if oblogreader still alive
    if (kill(pid, 0) != 0) {
      close_by_pid(pid, iter->second);
      iter = _client_peers.erase(iter);
    } else {
      ++iter;
    }
  }
}

void Arranger::close_by_pid(int pid, const ClientMeta& client)
{
  OMS_STREAM_WARN << "Exited oblogreader of pid: " << pid << " with clientId: " << client.id
                  << " of peer:" << client.peer.to_string();

  int fd = client.peer.fd;
  struct sockaddr_in peer_addr;
  socklen_t len;
  int ret = getpeername(fd, (struct sockaddr*)&peer_addr, &len);
  if (ret == 0 && peer_addr.sin_addr.s_addr != 0) {
    Peer p(peer_addr.sin_addr.s_addr, ntohs(peer_addr.sin_port), fd);
    OMS_STREAM_INFO << "fetched peer: " << p.to_string();
    if (p.id() != client.peer.id()) {
      // fd number was assigned to another(new connected) client session
      return;
    }
  } else {
    OMS_STREAM_WARN << "Failed to fetch peer info of fd:" << fd << ", errno:" << errno << ", error:" << strerror(errno);
  }

  // As fd was sent to child process and close immediately,
  // we do del again here for checking if fd was leaked caused by potential bugs
  _accepter.del(client.peer);

  OMS_STREAM_WARN << "Try to shutdown fd: " << fd;
  shutdown(fd, SHUT_RDWR);
  OMS_STREAM_WARN << "Shutdown fd: " << fd;
}

int Arranger::check_clog(const OblogConfig& oblog_config, std::string& errmsg)
{

  if (!_s_conf.check_clog_enable.val()) {
    return OMS_OK;
  }

  ClogMetaRoutine clog_meta;
  if (clog_meta.init(oblog_config) != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to initialize the Clog site checker";
    return OMS_FAILED;
  }

  if (!clog_meta.check(oblog_config, errmsg)) {
    OMS_STREAM_ERROR << errmsg;
    return OMS_FAILED;
  }

  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
