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
#include "binlog/convert_meta.h"
#include "obaccess/mysql_protocol.h"
#include "common/oblog_config.h"
#include "fork_thread.h"
#include "binlog_index.h"
#include "ob_log_event.h"
#include "binlog_state_machine.h"

using namespace oceanbase::logproxy;

static std::string host = "127.0.0.1";
static uint16_t port = 2888;
static std::string cluster = "ob_xxxx";
static std::string tenant = "tenant";
static std::string user = "user";
static std::string password = "pwd";

static std::string cluster_url = "";

static std::string binlog_log_bin_basename = "/home/ds/oblogproxy/";

class BinlogTest : public testing::Test {
protected:
  BinlogTest()
  {}  // Per-test-suite set-up.
  // Called before the first test in this test suite.
  // Can be omitted if not needed.
  static void SetUpTestSuite()
  {
    // Avoid reallocating static objects if called in subclasses of FooTest.
    if (_sql_executor == nullptr) {
      _sql_executor = new MysqlProtocol();
    }
  }

  // Per-test-suite tear-down.
  // Called after the last test in this test suite.
  // Can be omitted if not needed.
  static void TearDownTestSuite()
  {
    delete _sql_executor;
    _sql_executor = nullptr;
  }

  // You can define per-test set-up logic as usual.
  void SetUp() override
  {
    GTEST_SKIP();
    oblog_config.add("cluster_url", cluster_url);
    oblog_config.add("enable_convert_timestamp_to_unix_timestamp", "1");
    oblog_config.add("enable_output_trans_order_by_sql_operation", "1");
    oblog_config.add("sort_trans_participants", "1");
    oblog_config.add("log_bin_prefix", binlog_log_bin_basename + "/" + cluster + "/" + tenant);
    oblog_config.add("memory_limit", "3G");
    oblog_config.add("cluster", cluster);
    oblog_config.add("tenant", tenant);
    oblog_config.user.set(user);
    oblog_config.password.set(password);
    //    oblog_config.add("server_uuid", server_uuid);
    oblog_config.add("worker_path", oceanbase::binlog::get_default_state_file_path());
    meta = ConvertMeta();
    meta.log_bin_prefix = binlog_log_bin_basename;
    meta.max_binlog_size_bytes = 1024;
    ForkBinlogThread::invoke(meta, oblog_config);
    std::string sha_password;
    SHA1 sha1;
    sha1.input((const unsigned char*)password.c_str(), password.size());
    sha_password.resize(SHA1::SHA1_HASH_SIZE);
    sha1.get_result((unsigned char*)sha_password.data());
    _sql_executor->login(host, port, user, sha_password);
    MySQLResultSet rs;
    int ret = _sql_executor->query("CREATE DATABASE IF NOT EXISTS binlog_inrc_test", rs);
    ASSERT_EQ(ret, OMS_OK);
    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`binlog` (\n"
                               "  `c01` int(255),  \n"
                               "  `c02` numeric,  \n"
                               "  `c03` decimal(10,5),  \n"
                               "  `c04` bit(64),  \n"
                               "  `c05` tinyint(255),  \n"
                               "  `c06` smallint,  \n"
                               "  `c07` mediumint,  \n"
                               "  `c08` bigint,  \n"
                               "  `c09` float(20),  \n"
                               "  `c10` double,  \n"
                               "  `c11` varchar(255),  \n"
                               "  `c12` char(255),  \n"
                               "  `c13` tinytext,  \n"
                               "  `c14` mediumtext,  \n"
                               "  `c15` tinyblob,  \n"
                               "  `c16` longtext,  \n"
                               "  `c17` mediumblob,  \n"
                               "  `c18` longblob,  \n"
                               "  `c19` binary(255),  \n"
                               "  `c20` varbinary(255),  \n"
                               "  `c21` timestamp,  \n"
                               "  `c22` date,  \n"
                               "  `c23` time,  \n"
                               "  `c24` datetime,\n"
                               "  `c25` year,\n"
                               "  `c26` blob,\n"
                               "  primary key(`c01`));",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`number_time_singed` (\n"
                               "`c_tint` TINYINT NOT NULL,\n"
                               "`c_bool` BOOLEAN,\n"
                               "`c_sint` SMALLINT,\n"
                               "`c_mint` MEDIUMINT,\n"
                               "`c_int` INT NOT NULL,\n"
                               "`c_bint` BIGINT,\n"
                               "unique key(c_int)\n"
                               ");",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`decimal` (\n"
                               "`c_deciaml` DECIMAL(65,30)"
                               ");",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    // init json table
    ret = _sql_executor->query(
        "CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`test_json`(id int primary key, c1 json);", rs);
    ASSERT_EQ(ret, OMS_OK);

    // init gbk string table
    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`gbk_string` (\n"
                               "`c_char_min` CHAR(0),\n"
                               "`c_varchar_min` VARCHAR(0),\n"
                               "`c_char_max` CHAR(255),\n"
                               "`c_varchar_max` VARCHAR(65535),\n"
                               "`c_text` text,\n"
                               "`c_tinytext` tinytext,\n"
                               "`c_mediumtext` mediumtext,\n"
                               "`c_longtext` longtext,\n"
                               "`c_set` SET('推荐','热门', '置顶', '图文'),\n"
                               "`c_enum` ENUM('OB','TIDB'), \n"
                               "  primary key(c_char_max)\n"
                               " ) CHARSET=gbk;",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`binary_charset` (\n"
                               "`c_binary_min` BINARY(0),\n"
                               "`c_varbinary_min` VARBINARY(0),\n"
                               "`c_binary_max` BINARY(255),\n"
                               "`c_varbinary_max` VARBINARY(65535),\n"
                               "`c_tinyblob` tinyblob,\n"
                               "`c_blob` blob,\n"
                               "`c_mediumblob` mediumblob,\n"
                               "`c_longblob` longblob,\n"
                               "`c_int_pri` int,\n"
                               "  unique key(c_int_pri)\n"
                               " )CHARSET=binary;",
        rs);
    ASSERT_EQ(ret, OMS_OK);
    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`utf8_string` (\n"
                               "`c_char_min` CHAR(0),\n"
                               "`c_varchar_min` VARCHAR(0),\n"
                               "`c_char_max` CHAR(255),\n"
                               "`c_varchar_max` VARCHAR(65535),\n"
                               "`c_text` text,\n"
                               "`c_tinytext` tinytext,\n"
                               "`c_mediumtext` mediumtext,\n"
                               "`c_longtext` longtext,\n"
                               "`c_set` SET('推荐','热门', '置顶', '图文'),\n"
                               "`c_enum` ENUM('OB','TIDB'), \n"
                               "  primary key(c_char_max)\n"
                               " ) CHARSET=utf8;",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`gb18030_string` (\n"
                               "`c_char_min` CHAR(0),\n"
                               "`c_varchar_min` VARCHAR(0),\n"
                               "`c_char_max` CHAR(255),\n"
                               "`c_varchar_max` VARCHAR(65535),\n"
                               "`c_text` text,\n"
                               "`c_tinytext` tinytext,\n"
                               "`c_mediumtext` mediumtext,\n"
                               "`c_longtext` longtext,\n"
                               "`c_set` SET('推荐','热门', '置顶', '图文'),\n"
                               "`c_enum` ENUM('OB','TIDB'), \n"
                               "  primary key(c_char_max)\n"
                               " )CHARSET=gb18030;",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`gbk_binary` (\n"
                               "`c_int_pri` int,\n"
                               "`c_binary_min` BINARY(0),\n"
                               "`c_varbinary_min` VARBINARY(0),\n"
                               "`c_binary_max` BINARY(255),\n"
                               "`c_varbinary_max` VARBINARY(65535),\n"
                               "`c_tinyblob` tinyblob,\n"
                               "`c_blob` blob,\n"
                               "`c_mediumblob` mediumblob,\n"
                               "`c_longblob` longblob,\n"
                               "  primary key(c_int_pri)\n"
                               " )CHARSET=gbk;",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`t_set_1` (\n"
                               "`c_set` SET('推荐','热门', '置顶', '图文')"
                               ");",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret = _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`string_null` ("
                               "`c_varchar_max` VARCHAR(65535)"
                               " ) CHARSET=utf8;",
        rs);
    ASSERT_EQ(ret, OMS_OK);

    ret =
        _sql_executor->query("CREATE TABLE IF NOT EXISTS `binlog_inrc_test`.`ct_checkpoint` (\n"
                             "  `id` bigint(20) NOT NULL AUTO_INCREMENT COMMENT 'PK',\n"
                             "  `gmt_created` datetime NOT NULL COMMENT '@desc 创建时间',\n"
                             "  `gmt_modified` datetime NOT NULL COMMENT '@desc 修改时间',\n"
                             "  `task_name` varchar(256) DEFAULT NULL COMMENT '@desc writer名',\n"
                             "  `cluster_name` varchar(128) DEFAULT NULL COMMENT '@desc 集群名',\n"
                             "  `timestamp` int(11) NOT NULL COMMENT '@desc 结束时间戳',\n"
                             "  `err_msg` varchar(2048) DEFAULT NULL COMMENT '@desc 错误信息',\n"
                             "  `checkpoint` longtext NOT NULL COMMENT '@desc checkpoint',\n"
                             "  `task_id` int(11) DEFAULT NULL COMMENT '@desc congo id',\n"
                             "  `gmt` int(10) DEFAULT NULL COMMENT '@desc 采样时间',\n"
                             "  PRIMARY KEY (`id`),\n"
                             "  UNIQUE KEY `uk_connector_name_uni_ctcheckpoint` (`task_name`) BLOCK_SIZE 16384 LOCAL,\n"
                             "  KEY `idx_task_id_ctcheckpoint` (`task_id`) BLOCK_SIZE 16384 LOCAL,\n"
                             "  KEY `gmt_modified` (`gmt_modified`) BLOCK_SIZE 16384 LOCAL\n"
                             ")",
            rs);
    ASSERT_EQ(ret, OMS_OK);

    //    oblog_config = OblogConfig(oblog_config_str);
  }

  // You can define per-test tear-down logic as usual.
  void TearDown() override
  {
    //    fork_binlog_thread.detach();
    //    fork_binlog_thread.stop();
  }

public:
  // Some expensive resource shared by all tests.
  static MysqlProtocol* _sql_executor;
  OblogConfig oblog_config{""};
  ConvertMeta meta;
};

MysqlProtocol* BinlogTest::_sql_executor = nullptr;

TEST_F(BinlogTest, test_all_type)
{
  MySQLResultSet rs;
  int ret;
  // 1. TRUNCATE TABLE `binlog_inrc_test`.`binlog`;
  //
  //  int ret = _sql_executor->query("TRUNCATE TABLE `binlog_inrc_test`.`binlog`", rs);
  //  ASSERT_EQ(ret, OMS_OK);
  ret = _sql_executor->query("delete from `binlog_inrc_test`.`binlog` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  ret = _sql_executor->query(
      "INSERT INTO `binlog_inrc_test`.`binlog` values "
      "(01,02,0.13,14,15,16,17,18,2.9,10,'张三','威抄','天文本','中问本','15','长文本','中不','长步','1','20','2017-08-"
      "30 11:34:21','2018-09-22','21:41:23','2020-05-14 21:41:24','1995','26')",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`binlog` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  //  assert(index_record._current_mapping.second == 2);

  sleep(60);
  std::vector<ObLogEvent*> log_events;
  OMS_STREAM_INFO << "seek events:" << index_record._file_name;
  seek_events(index_record._file_name, log_events);

  ASSERT_EQ(true, log_events.size() >= 2);
  FormatDescriptionEvent format_description_event = reinterpret_cast<const FormatDescriptionEvent&>(log_events.at(0));
  ASSERT_EQ(true, format_description_event.get_header_len() == COMMON_HEADER_LENGTH);

  PreviousGtidsLogEvent previous_gtids_log_event = reinterpret_cast<const PreviousGtidsLogEvent&>(log_events.at(1));

  ASSERT_EQ(true, previous_gtids_log_event.get_header()->get_type_code() == PREVIOUS_GTIDS_LOG_EVENT);
}

TEST_F(BinlogTest, test_number_type)
{
  MySQLResultSet rs;
  int ret;
  // 1. TRUNCATE TABLE `binlog_inrc_test`.`binlog`;
  //
  //  int ret = _sql_executor->query("TRUNCATE TABLE `binlog_inrc_test`.`binlog`", rs);
  //  ASSERT_EQ(ret, OMS_OK);
  ret = _sql_executor->query("delete from `binlog_inrc_test`.`number_time_singed` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  ret = _sql_executor->query("insert into `binlog_inrc_test`.`number_time_singed` "
                             "(c_tint,c_bool,c_sint,c_mint,c_int,c_bint) values (127,true,32767,8388607,2147483647,\n"
                             "                                                       9.223372e+18-1);",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("update  `binlog_inrc_test`.`number_time_singed` set  c_bint='66666666' where 1=1;", rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`number_time_singed` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  //  assert(index_record._current_mapping.second == 2);

  sleep(60);
  std::vector<ObLogEvent*> log_events;
  OMS_STREAM_INFO << "seek events:" << index_record._file_name;
  seek_events(index_record._file_name, log_events);

  ASSERT_EQ(true, log_events.size() >= 2);
  FormatDescriptionEvent format_description_event = reinterpret_cast<const FormatDescriptionEvent&>(log_events.at(0));
  ASSERT_EQ(true, format_description_event.get_header_len() == COMMON_HEADER_LENGTH);

  PreviousGtidsLogEvent previous_gtids_log_event = reinterpret_cast<const PreviousGtidsLogEvent&>(log_events.at(1));

  ASSERT_EQ(true, previous_gtids_log_event.get_header()->get_type_code() == PREVIOUS_GTIDS_LOG_EVENT);
}

TEST_F(BinlogTest, test_decimal_type)
{
  MySQLResultSet rs;
  int ret;

  ret = _sql_executor->query("insert into `binlog_inrc_test`.`decimal` "
                             " values (1200.987654321);",
      rs);
  ASSERT_EQ(ret, OMS_OK);
  ret = _sql_executor->query("insert into `binlog_inrc_test`.`decimal` "
                             " values (0.13);",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("update  `binlog_inrc_test`.`decimal` set  c_deciaml='-1311.987654321' where 1=1;", rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`decimal` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  //  assert(index_record._current_mapping.second == 2);

  sleep(10);
  std::vector<ObLogEvent*> log_events;
  OMS_STREAM_INFO << "seek events:" << index_record._file_name;
  seek_events(index_record._file_name, log_events);

  ASSERT_EQ(true, log_events.size() >= 2);
  FormatDescriptionEvent format_description_event = reinterpret_cast<const FormatDescriptionEvent&>(log_events.at(0));
  ASSERT_EQ(true, format_description_event.get_header_len() == COMMON_HEADER_LENGTH);

  PreviousGtidsLogEvent previous_gtids_log_event = reinterpret_cast<const PreviousGtidsLogEvent&>(log_events.at(1));

  ASSERT_EQ(true, previous_gtids_log_event.get_header()->get_type_code() == PREVIOUS_GTIDS_LOG_EVENT);
}

TEST_F(BinlogTest, utf8_string_type)
{
  MySQLResultSet rs;
  int ret;

  ret = _sql_executor->query(
      "insert into `binlog_inrc_test`.`utf8_string` (c_char_min,c_varchar_min,c_char_max,c_varchar_max,c_text,\n"
      "                        c_tinytext,c_mediumtext,c_longtext,c_set,c_enum) values(\n"
      "                          '','','测试',NULL,lpad('测试',218,'犇'),\n"
      "                          lpad('测试',85,'犇'),lpad('测试',559,'犇'),lpad('测试',1677,'犇'),\n"
      "                          '图文','OB');",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`utf8_string` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  sleep(10);
  std::vector<ObLogEvent*> log_events;
  OMS_STREAM_INFO << "seek events:" << index_record._file_name;
  seek_events(index_record._file_name, log_events);

  ASSERT_EQ(true, log_events.size() >= 2);
  FormatDescriptionEvent format_description_event = reinterpret_cast<const FormatDescriptionEvent&>(log_events.at(0));
  ASSERT_EQ(true, format_description_event.get_header_len() == COMMON_HEADER_LENGTH);

  PreviousGtidsLogEvent previous_gtids_log_event = reinterpret_cast<const PreviousGtidsLogEvent&>(log_events.at(1));

  ASSERT_EQ(true, previous_gtids_log_event.get_header()->get_type_code() == PREVIOUS_GTIDS_LOG_EVENT);
}

TEST_F(BinlogTest, gbk_string_type)
{
  MySQLResultSet rs;
  int ret;

  ret = _sql_executor->query(
      "insert into `binlog_inrc_test`.`gbk_string` (c_char_min,c_varchar_min,c_char_max,c_varchar_max,c_text,\n"
      "                        c_tinytext,c_mediumtext,c_longtext,c_set,c_enum) values(\n"
      "                          '','','测试',lpad('真的啊',653,'数据'),lpad('测试',218,'犇'),\n"
      "                          lpad('测试',85,'犇'),lpad('测试',559,'犇'),lpad('测试',1677,'犇'),\n"
      "                          '图文','OB');",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`gbk_string` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  sleep(10);
  std::vector<ObLogEvent*> log_events;
  OMS_STREAM_INFO << "seek events:" << index_record._file_name;
  seek_events(index_record._file_name, log_events);

  ASSERT_EQ(true, log_events.size() >= 2);
  FormatDescriptionEvent format_description_event = reinterpret_cast<const FormatDescriptionEvent&>(log_events.at(0));
  ASSERT_EQ(true, format_description_event.get_header_len() == COMMON_HEADER_LENGTH);

  PreviousGtidsLogEvent previous_gtids_log_event = reinterpret_cast<const PreviousGtidsLogEvent&>(log_events.at(1));

  ASSERT_EQ(true, previous_gtids_log_event.get_header()->get_type_code() == PREVIOUS_GTIDS_LOG_EVENT);
}

TEST_F(BinlogTest, set_type)
{
  MySQLResultSet rs;
  int ret;
  ret = _sql_executor->query("insert into `binlog_inrc_test`.`t_set_1` "
                             " values ('图文,推荐');",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("update  `binlog_inrc_test`.`decimal` set  c_deciaml='-1311.987654321' where 1=1;", rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`t_set_1` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  sleep(10);
}

TEST_F(BinlogTest, string_null)
{
  MySQLResultSet rs;
  int ret;
  ret = _sql_executor->query("insert into `binlog_inrc_test`.`ct_checkpoint` "
                             " values "
                             "(1,now(),now(),'test',NULL,"
                             "1668517245,NULL,'test',1,1668517245);",
      rs);
  ASSERT_EQ(ret, OMS_OK);

  ret = _sql_executor->query("delete from `binlog_inrc_test`.`ct_checkpoint` where 1=1", rs);
  ASSERT_EQ(ret, OMS_OK);
  BinlogIndexRecord index_record;
  get_index(meta.log_bin_prefix + BINLOG_DATA_DIR + BINLOG_INDEX_NAME, index_record);

  sleep(10);
}
