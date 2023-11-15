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
#include "ob_log_event.h"
#include "codec/byte_decoder.h"

using namespace oceanbase::logproxy;
TEST(BinlogDumper, fake_rotate_event)
{
  std::string file_name("mysql-bin.000001");
  RotateEvent rotate_event = RotateEvent(0, file_name, 0, 0);
  rotate_event.set_next_binlog_position(4);
  rotate_event.get_header()->set_next_position(0);
  OMS_STREAM_INFO << "send events of binlog file:" << rotate_event.get_next_binlog_file_name();
  char* buff = static_cast<char*>(malloc(rotate_event.get_header()->get_event_length() + 1));
  int1store(reinterpret_cast<unsigned char*>(buff), 0);
  size_t pos = rotate_event.flush_to_buff(reinterpret_cast<unsigned char*>(buff + 1));
  //  for (int i = 0; i < pos; ++i) {
  //    printf("\\%02hhx", buff[i]);
  //  }
  OMS_STREAM_INFO << "fake rotate event len :" << rotate_event.get_header()->get_event_length() + 1;

  for (int i = COMMON_HEADER_LENGTH + ROTATE_HEADER_LEN + 1; i < pos + 1 && buff[i] != '\0'; ++i) {
    printf("\\%02hhx", buff[i]);
  }

  free(buff);
}
