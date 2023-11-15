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

#include <bitset>
#include "gtest/gtest.h"
#include "common.h"
#include "log.h"
#include "json_parser.h"
TEST(JSON, json_parser)
{
  std::string json_str = R"([{"abs": 123}, "123@#$%^&*()_+", [0]])";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[50] = {0x02,
      0x03,
      0x00,
      0x31,
      0x00,
      0x00,
      0x0d,
      0x00,
      0x0c,
      0x1b,
      0x00,
      0x02,
      0x2a,
      0x00,
      0x01,
      0x00,
      0x0e,
      0x00,
      0x0b,
      0x00,
      0x03,
      0x00,
      0x05,
      0x7b,
      0x00,
      0x61,
      0x62,
      0x73,
      0x0e,
      0x31,
      0x32,
      0x33,
      0x40,
      0x23,
      0x24,
      0x25,
      0x5e,
      0x26,
      0x2a,
      0x28,
      0x29,
      0x5f,
      0x2b,
      0x01,
      0x00,
      0x07,
      0x00,
      0x05,
      0x00,
      0x00};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}

TEST(JSON, json_array)
{
  std::string json_str = R"([3, 4, 18, 19, 20, 21, 22, 40002, 40003, 40004])";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[47] = {0x02,
      0x0a,
      0x00,
      0x2e,
      0x00,
      0x05,
      0x03,
      0x00,
      0x05,
      0x04,
      0x00,
      0x05,
      0x12,
      0x00,
      0x05,
      0x13,
      0x00,
      0x05,
      0x14,
      0x00,
      0x05,
      0x15,
      0x00,
      0x05,
      0x16,
      0x00,
      0x07,
      0x22,
      0x00,
      0x07,
      0x26,
      0x00,
      0x07,
      0x2a,
      0x00,
      0x42,
      0x9c,
      0x00,
      0x00,
      0x43,
      0x9c,
      0x00,
      0x00,
      0x44,
      0x9c,
      0x00,
      0x00};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}

TEST(JSON, json_null)
{
  std::string json_str = R"(["abc", 10, null, true, false])";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[24] = {0x02,
      0x05,
      0x00,
      0x17,
      0x00,
      0x0c,
      0x13,
      0x00,
      0x05,
      0x0a,
      0x00,
      0x04,
      0x00,
      0x00,
      0x04,
      0x01,
      0x00,
      0x04,
      0x02,
      0x00,
      0x03,
      0x61,
      0x62,
      0x63};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}

TEST(JSON, json_empty)
{
  std::string json_str = R"({})";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[5] = {0x00, 0x00, 0x00, 0x04, 0x00};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}

TEST(JSON, json_empty_string)
{
  std::string json_str = R"({"": ""})";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[13] = {0x00, 0x01, 0x00, 0x0c, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0c, 0x0b, 0x00, 0x00};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}

TEST(JSON, json_empty_array)
{
  std::string json_str = R"({"arr": []})";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[19] = {
      0x00, 0x01, 0x00, 0x12, 0x00, 0x0b, 0x00, 0x03, 0x00, 0x02, 0x0e, 0x00, 0x61, 0x72, 0x72, 0x00, 0x00, 0x04, 0x00};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}

TEST(JSON, json_empty_object)
{
  std::string json_str = R"({"obj": {}})";
  oceanbase::logproxy::MsgBuf msg_buf;
  oceanbase::binlog::JsonParser::parser(json_str.data(), msg_buf);

  int64_t count = 1;
  for (const auto& iter : msg_buf) {
    for (int i = 0; i < iter.size(); ++i) {
      printf("%02hhx ", (unsigned char)iter.buffer()[i]);
      if (count % 8 == 0) {
        printf("\n");
      }
      count++;
    }
  }

  printf("\n%zu\n", msg_buf.byte_size());

  uint8_t result[19] = {
      0x00, 0x01, 0x00, 0x12, 0x00, 0x0b, 0x00, 0x03, 0x00, 0x00, 0x0e, 0x00, 0x6f, 0x62, 0x6a, 0x00, 0x00, 0x04, 0x00};
  auto* result_bytes = static_cast<char*>(malloc(msg_buf.byte_size()));
  msg_buf.bytes(result_bytes);
  ASSERT_EQ(true, memcmp(result, result_bytes, sizeof(result)) == 0);
  free(result_bytes);
}
