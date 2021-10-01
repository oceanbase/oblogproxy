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

namespace oceanbase {
namespace logproxy {

void init_log(const char* argv0, bool restart)
{
#ifdef WITH_GLOG

  if (Config::instance().log_to_stdout.val()) {
    return;
  }

  if (restart) {
    google::ShutdownGoogleLogging();
  }

  std::string bin_name = argv0;
  auto pos = bin_name.find_last_of('/');
  if (pos != std::string::npos) {
    bin_name = bin_name.substr(pos + 1);
  }

  FLAGS_minloglevel = 0;
  FLAGS_logbufsecs = 0;
  FLAGS_max_log_size = 1024;
  FLAGS_stop_logging_if_full_disk = true;
  FLAGS_logtostderr = false;
  FLAGS_alsologtostderr = false;
  FLAGS_log_dir = "";

  google::SetLogSymlink(google::GLOG_INFO, bin_name.c_str());
  google::SetLogSymlink(google::GLOG_WARNING, bin_name.c_str());
  google::SetLogSymlink(google::GLOG_ERROR, bin_name.c_str());
  google::SetLogSymlink(google::GLOG_FATAL, bin_name.c_str());
  google::SetLogDestination(google::GLOG_INFO, "log/logproxy_info.");
  google::SetLogDestination(google::GLOG_WARNING, "log/logproxy_warn.");
  google::SetLogDestination(google::GLOG_ERROR, "log/logproxy_error.");
  google::SetLogDestination(google::GLOG_FATAL, "log/logproxy_error.");

  FileGcRoutine log_gc("./log", {"logproxy_", bin_name + ".log"});
  //    log_gc.start();
  //    log_gc.detach();

  google::InitGoogleLogging(bin_name.c_str());

#endif
}

}  // namespace logproxy
}  // namespace oceanbase
