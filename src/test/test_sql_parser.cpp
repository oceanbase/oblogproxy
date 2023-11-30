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
#include "log.h"
#include "blocking_queue.hpp"
#include "sql_parser.h"
#include "sql_cmd_processor.h"
#include "sql/SelectStatement.h"
#include "sql/show_binlog_events.h"
#include "sql/set_statement.h"
#include "sql/CreateStatement.h"
#include "sql/create_binlog.h"
using namespace oceanbase::binlog;
TEST(SQLParser, parser)
{
  hsql::SQLParserResult result;
  std::string query = "show binlog status;";
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_SHOW_BINLOG_STAT, result.getStatement(0)->type());
}

TEST(SQLParser, purge)
{
  hsql::SQLParserResult result;
  std::string query = "PURGE BINARY LOGS BEFORE '2019-04-02 22:46:26' FOR TENANT `cluster`.`tenant`";
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_PURGE_BINLOG, result.getStatement(0)->type());

  query = "PURGE BINARY LOGS BEFORE '2019-04-02 22:46:26'";
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_PURGE_BINLOG, result.getStatement(0)->type());

  query = "PURGE BINARY LOGS TO 'mysql-bin.000002' FOR TENANT `ob_10088121143.admin`.`binlog`";
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_PURGE_BINLOG, result.getStatement(0)->type());
}

TEST(SQLParser, create)
{
  {
    hsql::SQLParserResult result;
    std::string query = "CREATE BINLOG FOR TENANT `cluster`.`tenant` WITH CLUSTER URL "
                        "`cluster_url`,"
                        "SERVER UUID `2340778c-7464-11ed-a721-7cd30abc99b4`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());

    query = "CREATE BINLOG FOR TENANT `cluster`.`tenant` WITH CLUSTER URL "
            "`cluster_url`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());

    query = "CREATE BINLOG IF NOT EXISTS FOR TENANT `cluster`.`tenant` FROM 1678687771255176 WITH CLUSTER URL "
            "`cluster_url`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());
  }

  {
    hsql::SQLParserResult result;
    std::string query = "CREATE BINLOG FOR TENANT `cluster`.`tenant` TO USER `user` PASSWORD `pwd` WITH CLUSTER URL "
                        "`cluster_url`,"
                        "SERVER UUID `2340778c-7464-11ed-a721-7cd30abc99b4`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());

    query = "CREATE BINLOG FOR TENANT `cluster`.`tenant` WITH CLUSTER URL "
            "`cluster_url`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());

    query = "CREATE BINLOG IF NOT EXISTS FOR TENANT `cluster`.`tenant` TO USER `user` PASSWORD `pwd` FROM "
            "1678687771255176 WITH CLUSTER URL "
            "`cluster_url`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());
  }

  {
    hsql::SQLParserResult result;
    std::string query =
        "CREATE BINLOG FOR TENANT `cluster`.`tenant` TO USER `user` PASSWORD `pwd` WITH CLUSTER URL "
        "`cluster_url`,"
        "SERVER UUID `2340778c-7464-11ed-a721-7cd30abc99b4`,INITIAL_TRX_XID `ob_txn_id`,INITIAL_TRX_GTID_SEQ `31`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());
  }

  {
    hsql::SQLParserResult result;
    std::string query =
        "CREATE BINLOG FOR TENANT `cluster`.`tenant` TO USER `user` PASSWORD `pwd` WITH CLUSTER URL "
        "'cluster_url',"
        "SERVER UUID '2340778c-7464-11ed-a721-7cd30abc99b4',INITIAL_TRX_XID '{hash:1380121015845354198, inc:16474501, "
        "addr:\"127.0.0.1:10000\", t:1694412306958599}',INITIAL_TRX_GTID_SEQ '31'";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());
  }

  {
    hsql::SQLParserResult result;
    std::string query = "CREATE BINLOG FOR TENANT `ob3x.admin`.`mysql` WITH CLUSTER URL "
                        "`http://127.0.0.1:8080/oceanbase_configer/v2/obtest_admin_127.0.0.1_110000_ob3x`";
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    ASSERT_EQ(hsql::COM_CREATE_BINLOG, result.getStatement(0)->type());
    hsql::CreateBinlogStatement* create_statement = (hsql::CreateBinlogStatement*)result.getStatement(0);
    ASSERT_EQ(hsql::CLUSTER_URL, create_statement->binlog_options->at(0)->option_type);
    ASSERT_STREQ("http://127.0.0.1:8080/oceanbase_configer/v2/obtest_admin_127.0.0.1_110000_ob3x",
        create_statement->binlog_options->at(0)->value);
  }
}

TEST(SQLParser, select)
{
  std::string query = "SELECT * FROM test;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, set)
{
  std::string query = "set master_binlog_checksum=100000;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, set_global)
{
  std::smatch sm;
  std::string query = "set global master_binlog_checksum=100000;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, set_session)
{
  std::smatch sm;
  std::string query = "set  session master_binlog_checksum=100000;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, set_local)
{
  std::string query = "set local master_binlog_checksum=100000;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, set_session_)
{
  std::string query = "set @master_binlog_checksum='CRC32',@slave_uuid=777;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, select_sum)
{
  std::string query = "select sum(1);";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, select_gtid_subset)
{
  std::string query = "SELECT GTID_SUBSET('1-1-1,2-2-2,3-3-3', '1-1-1,2-2-2');";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, select_gtid_subset_lower)
{
  std::string query = "SELECT gtid_subset('1-1-1,2-2-2,3-3-3', '1-1-1,2-2-2');";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, select_gtid_subtract)
{
  {
    std::string query = "select "
                        "gtid_subtract('d9dd8510-1321-11ee-a9a7-7cd30abc99b4:1-51774,d9dd8510-1321-11ee-a9a7-"
                        "7cd302bc99b4:1-51774','d9dd8510-1321-11ee-a9a7-7cd30abc99b4:1-51775');";
    hsql::SQLParserResult result;
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    hsql::SelectStatement* p_statement = (hsql::SelectStatement*)result.getStatement(0);
    auto gtid_sub = p_statement->selectList->at(0)->exprList->at(0)->getName();
    auto gtid_target = p_statement->selectList->at(0)->exprList->at(1)->getName();
    std::string sql = "";
    oceanbase::logproxy::BinlogFunction::gtid_subtract(gtid_sub, gtid_target, sql);
    ASSERT_STREQ(sql.c_str(), "d9dd8510-1321-11ee-a9a7-7cd302bc99b4:1-51774");
  }

  {
    std::string query = "select "
                        "gtid_subtract('d9dd8510-1321-11ee-a9a7-7cd30abc99b4:1-999994,d9dd8510-1321-11ee-a9a7-"
                        "7cd302bc99b4:1-51774','d9dd8510-1321-11ee-a9a7-7cd30abc99b4:1-51775');";
    hsql::SQLParserResult result;
    ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
    hsql::SelectStatement* p_statement = (hsql::SelectStatement*)result.getStatement(0);
    auto gtid_sub = p_statement->selectList->at(0)->exprList->at(0)->getName();
    auto gtid_target = p_statement->selectList->at(0)->exprList->at(1)->getName();
    std::string sql = "";
    oceanbase::logproxy::BinlogFunction::gtid_subtract(gtid_sub, gtid_target, sql);
    ASSERT_STREQ(
        sql.c_str(), "d9dd8510-1321-11ee-a9a7-7cd302bc99b4:1-51774,d9dd8510-1321-11ee-a9a7-7cd30abc99b4:51776-999994");
  }
}

TEST(SQLParser, select_master_binlog_checksum)
{
  std::string query = "SELECT @master_binlog_checksum;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_SELECT, result.getStatement(0)->type());
  auto* statement = (hsql::SelectStatement*)result.getStatement(0);
  hsql::Expr* expr = statement->selectList->at(0);
  ASSERT_EQ(hsql::kExprVar, expr->type);
  ASSERT_STREQ("master_binlog_checksum", expr->getName());
  ASSERT_EQ(hsql::kUser, expr->var_type);
  ASSERT_EQ(hsql::Session, expr->var_level);
}

TEST(SQLParser, set_master_heartbeat_period)
{
  std::string query = "SET @master_heartbeat_period=10";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_SET, result.getStatement(0)->type());
  auto* statement = (hsql::SetStatement*)result.getStatement(0);
  std::vector<hsql::SetClause*>* set_clause = statement->sets;
  ASSERT_STREQ("master_heartbeat_period", set_clause->at(0)->column);
  ASSERT_EQ(hsql::kUser, set_clause->at(0)->var_type);
  ASSERT_EQ(hsql::Session, set_clause->at(0)->type);
}

TEST(SQLParser, select_global)
{
  std::string query = "select @@system_variable_name;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  ASSERT_EQ(hsql::COM_SELECT, result.getStatement(0)->type());
  auto* statement = (hsql::SelectStatement*)result.getStatement(0);
  hsql::Expr* expr = statement->selectList->at(0);
  ASSERT_EQ(hsql::kExprVar, expr->type);
  ASSERT_STREQ("system_variable_name", expr->getName());
  ASSERT_EQ(hsql::kSys, expr->var_type);
  ASSERT_EQ(hsql::Global, expr->var_level);
}

TEST(SQLParser, show_var)
{
  std::string query = "SHOW VARIABLES LIKE 'max_connections';";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, show_gtid_executed)
{
  std::string query = "show global variables like 'gtid_executed'";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  hsql::ShowStatement* show_statement = (hsql::ShowStatement*)result.getStatement(0);
  ASSERT_EQ(hsql::Global, show_statement->var_type);
}

TEST(SQLParser, select_limit)
{
  std::string query = "select @@version_comment limit 1;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, drop_binlog)
{
  std::string query = "DROP BINLOG IF EXISTS FOR TENANT `ob4prf098hngao`.`t4prgngfyd4lc`";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
}

TEST(SQLParser, session)
{
  std::string query =
      "SET @@ob_enable_transmission_checksum = 1, @@ob_trace_info = 'client_ip=172.17.0.15', @@wait_timeout = 9999999, "
      "@@net_write_timeout = 7200, @@net_read_timeout = 7200, @@character_set_client = 63, @@character_set_results = "
      "63, @@character_set_connection = 63, @@collation_connection = 63, @master_binlog_checksum = 'CRC32', "
      "@slave_uuid = '3d241ade-167f-11ee-a9a7-7cd30abc99b4', @mariadb_slave_capability = X'34', "
      "@master_heartbeat_period = 15000000000;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  auto* statement = (hsql::SetStatement*)result.getStatement(0);
  std::vector<hsql::SetClause*>* set_clause = statement->sets;
  ASSERT_STREQ("ob_enable_transmission_checksum", set_clause->at(0)->column);
  ASSERT_STREQ("ob_trace_info", set_clause->at(1)->column);
  ASSERT_STREQ("wait_timeout", set_clause->at(2)->column);
  ASSERT_STREQ("net_write_timeout", set_clause->at(3)->column);
  ASSERT_STREQ("net_read_timeout", set_clause->at(4)->column);
  ASSERT_STREQ("character_set_client", set_clause->at(5)->column);
  ASSERT_STREQ("character_set_results", set_clause->at(6)->column);
  ASSERT_STREQ("mariadb_slave_capability", set_clause->at(11)->column);
  ASSERT_STREQ("4", set_clause->at(11)->value->get_value().c_str());
}

TEST(SQLParser, session_show_ddl)
{
  std::string query =
      "SET @@character_set_database = 28, @@character_set_server = 28, @@collation_database = 28, "
      "@@collation_server = 28, @@ob_trx_timeout = 10000000000000, @@ob_enable_transmission_checksum = "
      "1, @@ob_trx_idle_timeout = 10000000000, @@_show_ddl_in_compat_mode = 1, @@wait_timeout = "
      "9999999, @@net_write_timeout = 7200, @@net_read_timeout = 7200, @@character_set_client = 63, "
      "@@character_set_results = 63, @@character_set_connection = 63, @@collation_connection = 63, "
      "@@ob_query_timeout = 1000000000000, @_min_cluster_version = '4.1.0.1', @master_binlog_checksum "
      "= X'4352433332', @slave_uuid = '806f3835-209d-11ee-aa36-00163e0c9be7', "
      "@mariadb_slave_capability = X'34', @master_heartbeat_period = 15000000000, @master_binlog_checksum "
      "= 0x4352433332;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  auto* statement = (hsql::SetStatement*)result.getStatement(0);
  std::vector<hsql::SetClause*>* set_clause = statement->sets;
  ASSERT_STREQ("master_binlog_checksum", set_clause->at(17)->column);
  ASSERT_STREQ("CRC32", set_clause->at(17)->value->get_value().c_str());
  ASSERT_STREQ("4", set_clause->at(19)->value->get_value().c_str());
  ASSERT_STREQ("CRC32", set_clause->at(21)->value->get_value().c_str());
}

TEST(SQLParser, session_hex)
{
  {
    std::string query = "X'4352433332'";
    size_t len = strlen(query.c_str()) - 2;
    size_t end_pos = strlen(query.c_str());
    if ('\'' == query[strlen(query.c_str()) - 1]) {
      // Values written using X'val' notation
      --len;
      --end_pos;
    }
    query = hsql::substr(query.c_str(), 2, end_pos);
    std::string result = strdup(hsql::hex2bin(query.c_str(), strlen(query.c_str())));
    ASSERT_STREQ("CRC32", result.c_str());
  }

  {
    std::string query = "0x4352433332";
    size_t len = strlen(query.c_str()) - 2;
    size_t end_pos = strlen(query.c_str());
    if ('\'' == query[strlen(query.c_str()) - 1]) {
      // Values written using X'val' notation
      --len;
      --end_pos;
    }
    query = hsql::substr(query.c_str(), 2, end_pos);
    std::string result = strdup(hsql::hex2bin(query.c_str(), strlen(query.c_str())));
    ASSERT_STREQ("CRC32", result.c_str());
  }
}

TEST(SQLParser, show_binlog_events)
{
  std::string query = "show binlog events limit 1";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  auto* statement = result.getStatement(0);
  auto* p_statement = (hsql::ShowBinlogEventsStatement*)statement;
  std::string binlog_file;
  std::string start_pos;
  std::string offset_str;
  std::string limit_str;

  if (p_statement->binlog_file != nullptr) {
    binlog_file = p_statement->binlog_file->get_value();
  }

  if (p_statement->start_pos != nullptr) {
    start_pos = p_statement->start_pos->get_value();
  }

  if (p_statement->limit != nullptr) {
    if (p_statement->limit->offset != nullptr) {
      offset_str = p_statement->limit->offset->get_value();
    }

    if (p_statement->limit->limit != nullptr) {
      limit_str = p_statement->limit->limit->get_value();
    }
  }

  ASSERT_EQ(1, atoll(limit_str.c_str()));
}

TEST(SQLParser, show_binlog_events_limit)
{
  std::string query = "show binlog events limit 1;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  auto* statement = result.getStatement(0);
  auto* p_statement = (hsql::ShowBinlogEventsStatement*)statement;
  std::string binlog_file;
  std::string start_pos;
  std::string offset_str;
  std::string limit_str;

  if (p_statement->binlog_file != nullptr) {
    binlog_file = p_statement->binlog_file->get_value();
  }

  if (p_statement->start_pos != nullptr) {
    start_pos = p_statement->start_pos->get_value();
  }

  if (p_statement->limit != nullptr) {
    if (p_statement->limit->offset != nullptr) {
      offset_str = p_statement->limit->offset->get_value();
    }

    if (p_statement->limit->limit != nullptr) {
      limit_str = p_statement->limit->limit->get_value();
    }
  }

  ASSERT_EQ(1, atoll(limit_str.c_str()));
}

TEST(SQLParser, show_binlog_events_in_file_limit)
{
  std::string query = "show binlog events in 'binlog.000001' limit 1;";
  hsql::SQLParserResult result;
  ASSERT_EQ(OMS_OK, ObSqlParser::parse(query, result));
  auto* statement = result.getStatement(0);
  auto* p_statement = (hsql::ShowBinlogEventsStatement*)statement;
  std::string binlog_file;
  std::string start_pos;
  std::string offset_str;
  std::string limit_str;

  if (p_statement->binlog_file != nullptr) {
    binlog_file = p_statement->binlog_file->get_value();
  }

  if (p_statement->start_pos != nullptr) {
    start_pos = p_statement->start_pos->get_value();
  }

  if (p_statement->limit != nullptr) {
    if (p_statement->limit->offset != nullptr) {
      offset_str = p_statement->limit->offset->get_value();
    }

    if (p_statement->limit->limit != nullptr) {
      limit_str = p_statement->limit->limit->get_value();
    }
  }

  ASSERT_EQ(1, atoll(limit_str.c_str()));
  ASSERT_EQ("binlog.000001", binlog_file);
}