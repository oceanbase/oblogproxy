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

#include "gtest/gtest.h"
#include "common.h"
#include "log.h"
#include "common/jsonutil.hpp"
#include "binlog/common_util.h"
#include "common/shell_executor.h"
#include "metric/sys_metric.h"
#include "str.h"

using namespace oceanbase::logproxy;

TEST(COMMON, hex2bin)
{
  const char* text = "this is a text";
  int len = strlen(text);

  std::string hexstr;
  dumphex(text, len, hexstr);

  std::string binstr;
  hex2bin(hexstr.data(), hexstr.size(), binstr);
  ASSERT_STREQ(binstr.c_str(), text);
}

TEST(COMMON, json)
{
  std::string jstr;

  {
    Json::Value json;
    jstr = "{}";
    ASSERT_TRUE(str2json(jstr, json));
    ASSERT_FALSE(json.isMember("Data"));
    Json::Value node = json["Data"];
    ASSERT_TRUE(node.isNull());
  }

  {
    Json::Value json;
    jstr = "{\"Data\":{}}";
    ASSERT_TRUE(str2json(jstr, json));
    ASSERT_TRUE(json.isMember("Data"));
    Json::Value node = json["Data"];
    ASSERT_FALSE(node.isNull());
    ASSERT_FALSE(node.isArray());
    ASSERT_TRUE(node.isObject());
  }

  {
    Json::Value json;
    jstr = "{\"Data\":[]}";
    ASSERT_TRUE(str2json(jstr, json));
    ASSERT_TRUE(json.isMember("Data"));
    Json::Value node = json["Data"];
    ASSERT_FALSE(node.isNull());
    ASSERT_FALSE(node.isObject());
    ASSERT_TRUE(node.isArray());
    ASSERT_TRUE(node.empty());
  }

  {
    Json::Value json;
    jstr = R"({"Data":[{"a":"b","c":111}]})";
    ASSERT_TRUE(str2json(jstr, json));
    ASSERT_TRUE(json.isMember("Data"));
    Json::Value node = json["Data"];
    ASSERT_FALSE(node.isNull());
    ASSERT_FALSE(node.isObject());
    ASSERT_TRUE(node.isArray());
    ASSERT_FALSE(node.empty());

    ASSERT_TRUE(node[0]["a"].asString() == "b");
    ASSERT_TRUE(node[0]["c"].asInt() == 111);
  }
}

TEST(COMMON, to_json)
{
  oceanbase::logproxy::MemoryStatus memory_status;
  memory_status.mem_used_ratio = 2;
  OMS_STREAM_INFO << to_json(memory_status);

  oceanbase::logproxy::ProcessMetric process_metric;
  process_metric.memory_status.mem_used_size_mb = 2;
  //  memory.append(to_json(process_metric.memory_status).asString());
  OMS_STREAM_INFO << process_metric.serialize();
}

TEST(COMMON, release_vector)
{
  std::vector<int*> vector_ptr;

  int* val_1 = new int();
  vector_ptr.emplace_back(val_1);

  release_vector(vector_ptr);
}
TEST(COMMON, hex_to_bin)
{
  auto* ret = static_cast<unsigned char*>(malloc(6));
  oceanbase::binlog::CommonUtils::hex_to_bin("00163e1c5764", ret);

  for (int i = 0; i < 6; ++i) {
    printf("\\%02hhx", ret[i]);
  }
}
TEST(COMMON, execmd)
{
  std::string cmd = "echo 'aaa\nbbb\n'";

  std::vector<std::string> lines;
  int ret = exec_cmd(cmd, lines);
  OMS_STREAM_INFO << "cmd: " << cmd << ", returns:";
  for (auto& line : lines) {
    OMS_STREAM_INFO << line;
  }
  ASSERT_TRUE(ret == 0);
  ASSERT_TRUE(lines.size() == 2);
  ASSERT_STREQ("aaa", lines[0].c_str());
  ASSERT_STREQ("bbb", lines[1].c_str());

  ret = exec_cmd(cmd, lines, false);
  OMS_STREAM_INFO << "cmd: " << cmd << ", returns:";
  for (auto& line : lines) {
    OMS_STREAM_INFO << line;
  }
  ASSERT_TRUE(ret == 0);
  ASSERT_TRUE(lines.size() == 3);
  ASSERT_STREQ("aaa\n", lines[0].c_str());
  ASSERT_STREQ("bbb\n", lines[1].c_str());
  ASSERT_STREQ("\n", lines[2].c_str());
}

TEST(COMMON, spdlog)
{
  OMS_INFO("Start test:{}", "info");
  OMS_DEBUG("Start test:{}", "debug");
  OMS_WARN("Start test:{}", "warn");
  OMS_ERROR("Start test:{}", "error");
  OMS_FATAL("Start test:{}", "fatal");

  OMS_STREAM_INFO << "Start test:"
                  << "compatibility"
                  << "1"
                  << "2";
  OMS_STREAM_DEBUG << "Start test:"
                   << "compatibility";
  OMS_STREAM_ERROR << "Start test:"
                   << "compatibility";
  OMS_STREAM_WARN << "Start test:"
                  << "compatibility";
}

TEST(COMMON, any_string_equal)
{
  std::string str = "World";
  ASSERT_TRUE(ANY_STRING_EQUAL(str.c_str(), "Hello", "World", "Goodbye"));

  std::string str1 = "World1";
  ASSERT_FALSE(ANY_STRING_EQUAL(str1.c_str(), "Hello", "World", "Goodbye"));
}