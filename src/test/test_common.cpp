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
#include "common/common.h"
#include "common/log.h"
#include "common/config.h"
#include "common/jsonutil.hpp"

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
