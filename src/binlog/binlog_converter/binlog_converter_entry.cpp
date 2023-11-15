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

#include "thread.h"
#include "fs_util.h"
#include "config.h"
#include "oblog_config.h"
#include "convert_meta.h"
#include "trace_log.h"
#include "binlog_converter.h"

using namespace oceanbase::logproxy;
int init_configs(char** argv, OblogConfig& obLogConfig, ConvertMeta& meta);

int main(int argc, char** argv)
{
  // change work path
  ::chdir(argv[2]);
  replace_spdlog_default_logger();

  // init configs
  OblogConfig config;
  ConvertMeta meta;
  if (OMS_OK != init_configs(argv, config, meta)) {
    OMS_ERROR("!!! Exiting binlog converter process: {}, due to failed to init configs.", getpid());
    ::exit(-1);
  }
  config_password(config);

  const char* child_process_name = argv[0];
  Config& conf = Config::instance();
  conf.process_name_address.set((uint64_t)child_process_name);

  // create process used dir path
  std::string process_path = meta.log_bin_prefix + std::string("/");
  FsUtil::mkdir(process_path + "log");
  FsUtil::mkdir(process_path + "data");

  /* !!! Before init logger, logs will be printed to the default logger (i.e., stdout) !!! */
  init_log(child_process_name);
  if (conf.verbose_record_read.val()) {
    TraceLog::init(conf.log_max_file_size_mb.val(), conf.log_retention_h.val());
  }

  BinlogConverter converter;
  converter.set_meta(meta);
  OMS_INFO(
      "Init fork binlog thread: {}[first_start_timestamp: {}]", config.generate_config(), meta.first_start_timestamp);
  int ret = converter.init(MessageVersion::V2, config);
  if (ret == OMS_OK) {
    converter.start();
    converter.join();
  }

  // !!!IMPORTANT!!! we don't quit current thread which work as child process's main thread
  // we IGNORE other context inheriting from parent process
  OMS_ERROR("!!! Exit binlog converter process with pid: {}", getpid());
  converter.stop();
  return 0;
}

int init_configs(char** argv, OblogConfig& obLogConfig, ConvertMeta& meta)
{
  std::string config_file(argv[1]);
  rapidjson::Document doc;
  if (OMS_OK != load_configs(config_file, doc)) {
    return OMS_FAILED;
  }

  if ((!doc.HasMember(CONFIG) || !doc[CONFIG].IsObject() || doc[CONFIG].IsNull()) ||
      (!doc.HasMember(OBLOG_CONFIG) || !doc[OBLOG_CONFIG].IsObject() || doc[OBLOG_CONFIG].IsNull()) ||
      (!doc.HasMember(CONVERT_META) || !doc[CONVERT_META].IsObject() || doc[CONVERT_META].IsNull())) {
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
  if (OMS_OK != meta.init_from_json(doc[CONVERT_META])) {
    return OMS_FAILED;
  }
  OMS_INFO("Parsed {}: {}", CONVERT_META, meta.to_string());

  OMS_INFO("Success to init all configs of binlog converter process from json file: {}", config_file);
  return OMS_OK;
}