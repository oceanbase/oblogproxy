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

#include "common/log.h"
#include "common/config.h"
#include "common/option.h"
#include "codec/encoder.h"
#include "communication/io.h"
#include "communication/communicator.h"
#include "obaccess/oblog_config.h"
#include "obaccess/mysql_protocol.h"

using namespace oceanbase::logproxy;

void debug_record(const RecordDataMessage& record)
{
  OMS_INFO << "fetch a record from liboblog: ";
  //    << "record_type: " << record.recordType()
  //    << ", timestamp: " << record->getTimestamp()
  //    << ", checkpoint: " << record->getFileNameOffset()
  //    << ", dbname: " << record->dbname()
  //    << ", tbname: " << record->tbname();
}

EventResult on_msg(const PeerInfo& peer, const Message& message)
{
  switch (message.type()) {
    case MessageType::HANDSHAKE_RESPONSE_CLIENT:
      OMS_INFO << "Connect LogProxy Succ, response: " << ((ClientHandshakeResponseMessage&)message).to_string();
      break;

    case MessageType::DATA_CLIENT: {
      const RecordDataMessage& record = (const RecordDataMessage&)message;
      OMS_INFO << "data packet, compress: " << (int)record.compress_type << ", count: " << record.count();

      debug_record(record);
      break;
    }

    case MessageType::STATUS:
      break;

    case MessageType::ERROR_RESPONSE:
      OMS_ERROR << "Error occured: " << ((ErrorMessage&)message).to_string();
      return EventResult::ER_CLOSE_CHANNEL;

    default:
      OMS_WARN << "Unknown Event income: " << (int)message.type();
      break;
  }
  return EventResult::ER_SUCCESS;
}

EventResult on_err(const PeerInfo& peer, PacketError err)
{
  OMS_ERROR << "Error occured peer: " << peer.to_string() << ", err: " << (int)err;
  return EventResult::ER_CLOSE_CHANNEL;
}

int run(const std::string& host, uint16_t port, const std::string& client_id, const std::string& config)
{
  Communicator comm;
  int ret = comm.init();
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to init Communicator";
    return ret;
  }

  int sockfd = 0;
  ret = connect(host.c_str(), port, true, 0, sockfd);
  if (ret != OMS_OK || sockfd <= 0) {
    OMS_ERROR << "Failed to connect " << host << ":" << port;
    return -1;
  }

  set_non_block(sockfd);

  OMS_INFO << "Connected to " << host << ":" << port << " with sockfd: " << sockfd;
  PeerInfo peer(sockfd);
  ret = comm.add_channel(peer);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to add channel with sockfd: " << sockfd << ", ret: " << ret;
    return -1;
  }

  ClientHandshakeRequestMessage handshake((int)LogType::OCEANBASE,
      host.c_str(),
      client_id.empty() ? "DemoClient" : client_id.c_str(),
      "1.0.0",
      false,
      config.c_str());
  comm.set_read_callback(on_msg);
  comm.set_error_callback(on_err);
  ret = comm.send_message(peer, handshake);
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to send handshake: " << handshake.to_string();
    return -1;
  }

  ret = comm.start();
  //  while (ret == OMS_AGAIN) {
  //    OMS_INFO << "No event, sleeping...";
  //    ::sleep(2);
  //    ret = comm.start();
  //  }
  return ret;
}

int main(int argc, char** argv)
{
  std::string host = "127.0.0.1";
  uint16_t port = 2983;
  std::string config;
  std::string client_id;
  std::string channel_type = "plain";
  std::string tls_ca_cert_file;
  std::string tls_key_file;
  std::string tls_cert_file;

  OmsOptions options("LogProxy Client Demo");
  options.add(OmsOption('h', "host", true, "Host", [&](const std::string& optarg) { host = optarg; }));
  options.add(OmsOption(
      'P', "port", true, "Port", [&](const std::string& optarg) { port = strtol(optarg.c_str(), nullptr, 10); }));
  options.add(OmsOption('c', "config", true, "Configuration", [&](const std::string& optarg) { config = optarg; }));
  options.add(OmsOption('i', "id", true, "Client Id", [&](const std::string& optarg) { client_id = optarg; }));
  options.add(OmsOption(
      'T', "channel-type", true, "Channel-Type(plain/tls)", [&](const std::string& optarg) { channel_type = optarg; }));
  options.add(
      OmsOption('C', "tls-ca-file", true, "ca file", [&](const std::string& optarg) { tls_ca_cert_file = optarg; }));
  options.add(
      OmsOption('E', "tls-cert-file", true, "cert file", [&](const std::string& optarg) { tls_cert_file = optarg; }));
  options.add(
      OmsOption('K', "tls-key-file", true, "key file", [&](const std::string& optarg) { tls_key_file = optarg; }));

  struct option* long_options = options.long_pattern();
  std::string&& pattern = options.pattern();
  for (int opt = 0; (opt = getopt_long(argc, argv, pattern.c_str(), long_options, nullptr)) != -1;) {
    OmsOption* option = options.get(opt);
    if (option == nullptr) {
      options.usage();
      return 0;
    } else {
      option->func(option->is_arg ? std::string(optarg) : "");
    }
  }

  if (config.empty()) {
    printf("No configuration, please use -c\n");
    return -1;
  }

  //  init_log(argv[0]);

  OblogConfig oblog_config(config);
  if (!oblog_config.password.val().empty()) {
    std::string password_sha1;
    MysqlProtocol::do_sha_password(oblog_config.password.val(), password_sha1);
    oblog_config.password.set(dumphex(password_sha1));
  }
  if (!oblog_config.sys_password.val().empty()) {
    //    std::string sys_password_sha1;
    //    MysqlProtocol::do_sha_password(oblog_config.sys_password.val(), sys_password_sha1);
    oblog_config.sys_password.set(oblog_config.sys_password.val());
  }

  Config::instance().verbose.set(true);
  Config::instance().verbose_packet.set(true);
  Config::instance().communication_mode.set("client");
  Config::instance().channel_type.set(channel_type);
  Config::instance().tls_key_file.set(tls_key_file);
  Config::instance().tls_cert_file.set(tls_cert_file);
  Config::instance().tls_ca_cert_file.set(tls_ca_cert_file);
  Config::instance().packet_magic.set(false);
  return run(host, port, client_id, oblog_config.generate_config_str());
}
