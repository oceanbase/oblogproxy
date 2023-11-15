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

#include <unistd.h>
#ifdef linux
#include <sys/prctl.h>
#endif
#include <filesystem>
#include "log.h"
#include "config.h"
#include "fs_util.h"
#include "obaccess/ob_access.h"
#include "source_invoke.h"

namespace oceanbase {
namespace logproxy {

int SourceInvoke::start_oblogreader(Comm& comm, const ClientMeta& client, OblogConfig& config)
{
  std::string oblogreader_work_path = Config::instance().oblogreader_path.val() + std::string("/") + client.id;
  FsUtil::mkdir(oblogreader_work_path);
  std::string config_name = "oblogreader.conf";
  std::string config_file = oblogreader_work_path + std::string("/") + config_name;
  if (OMS_OK != serialize_configs(client, config, config_file)) {
    OMS_ERROR("Failed to serialize configs of oblogreader process to file: {}.", config_file);
    return OMS_FAILED;
  }

  int pid = fork();
  if (pid == -1) {
    OMS_ERROR("Failed to fork: {}({})", errno, strerror(errno));
    return OMS_FAILED;
  }

  if (pid == 0) {  // children
    // close fds
    comm.stop(client.peer.fd);

    // exec oblogreader
    std::string oblogreader_bin_file = Config::instance().bin_path.val() + std::string("/") + "oblogreader";
    char* argv[] = {const_cast<char*>("./oblogreader"),
        const_cast<char*>(config_name.c_str()),
        const_cast<char*>(oblogreader_work_path.c_str()),
        nullptr};
    execv(oblogreader_bin_file.c_str(), argv);
    ::exit(-1);
  }

  OMS_INFO("+++ Created oblogreader with pid: {}", pid);
  return pid;
}

int SourceInvoke::serialize_configs(const ClientMeta& client, const OblogConfig& config, const std::string& config_file)
{
  // serialize json to buffer
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

  writer.StartObject();
  Config::instance().to_json(writer);
  config.to_json(writer);
  client.to_json(writer);
  writer.EndObject();

  // write file
  return FsUtil::write_file(config_file, buffer.GetString());
}

/**
 * @return pid of childern process or -1 failurs, childern process never return
 */
int SourceInvoke::invoke(Comm& comm, const ClientMeta& client, OblogConfig& config)
{
  switch (client.type) {
    case OCEANBASE:
      return start_oblogreader(comm, client, config);

    default:
      OMS_ERROR("Unsupported invoke log type: {}", client.type);
      return OMS_FAILED;
  }
}

}  // namespace logproxy
}  // namespace oceanbase
