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

#include "spdlog/spdlog.h"
#include "fs_util.h"
#include "log.h"
#include "config.h"
#include "oblog_config.h"
#include "client_meta.h"
#include "trace_log.h"
#include "oblogreader.h"

using namespace oceanbase::logproxy;
int init_configs(char** argv, OblogConfig& obLogConfig, ClientMeta& client);

int main(int argc, char** argv)
{
  // change work path
  ::chdir(argv[2]);
  replace_spdlog_default_logger();

  // init config
  OblogConfig config;
  ClientMeta client;
  if (OMS_OK != init_configs(argv, config, client)) {
    OMS_ERROR("!!! Exiting oblogreader process: {}, due to failed to init configs.", getpid());
    ::exit(-1);
  }
  config_password(config);

  const char* child_process_name = argv[0];
  Config& conf = Config::instance();
  conf.process_name_address.set((uint64_t)child_process_name);

  // create process used dir path
  FsUtil::mkdir("/log");

  /* !!! Before init logger, logs will be printed to the default logger (i.e., stdout) !!! */
  init_log(child_process_name);
  if (conf.verbose_record_read.val()) {
    TraceLog::init(conf.log_max_file_size_mb.val(), conf.log_retention_h.val());
  }

  OMS_INFO("!!! Started oblogreader process({}) with peer: {}, client meta: {}",
      getpid(),
      client.peer.to_string(),
      client.to_string());

  // we create new thread for fork() acting as children process's main thread
  // child never return as exit internal if error
  ObLogReader reader;
  int ret = reader.init(client.id, client.packet_version, client, config);
  if (ret == OMS_OK) {
    reader.start();
    reader.join();
  }

  // !!!IMPORTANT!!! we don't quit current thread which work as child process's main thread
  // we IGNORE other context inheriting from parent process
  OMS_WARN("!!! Exiting oblogreader process({}) with peer: {}, client meta: {}",
      getpid(),
      client.peer.to_string(),
      client.to_string());
  return 0;
}

int init_configs(char** argv, OblogConfig& obLogConfig, ClientMeta& client)
{
  std::string config_file(argv[1]);
  rapidjson::Document doc;
  if (OMS_OK != load_configs(config_file, doc)) {
    return OMS_FAILED;
  }

  if ((!doc.HasMember(CONFIG) || !doc[CONFIG].IsObject() || doc[CONFIG].IsNull()) ||
      (!doc.HasMember(OBLOG_CONFIG) || !doc[OBLOG_CONFIG].IsObject() || doc[OBLOG_CONFIG].IsNull()) ||
      (!doc.HasMember(CLIENT_META) || !doc[CLIENT_META].IsObject() || doc[CLIENT_META].IsNull())) {
    OMS_ERROR("Invalid json format: {}", config_file);
    return OMS_FAILED;
  }

  // deserialize Config
  if (OMS_OK != Config::instance().from_json(doc[CONFIG])) {
    return OMS_FAILED;
  }
  OMS_INFO("Parsed {}: {}", CONFIG, Config::instance().debug_str(true));

  // deserialize OblogConfig
  if (OMS_OK != obLogConfig.init_from_json(doc[OBLOG_CONFIG])) {
    return OMS_FAILED;
  }
  OMS_INFO("Parsed {}: {}", OBLOG_CONFIG, obLogConfig.debug_str(true));

  // deserialize ClientMeta
  if (OMS_OK != client.init_from_json(doc[CLIENT_META])) {
    return OMS_FAILED;
  }
  OMS_INFO("Parsed {}: {}", CLIENT_META, client.to_string());

  OMS_INFO("Success to init all configs of oblogreader process from json file: {}", config_file);
  return OMS_OK;
}