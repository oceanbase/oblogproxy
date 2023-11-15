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

#include "fork_thread.h"
#include "binlog_converter/binlog_converter.h"
#include "binlog_state_machine.h"

namespace oceanbase {
namespace logproxy {

std::function<void(int)> cancel_handler;

void signal_handler(int signal)
{
  cancel_handler(signal);
}

int ForkBinlogThread::invoke(const ConvertMeta& meta, OblogConfig config)
{
  std::string converter_work_path = meta.log_bin_prefix;
  FsUtil::mkdir(converter_work_path);
  std::string config_name = "binlog_converter.conf";
  std::string config_file = converter_work_path + string("/") + config_name;
  if (OMS_OK != serialize_configs(meta, config, config_file)) {
    OMS_ERROR("Failed to serialize configs of binlog converter process to file: {}", config_file);
    return OMS_FAILED;
  }

  int pid = fork();
  if (pid == -1) {
    OMS_ERROR("Failed to fork: {}({})", errno, strerror(errno));
    return OMS_FAILED;
  }

  if (pid == 0) {  // children
    // close fds
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
      if (i != STDOUT_FILENO && i != STDERR_FILENO)
        close(i);
    }

    // exec binlog converter
    std::string converter_bin_file = Config::instance().bin_path.val() + std::string("/") + "binlog_converter";
    char* argv[] = {const_cast<char*>("./binlog_converter"),
        const_cast<char*>(config_name.c_str()),
        const_cast<char*>(converter_work_path.c_str()),
        nullptr};
    execv(converter_bin_file.c_str(), argv);

    ::exit(-1);
  }

  // parent
  std::string cluster = config.cluster.val();
  std::string tenant = config.tenant.val();
  OMS_INFO("+++ create binlog converter with pid: {}", pid);
  binlog::StateMachine state_machine{
      cluster, tenant, pid, converter_work_path, binlog::RUNNING, config.generate_config()};
  binlog::g_state_machine->update_state(binlog::get_default_state_file_path(), state_machine);

  return OMS_OK;
}

int ForkBinlogThread::serialize_configs(const ConvertMeta& client, const OblogConfig& config, const string& config_file)
{
  // serialize json to buffer
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

  writer.StartObject();
  Config::instance().to_json(writer);
  config.to_json(writer);
  client.to_json(writer);
  writer.EndObject();

  return FsUtil::write_file(config_file, buffer.GetString());
}

int start_binlog_converter(std::string config)
{
  logproxy::ConvertMeta meta;
  OMS_STREAM_INFO << "binlog convert config:" << config;
  logproxy::OblogConfig oblog_config;
  if (oblog_config.deserialize_configs(config) != OMS_OK) {
    oblog_config = OblogConfig(config);
  }
  meta.log_bin_prefix = oblog_config.get("log_bin_prefix");
  meta.first_start_timestamp = oblog_config.start_timestamp_us.val();
  meta.max_binlog_size_bytes = Config::instance().binlog_max_file_size_bytes.val();
  logproxy::BinlogIndexRecord index_record;
  logproxy::get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);
  if (index_record.get_checkpoint() != 0) {
    meta.first_start_timestamp = index_record.get_checkpoint();
  }
  OMS_INFO("first_start_timestamp_us:{},meta:{}", oblog_config.start_timestamp_us.val(), meta.first_start_timestamp);
  OMS_INFO("Start OceanBase binlog converter,convert config:{}", oblog_config.generate_config());
  // we create new thread for fork() acting as children process's main thread
  return ForkBinlogThread::invoke(meta, oblog_config);
}

int stop_binlog_converter(std::string status_config)
{
  binlog::StateMachine state_machine;
  state_machine.parse(status_config);
  OMS_INFO("Release BC process begin, cluster:{},tenant:{},pid:{}",
      state_machine.get_cluster(),
      state_machine.get_tenant(),
      state_machine.get_pid());
  if (state_machine.get_pid() > 0) {
    // stop BC process
    int ret = kill(state_machine.get_pid(), SIGKILL);
    if (ret != 0) {
      OMS_ERROR("Failed to stop BC process, cluster:{},tenant:{},pid:{},reason:{}",
          state_machine.get_cluster(),
          state_machine.get_tenant(),
          state_machine.get_pid(),
          logproxy::system_err(errno));
    }
    state_machine.set_converter_state(binlog::STOP);
    binlog::g_state_machine->update_state(binlog::get_default_state_file_path(), state_machine);
  }

  // Clean up related binlog files and working directories
  std::string work_path = state_machine.get_work_path();
  if (!work_path.empty()) {
    if (work_path.find(state_machine.get_tenant()) != string::npos &&
        work_path.find(state_machine.get_cluster()) != string::npos) {
      error_code err;
      uintmax_t ret = std::filesystem::remove_all(work_path, err);
      if (err) {
        OMS_ERROR("Failed to release BC and clean file,err:{}", err.message());
      }
      OMS_STREAM_INFO << "Deleted " << ret << " files or directories\n";
    } else {
      // There may be a problem with the currently cleaned log directory
      OMS_STREAM_ERROR << "There may be a problem with the currently cleaned log directory:" << work_path;
    }
  }

  OMS_INFO("Release BC process end, cluster:{},tenant:{},pid:{}",
      state_machine.get_cluster(),
      state_machine.get_tenant(),
      state_machine.get_pid());
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
