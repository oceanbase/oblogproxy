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

#include "LogMessageFactory.h"
#include "LogMessageBuf.h"

#include "codec/encoder.h"
#include "codec/decoder.h"
#include "codec/message.h"
#include "codec/message_buffer.h"

using namespace oceanbase::logproxy;

TEST(Codec, decode_hand_shake)
{
  ClientHandshakeRequestMessage handshake_message(0,
      "127.0.0.1",
      "1",
      "1.0",
      false,
      "tb_white_list=*.*.* "
      "cluster_url=http://127.0.0.1:8080/"
      "services?Action=ObRootServiceInfo&User_ID=alibaba&UID=admin&ObRegion=metacluster cluster_user=xxx "
      "cluster_password=yyy");

  ProtobufEncoder encoder;
  MessageBuffer message_buffer;
  int ret = encoder.encode(handshake_message, message_buffer);
  ASSERT_EQ(ret, 0);

  Message* decoded_msg = nullptr;
  Decoder& decoder = Decoder::get_default();
  ret = decoder.decode_payload(MessageVersion::V2, message_buffer, &decoded_msg);
  ASSERT_EQ(ret, 0);

  ASSERT_EQ(decoded_msg->get_message_type(), MessageType::HANDSHAKE_REQUEST_CLIENT);

  ClientHandshakeRequestMessage* decoded_handshake_msg = (ClientHandshakeRequestMessage*)decoded_msg;
  ASSERT_EQ(decoded_handshake_msg->log_type(), 0);
  ASSERT_EQ(decoded_handshake_msg->ip(), std::string("127.0.0.1"));
}

TEST(Codec, decode_client_data)
{
  LMLocalinit();

  ILogRecord* log_record = LogMessageFactory::createLogRecord(_s_logmsg_type, true);
  ASSERT_NE(log_record, nullptr);

  log_record->setSrcType(100);
  log_record->setSrcCategory(200);
  log_record->setThreadId(0x700);
  log_record->setTimestamp(123456);
  log_record->setDbname("dbname");
  log_record->setTbname("table_name");

  std::string old = "old";
  std::string new_ = "new";
  log_record->putOld(&old);
  log_record->putNew(&new_);

  std::vector<ILogRecord*> log_records;
  log_records.push_back(log_record);

  RecordDataMessage record_data_message(std::move(log_records), 0);
  ProtobufEncoder encoder;
  MessageBuffer message_buffer;
  int ret = encoder.encode(record_data_message, message_buffer);
  ASSERT_EQ(ret, OMS_OK);

  Message* decoded_msg = nullptr;
  Decoder& decoder = Decoder::get_default();
  ret = decoder.decode_payload(MessageVersion::V2, message_buffer, &decoded_msg);
  ASSERT_EQ(ret, OMS_OK);
  ASSERT_NE(decoded_msg, nullptr);
  ASSERT_EQ(decoded_msg->get_message_type(), MessageType::DATA_CLIENT);

  RecordDataMessage* decoded_record_data_message = (RecordDataMessage*)decoded_msg;
  auto& decoded_log_records = decoded_record_data_message->log_records();
  ASSERT_EQ((int)decoded_log_records.size(), 1);
  ILogRecord* decoded_log_record = decoded_log_records[0];
  ASSERT_NE(decoded_log_record, nullptr);
  ASSERT_EQ(decoded_log_record->getSrcType(), 100);
  ASSERT_EQ(decoded_log_record->getSrcCategory(), 200);
  ASSERT_EQ((int)decoded_log_record->getThreadId(), 0x700);
  ASSERT_EQ(decoded_log_record->getTimestamp(), 123456);
  ASSERT_STREQ(decoded_log_record->dbname(), "dbname");
  ASSERT_STREQ(decoded_log_record->tbname(), "table_name");

  LMLocaldestroy();
}
