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
#include "oblogreader/oblogreader.h"

using namespace oceanbase::logproxy;

int run(const std::string& cluster_url, const std::string& user, const std::string& password,
    const std::string& tb_white_list)
{
  std::string config_str = "tb_white_list=" + tb_white_list + " cluster_url=" + cluster_url + " cluster_user=" + user +
                           " cluster_password=" + password;

  Config::instance().readonly.set(true);

  OblogConfig oblog_config(config_str);
  oblog_config.start_timestamp = time(nullptr);
  OMS_INFO << "OB Log Config: " << oblog_config.to_string();

  ObLogReader& reader = ObLogReader::instance();
  PeerInfo peer(0);
  ChannelFactory channel_factory;
  int ret = channel_factory.init(Config::instance());
  if (ret != OMS_OK) {
    OMS_ERROR << "Failed to init channel factory";
    return ret;
  }
  Channel* ch = channel_factory.create(peer);
  ret = reader.init("test_oblogreader", MessageVersion::V2, ch, oblog_config);
  if (ret != OMS_OK) {
    return ret;
  }
  reader.start();
  reader.join();
  return OMS_OK;
}

int main(int argc, char** argv)
{
  if (argc < 5) {
    OMS_ERROR << "Invalid param. use: " << argv[0] << " cluster_url user password tb_white_list";
    exit(-1);
  }
  return run(argv[1], argv[2], argv[3], argv[4]);
}
