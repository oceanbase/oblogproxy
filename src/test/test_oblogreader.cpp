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

#include "log.h"
#include "config.h"
#include "oblogreader.h"
#include "communication/channel_factory.h"

using namespace oceanbase::logproxy;

int run(const std::string& cluster_url, const std::string& user, const std::string& password,
    const std::string& tb_white_list, const std::string& start_timestamp = "0")
{
  std::string config_str = "tb_white_list=" + tb_white_list + " cluster_url=" + cluster_url + " cluster_user=" + user +
                           " cluster_password=" + password + " first_start_timestamp=" + start_timestamp;

  Config::instance().readonly.set(true);
  Config::instance().debug.set(true);
  Config::instance().verbose.set(true);
  Config::instance().verbose_packet.set(true);

  OblogConfig oblog_config(config_str);
  OMS_STREAM_INFO << "OB Log Config: " << oblog_config.debug_str(true);

  ObLogReader reader;
  Peer peer(0);
  ChannelFactory& channel_factory = ChannelFactory::instance();
  int ret = channel_factory.init(Config::instance());
  if (ret != OMS_OK) {
    OMS_STREAM_ERROR << "Failed to init channel factory";
    return ret;
  }
  Channel& ch = channel_factory.add(peer.id(), peer);
  ret = reader.init("test_oblogreader", MessageVersion::V2, peer, oblog_config);
  if (ret != OMS_OK) {
    return ret;
  }
  reader.start();
  reader.join();
  return OMS_OK;
}

int main(int argc, char** argv)
{
  if (argc < 6) {
    OMS_STREAM_ERROR << "Invalid param. use: " << argv[0] << " cluster_url user password tb_white_list";
    exit(-1);
  }
  return run(argv[1], argv[2], argv[3], argv[4], argv[5]);
}
