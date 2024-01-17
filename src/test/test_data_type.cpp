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
#include "binlog/binlog_converter/binlog_convert.h"
#include "data_type.h"
// #include "binlog/data_type.h"

using namespace oceanbase::logproxy;

TEST(DataType, fill_bitmap)
{
  auto* bitmap = new unsigned char[3];
  fill_bitmap(20, 3, bitmap);
  auto b1 = bitmap[0];
  ASSERT_EQ(true, b1 == 0x00);
  auto b2 = bitmap[1];
  ASSERT_EQ(true, b2 == 0x00);
  auto b3 = bitmap[2];
  ASSERT_EQ(true, b3 == 0xf0);
}

TEST(DataType, decimal_type)
{
  IColMeta col_meta;
  std::string val = "123123123123.1122330000";
  uint8_t result[12] = {128, 0, 123, 7, 86, 181, 179, 6, 176, 138, 40, 0};
  col_meta.setScale(10);
  col_meta.setPrecision(25);
  col_meta.setType(OB_TYPE_NEWDECIMAL);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  ASSERT_EQ(true, len == 12);
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, decimal_type_null_intg)
{
  IColMeta col_meta;
  std::string val = "0.13000";
  uint8_t result[6] = {0x80, 0x00, 0x00, 0x00, 0x32, 0xc8};
  col_meta.setScale(5);
  col_meta.setPrecision(10);
  col_meta.setType(OB_TYPE_NEWDECIMAL);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, decimal_type_bug)
{
  IColMeta col_meta;
  std::string val = "0.180x0";
  //  uint8_t result[6] = {0x80, 0x00, 0x00, 0x00, 0x32, 0xc8};
  col_meta.setScale(2);
  col_meta.setPrecision(18);
  col_meta.setType(OB_TYPE_NEWDECIMAL);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
}

TEST(DataType, double_type)
{
  IColMeta col_meta;
  std::string val = "123.2";
  uint8_t result[8] = {205, 204, 204, 204, 204, 204, 94, 64};
  col_meta.setType(OB_TYPE_DOUBLE);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());

  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, float_type)
{
  IColMeta col_meta;
  std::string val = "123.1";
  uint8_t result[4] = {51, 51, 246, 66};
  col_meta.setType(OB_TYPE_FLOAT);
  MsgBuf msg_buf;
  get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, float)
{
  IColMeta col_meta;
  std::string val = "-1.175494351E-38";
  //  std::string val = "-1.17549e-38";
  uint8_t result[4] = {0x00, 0x00, 0x80, 0x80};
  col_meta.setType(OB_TYPE_FLOAT);
  col_meta.setPrecision(24);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  printf("\n");
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, float_convert)
{
  IColMeta col_meta;
  std::string val = "-1.175494351E-38";
  //  std::string val = "-1.17549e-38";
  stof(val);
}

TEST(DataType, short_type)
{
  IColMeta col_meta;
  std::string val = "-22";
  uint8_t result[2] = {234, 255};
  col_meta.setType(OB_TYPE_SHORT);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, long_type)
{
  IColMeta col_meta;
  std::string val = "-2222";
  uint8_t result[4] = {82, 247, 255, 255};
  col_meta.setType(OB_TYPE_LONG);
  MsgBuf msg_buf;
  get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, bit_type)
{
  IColMeta col_meta;
  std::string val = "6";
  col_meta.setType(OB_TYPE_BIT);
  col_meta.setPrecision(5);
  MsgBuf msg_buf;
  get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());

  uint8_t result[1] = {6};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, date_type)
{
  IColMeta col_meta;
  std::string val = "2017-12-14";
  col_meta.setType(OB_TYPE_DATE);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  uint8_t result[3] = {142, 195, 15};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, datetime_type)
{
  IColMeta col_meta;
  std::string val = "2017-12-14 09:54:00.112";
  col_meta.setType(OB_TYPE_DATETIME);
  col_meta.setScale(3);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  uint8_t result[7] = {0x99, 0x9e, 0x5c, 0x9d, 0x80, 0x04, 0x60};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);

  std::string val_negative = "-2017-12-14 09:54:00.112";
  col_meta.setType(OB_TYPE_DATETIME);
  col_meta.setScale(3);
  MsgBuf msg_buf_negative;
  len = get_column_val_bytes(col_meta, val_negative.size(), val_negative.data(), msg_buf_negative, std::string());
  printf("\n");
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf_negative.begin()->buffer()[i]);
  }
  uint8_t result_negative[7] = {0x19, 0x9e, 0x5c, 0x9d, 0x80, 0x04, 0x60};
  ASSERT_EQ(true, memcmp(result_negative, msg_buf_negative.begin()->buffer(), sizeof(result_negative)) == 0);
}

TEST(DataType, timestamp_type)
{
  IColMeta col_meta;
  //  std::string val = "2017-12-14 09:54:00.1113";
  std::string val = "1513216440.111300";
  col_meta.setType(OB_TYPE_TIMESTAMP);
  col_meta.setScale(6);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  printf("\n");
  uint8_t result[7] = {90, 49, 217, 184, 01, 0xb2, 0xc4};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, timestamp_type_precision)
{
  IColMeta col_meta;
  //  std::string val = "2017-12-14 09:54:00.1113";
  // This is because obcdc will always output a timestamp with a precision of 6 no matter what the precision is
  std::string val = "1513216440.111300";
  col_meta.setType(OB_TYPE_TIMESTAMP);
  col_meta.setScale(4);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  printf("\n");
  uint8_t result[6] = {0x5a, 0x31, 0xd9, 0xb8, 0x04, 0x59};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, time_type)
{
  {
    IColMeta col_meta;
    //  std::string val = "2017-12-14 09:54:00.1113";
    std::string val = "09:54:00.000001";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(6);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
    printf("case 0 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    uint8_t result[6] = {128, 157, 128, 0, 0, 1};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    //  std::string val = "2017-12-14 09:54:00.1113";
    std::string val = "09:54:00.00000";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(5);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
    printf("\n");
    printf("case 1 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    uint8_t result[6] = {128, 157, 128, 0, 0, 0};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "09:54:00";
    col_meta.setType(OB_TYPE_TIME);
    MsgBuf msg_buf_zero;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf_zero, std::string());
    printf("\n");
    printf("case 2 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf_zero.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result_zero[3] = {
        128,
        157,
        128,
    };
    ASSERT_EQ(true, memcmp(result_zero, msg_buf_zero.begin()->buffer(), sizeof(result_zero)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.123456";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(6);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 3 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[6] = {0x80, 0x00, 0x01, 0x01, 0xe2, 0x40};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.12346";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(5);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 4 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[6] = {0x80, 0x00, 0x01, 0x01, 0xe2, 0x44};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.1235";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(4);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 5 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[5] = {0x80, 0x00, 0x01, 0x04, 0xd3};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.123";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(3);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 6 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[5] = {0x80, 0x00, 0x01, 0x04, 0xce};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.12";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(2);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 7 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[4] = {0x80, 0x00, 0x01, 0x0c};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.1";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(1);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 8 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[4] = {0x80, 0x00, 0x01, 0x0a};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.012345";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(6);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 9 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[6] = {0x80, 0x00, 0x01, 0x00, 0x30, 0x39};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "00:00:01.012";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(3);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 10 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[5] = {0x80, 0x00, 0x01, 0x00, 0x78};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "-00:00:01.012345";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(6);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 11 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[6] = {0x7f, 0xff, 0xfe, 0xff, 0xcf, 0xc7};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "-00:00:01.012";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(3);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 12 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[5] = {0x7f, 0xff, 0xfe, 0xff, 0x88};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    std::string val_zero = "-00:00:11.000";
    col_meta.setType(OB_TYPE_TIME);
    col_meta.setScale(3);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val_zero.size(), val_zero.data(), msg_buf, std::string());
    printf("\n");
    printf("case 13 : ");
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    printf("\n");
    uint8_t result[5] = {0x7f, 0xff, 0xf5, 0x00, 0x00};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }
}

TEST(DataType, year_type)
{
  {
    IColMeta col_meta;
    //  std::string val = "2017-12-14 09:54:00.1113";
    std::string val = "2017";
    col_meta.setType(OB_TYPE_YEAR);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    uint8_t result[1] = {117};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }

  {
    IColMeta col_meta;
    //  std::string val = "2017-12-14 09:54:00.1113";
    std::string val = "0000";
    col_meta.setType(OB_TYPE_YEAR);
    MsgBuf msg_buf;
    size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
    for (int i = 0; i < len; ++i) {
      printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
    }
    uint8_t result[1] = {0};
    ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
  }
}

TEST(DataType, varchar_type)
{
  IColMeta col_meta;
  std::string val = "abcdefg";
  col_meta.setType(OB_TYPE_VARCHAR);
  col_meta.setLength(500);
  col_meta.setEncoding("utf8");
  col_meta.setName("col1");
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  printf("\n");
  uint8_t result[9] = {0x07, 0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, set_type)
{
  IColMeta col_meta;
  std::string val = "图文,推荐";
  col_meta.setType(OB_TYPE_SET);
  vector<std::string> set_def;
  //'推荐','热门', '置顶', '图文'
  set_def.push_back("推荐");
  set_def.push_back("热门");
  set_def.push_back("置顶");
  set_def.push_back("图文");
  col_meta.setValuesOfEnumSet(set_def);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  uint8_t result[1] = {0x09};
  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, bit_map)
{
  auto* after_bitmap = static_cast<unsigned char*>(malloc(2));
  int data_len[17] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 11, 12, 13, 14, 15, 16, 17};
  fill_bitmap(17, 3, after_bitmap);
  for (int i = 0; i < 17; ++i) {
    if (data_len[i] <= 0) {
      after_bitmap[i / 8] |= (0x01 << ((i % 8)));
    }
  }

  for (int j = 0; j < 2; ++j) {
    printf("\\%02hhx", after_bitmap[j]);
  }
}

TEST(DataType, uuid)
{
  GtidLogEvent gtid_log_event;
  gtid_log_event.set_gtid_uuid("3a3df14d-6bcf-11ed-b489-00163e1c5764");

  for (int j = 0; j < 16; ++j) {
    printf("\\%02hhx", gtid_log_event.get_gtid_uuid()[j]);
  }
  printf("\n");
  uint8_t result[16] = {0x3a, 0x3d, 0xf1, 0x4d, 0x6b, 0xcf, 0x11, 0xed, 0xb4, 0x89, 0x00, 0x16, 0x3e, 0x1c, 0x57, 0x64};
  ASSERT_EQ(true, memcmp(result, gtid_log_event.get_gtid_uuid(), sizeof(result)) == 0);
}

TEST(DataType, datetime_type_test)
{
  IColMeta col_meta;
  std::string val = "2020-12-02 00:00:00";
  col_meta.setType(OB_TYPE_DATETIME);
  col_meta.setScale(0);
  MsgBuf msg_buf;
  size_t len = get_column_val_bytes(col_meta, val.size(), val.data(), msg_buf, std::string());
  for (int i = 0; i < len; ++i) {
    printf("\\%02hhx", (unsigned char)msg_buf.begin()->buffer()[i]);
  }
  //  uint8_t result[7] = {0x99, 0x9e, 0x5c, 0x9d, 0x80, 0x04, 0x60};
  //  ASSERT_EQ(true, memcmp(result, msg_buf.begin()->buffer(), sizeof(result)) == 0);
}

TEST(DataType, charset_encoding_bytes)
{
  std::string val = "latin5";
  ASSERT_EQ(1, charset_encoding_bytes(val, "tab1", "col1"));

  std::string val1 = "utf8mb4";
  ASSERT_EQ(4, charset_encoding_bytes(val1, "tab2", "col2"));

  std::string val2 = "utf8mb4xxx";
  ASSERT_EQ(4, charset_encoding_bytes(val2, "tab2", "col2"));
}
