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

#include <mutex>
#include <memory>
#include "common/version.h"
#include "common/log.h"
#include "arranger/source_invoke.h"
#include "arranger/arranger.h"
#include "obaccess/ob_access.h"
#include "obaccess/oblog_config.h"

namespace oceanbase {
namespace logproxy {

static Config& _s_conf = Config::instance();

int Arranger::init()
{
  if (!localhostip(_localhost, _localip) || _localhost.empty() || _localip.empty()) {
    OMS_ERROR << "Failed to fetch localhost name or localip";
    return OMS_FAILED;
  }

  int ret = _accepter.init();
  if (ret == OMS_FAILED) {
    return ret;
  }
  _accepter.set_read_callback([this](const PeerInfo& peer, const Message& msg) { return on_msg(peer, msg); });
  return OMS_OK;
}

int Arranger::run_foreground()
{
  int ret = _accepter.listen(_s_conf.service_port.val());
  if (ret != OMS_OK) {
    return ret;
  }
  return _accepter.start();
}

EventResult Arranger::on_msg(const PeerInfo& peer, const Message& msg)
{
  OMS_INFO << "Arranger on_msg fired: " << peer.to_string();
  if (msg.type() == MessageType::HANDSHAKE_REQUEST_CLIENT) {
    ClientHandshakeRequestMessage& handshake = (ClientHandshakeRequestMessage&)msg;
    OMS_INFO << "Handshake request from peer: " << peer.to_string() << ", msg: " << handshake.to_string();

    ClientMeta client = ClientMeta::from_handshake(peer, handshake);
    client.packet_version = msg.version();

    std::string errmsg;
    if (auth(client, errmsg) != OMS_OK) {
      response_error(peer, msg.version(), ErrorCode::NO_AUTH, errmsg);
      return EventResult::ER_CLOSE_CHANNEL;
    }

    ClientHandshakeResponseMessage resp(0, _localip, __OMS_VERSION__);
    resp.set_version(msg.version());
    int ret = _accepter.send_message(peer, resp, true);
    if (ret != OMS_OK) {
      OMS_WARN << "Failed to send handshake response message. peer=" << peer.to_string();
      return EventResult::ER_CLOSE_CHANNEL;
    }

    ret = create(client);
    if (ret != OMS_OK) {
      response_error(peer, msg.version(), E_INNER, "Failed to create oblogreader");
      return EventResult::ER_CLOSE_CHANNEL;
    }

  } else {
    OMS_WARN << "Unknown message type: " << (int)msg.type();
  }
  return EventResult::ER_SUCCESS;
}

int Arranger::auth(ClientMeta& client, std::string& errmsg)
{
  if (_s_conf.auth_user.val()) {
    OblogConfig oblog_config(client.configuration);

    ObAccess ob_access;
    int ret = ob_access.init(oblog_config);
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

int Arranger::create(const ClientMeta& client)
{
  std::lock_guard<std::mutex> lg(_op_mutex);

  OMS_INFO << "Client connecting: " << client.to_string();

  const ClientId& client_id = client.id;
  const auto& fd_entry = _client_peers.find(client_id);
  if (fd_entry != _client_peers.end()) {
    if (fd_entry->second != client.peer) {
      OMS_ERROR << "duplication exist clientId: " << client_id.to_string();
      close_client_locked(client, "duplication exist client_id");
      return OMS_FAILED;
    } else {
      OMS_WARN << "duplication exist clientId and channel";
      return OMS_OK;
    }
  } else {
    _client_peers.emplace(client_id, client.peer);
  }

  int ret = start_source(client, client.configuration);
  if (ret != OMS_OK) {
    close_client_locked(client, "");
    return ret;
  }

  //    if (client.getEnableMonitor()) {
  //        LogProxyMetric.instance().registerRuntimeStatusCallback(client.getClientId(), status -> {
  //            status = status.toBuilder().setStreamCount(clients.size()).setWorkerCount(sources.size()).build();
  //            clientDataChannel.pushRuntimeStatus(status);
  //            return 0;
  //        });
  //        logger.info("Client registered Monitor: {}", client.getClientId());
  //    }

  OMS_INFO << "Client connected: " << client_id.to_string();
  return OMS_OK;
}

int Arranger::start_source(const ClientMeta& client, const std::string& configuration)
{
  int ret = SourceInvoke::invoke(_accepter, client, configuration);
  if (ret != OMS_OK) {
    return ret;
  }
  return OMS_OK;
}

void Arranger::response_error(const PeerInfo& peer, MessageVersion version, ErrorCode code, const std::string& errmsg)
{
  ErrorMessage error(code, errmsg);
  error.set_version(version);
  int ret = _accepter.send_message(peer, error, true);
  if (ret != OMS_OK) {
    OMS_WARN << "Failed to send error response message to peer:" << peer.to_string() << " for message:" << errmsg;
  }
}

int Arranger::close_client(const ClientMeta& client, const std::string& msg)
{
  std::lock_guard<std::mutex> _lp(_op_mutex);
  return close_client_locked(client, msg);
}

int Arranger::close_client_locked(const ClientMeta& client, const std::string& msg)
{
  auto channel_entry = _client_peers.find(client.id);
  if (channel_entry != _client_peers.end()) {
    // try to send errmsg to client, ignore send failures
    // TODO... error code
    ErrorMessage err(OMS_FAILED, msg);
    err.set_version(client.packet_version);
    int ret = _accepter.send_message(channel_entry->second, err, true);
    if (ret != OMS_OK) {
      OMS_WARN << "Failed to send error response message. client=" << client.peer.id();
    }

    _accepter.remove_channel(channel_entry->second);
    _client_peers.erase(channel_entry);
  }
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
