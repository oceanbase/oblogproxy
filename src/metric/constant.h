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

#pragma once

#include <string>

namespace oceanbase {
namespace logproxy {
const std::string PATH_CPU_SET = "/sys/fs/cgroup/cpuset";
const std::string PATH_CPU = "/sys/fs/cgroup/cpu";
const std::string PATH_MEMORY = "/sys/fs/cgroup/memory";
const std::string PATH_BLKIO = "/sys/fs/cgroup/blkio";
const std::string PATH_CPU_ACCT = "/sys/fs/cgroup/cpuacct";
const std::string PATH_SYS_PROC = "/proc";
const int64_t MEM_DEFAULT = 9223372036854771712L;
const int64_t UNIT_GB = 1024 * 1024 * 1024L;
const int64_t UNIT_MB = 1024 * 1024L;
const int64_t UNIT_KB = 1024L;

const std::string NEWLINE = "\\r?\\n";
const std::string PIPE = "|";
const std::string SPACE = " ";
const std::string COMMA = ",";
const std::string HYPHEN = "-";
const std::string COLON = ":";
const std::string SEMICOLON = ";";
const std::string DIVIDE = "/";
const std::string EQUAL = "=";

const int64_t READ_BATCH_SIZE = 8 * 1024 * 1024;  // 8K

}  // namespace logproxy
}  // namespace oceanbase
