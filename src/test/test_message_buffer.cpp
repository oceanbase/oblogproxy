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

#include "codec/message_buffer.h"

using namespace oceanbase::logproxy;

void test_read_one(MessageBufferReader& reader, size_t size, const std::string& expect)
{
  char out[size];
  reader.read(out, size);
  std::string out_str(out, size);
  OMS_INFO << "out" << size << ": " << out_str << ", " << reader.debug_info();
  ASSERT_STREQ(out_str.c_str(), expect.c_str());
  reader.backward(size);
}

TEST(MessageBuffer, io)
{
  char buffer1[11] = "0123456789";
  char buffer2[11] = "abcdefghij";
  char buffer3[11] = "ABCDEFGHIJ";
  MessageBuffer buf;
  buf.push_back(buffer1, 10, false);
  buf.push_back(buffer2, 10, false);
  buf.push_back(buffer3, 10, false);

  MessageBufferReader reader(buf);
  OMS_INFO << reader.debug_info();

  test_read_one(reader, 1, "0");
  test_read_one(reader, 5, "01234");
  test_read_one(reader, 9, "012345678");
  test_read_one(reader, 10, "0123456789");
  test_read_one(reader, 11, "0123456789a");
  test_read_one(reader, 15, "0123456789abcde");
  test_read_one(reader, 19, "0123456789abcdefghi");
  test_read_one(reader, 20, "0123456789abcdefghij");
  test_read_one(reader, 21, "0123456789abcdefghijA");
  test_read_one(reader, 25, "0123456789abcdefghijABCDE");
  test_read_one(reader, 29, "0123456789abcdefghijABCDEFGHI");
  test_read_one(reader, 30, "0123456789abcdefghijABCDEFGHIJ");
}
