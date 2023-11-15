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
#include "binlog/binlog_converter/binlog_convert.h"
#include "data_type.h"
#include "binlog/binlog_converter/binlog_converter.h"
// #include "binlog/data_type.h"

using namespace oceanbase::logproxy;

TEST(WriteRowsEvent, deserialize)
{
  unsigned char event[] = {0x27,
      0x21,
      0x18,
      0x64,
      0x1e,
      0x44,
      0x0f,
      0x65,
      0x44,
      0x20,
      0x00,
      0x00,
      0x00,
      0x9f,
      0x01,
      0x00,
      0x00,
      0x01,
      0x00,
      0xe0,
      0x3d,
      0x09,
      0x0b,
      0xe7,
      0x63,
      0x01,
      0x00,
      0x02,
      0x00,
      0x01,
      0xff,
      0xff};
  WriteRowsEvent write_rows_event = WriteRowsEvent(0, 0);
  write_rows_event.set_checksum_flag(OFF);
  write_rows_event.deserialize(event);
  printf("%s", write_rows_event.print_event_info().c_str());
  ASSERT_EQ("table_id: 109843973750240 flags: STMT_END_F", write_rows_event.print_event_info());
}

TEST(QueryEvent, deserialize)
{}