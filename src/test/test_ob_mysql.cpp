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

#include <vector>

#include "obaccess/mysql_protocol.h"
#include "obaccess/ob_sha1.h"
#include "obaccess/ob_access.h"
#include "gtest/gtest.h"

using namespace oceanbase::logproxy;
using namespace std;

TEST(MYSQL_AUTH, obusername)
{
  ObUsername username1("cluster:tenant:username");
  ASSERT_STREQ("cluster", username1.cluster.c_str());
  ASSERT_STREQ("tenant", username1.tenant.c_str());
  ASSERT_STREQ("username", username1.username.c_str());

  ObUsername username2("cluster:tenant");
  ASSERT_STREQ("", username2.cluster.c_str());
  ASSERT_STREQ("", username2.tenant.c_str());
  ASSERT_STREQ("cluster:tenant", username2.username.c_str());

  ObUsername username3("username@tenant#cluster");
  ASSERT_STREQ("cluster", username3.cluster.c_str());
  ASSERT_STREQ("tenant", username3.tenant.c_str());
  ASSERT_STREQ("username", username3.username.c_str());

  ObUsername username4("username@tenant");
  ASSERT_STREQ("tenant", username4.tenant.c_str());
  ASSERT_STREQ("username", username4.username.c_str());
}

int do_sha_password(const std::string& pswd, std::string& sha_password)
{
  SHA1 sha1;
  sha1.input((const unsigned char*)pswd.c_str(), pswd.size());
  sha_password.resize(SHA1::SHA1_HASH_SIZE);
  return sha1.get_result((unsigned char*)sha_password.data());
}

void login(const std::string& host, int port, const std::string& user, const std::string& passwd,
    const std::string& sql, MySQLResultSet& rs)
{
  std::string sha_password;
  int ret = do_sha_password(passwd, sha_password);
  ASSERT_EQ(ret, 0);
  MysqlProtocol auther;
  int auth_result = auther.login(host, port, user, sha_password);
  ASSERT_EQ(auth_result, 0);

  ret = auther.query(sql, rs);
  ASSERT_EQ(ret, 0);

  LogStream ls(0, "", 0, nullptr);
  ls << "| column counts:" << rs.cols.size() << " |\n";
  for (MySQLRow& row : rs.rows) {
    ls << "| ";
    for (const std::string& field : row.fields()) {
      ls << field << " | ";
    }
    ls << "\n";
  }
  ls << "| " << rs.rows.size() << " rows returned |";
  OMS_INFO << "\n" << ls.str();
}

static std::string host = "127.0.0.1";
static uint16_t port = 2883;
static std::string user = "root@tenant#cluster";
static std::string password = "password";
static std::string password_sha1;
static std::string tenant = "tenant";

static std::string sys_user = "root@sys";
static std::string sys_password = "root";
static std::string sys_password_sha1;

static std::string cluster_url = "";

// static std::vector<std::string> sqls = {"select 1"};
static std::vector<std::string> sqls = {"show tenant", "select version()"};
static std::vector<std::string> sqls_for_sys = {
    "SELECT server.svr_ip, server.svr_port, server.zone, tenant.tenant_id, tenant.tenant_name from "
    "oceanbase.__all_resource_pool AS pool, oceanbase.__all_unit AS unit, oceanbase.__all_server AS "
    "server, oceanbase.__all_tenant AS tenant WHERE tenant.tenant_id = pool.tenant_id AND "
    "unit.resource_pool_id = pool.resource_pool_id AND unit.svr_ip = server.svr_ip AND "
    "unit.svr_port = server.svr_port AND tenant.tenant_name='" +
    tenant + "'"};

TEST(MYSQL_AUTH, query)
{
  MySQLResultSet rs;
  for (auto& sql : sqls) {
    login(host, port, user, password, sql, rs);
  }
  for (auto& sql : sqls_for_sys) {
    login(host, port, sys_user, sys_password, sql, rs);
  }
}

TEST(MYSQL_AUTH, auth_ok)
{
  Config::instance().ob_sys_username.set(sys_user);
  Config::instance().ob_sys_password.set(sys_password);

  do_sha_password(password, password_sha1);
  do_sha_password(sys_password, sys_password_sha1);

  OblogConfig config("first_start_timestamp=0 rootserver_list=" + host + ":2881:" + std::to_string(port) +
                     " cluster_user=" + user + " cluster_password=" + dumphex(password_sha1) +
                     " tb_white_list=" + tenant + ".*.*");
  std::string rslist = host + ":2881:" + std::to_string(port);
  ASSERT_STREQ(rslist.c_str(), config.root_servers.val().c_str());
  ASSERT_STREQ(user.c_str(), config.user.val().c_str());
  ASSERT_TRUE(strncmp(password_sha1.c_str(), config.password.val().c_str(), SHA1::SHA1_HASH_SIZE));

  ObAccess ob_access;
  int ret = ob_access.init(config);
  ASSERT_EQ(0, ret);

  ret = ob_access.auth();
  ASSERT_EQ(OMS_OK, ret);
}

TEST(MYSQL_AUTH, auth_all_ok)
{
  do_sha_password(password, password_sha1);
  do_sha_password(sys_password, sys_password_sha1);

  Config::instance().ob_sys_username.set(sys_user);
  Config::instance().ob_sys_password.set(sys_password);

  OblogConfig config("first_start_timestamp=0 rootserver_list=" + host + ":2881:" + std::to_string(port) +
                     " cluster_user=" + sys_user + " cluster_password=" + dumphex(sys_password_sha1) +
                     " tb_white_list=*.*.*");
  ObAccess ob_access;
  int ret = ob_access.init(config);
  ASSERT_EQ(0, ret);

  ret = ob_access.auth();
  ASSERT_EQ(OMS_OK, ret);
}

TEST(MYSQL_AUTH, auth_all_failed)
{
  do_sha_password(password, password_sha1);
  do_sha_password(sys_password, sys_password_sha1);

  Config::instance().ob_sys_username.set(sys_user);
  Config::instance().ob_sys_password.set(sys_password);

  OblogConfig config("first_start_timestamp=0 rootserver_list=" + host + ":2881:" + std::to_string(port) +
                     " cluster_user=" + user + " cluster_password=" + dumphex(password_sha1) + " tb_white_list=*.*.*");
  ObAccess ob_access;
  int ret = ob_access.init(config);
  ASSERT_EQ(0, ret);

  ret = ob_access.auth();
  ASSERT_EQ(OMS_FAILED, ret);
}

TEST(MYSQL_AUTH, auth_by_cluster_url)
{
  do_sha_password(password, password_sha1);
  do_sha_password(sys_password, sys_password_sha1);

  Config::instance().ob_sys_username.set(sys_user);
  Config::instance().ob_sys_password.set(sys_password);

  OblogConfig config("first_start_timestamp=0 rootserver_list=" + host + ":2881:" + std::to_string(port) +
                     " cluster_user=" + sys_user + " cluster_password=" + dumphex(sys_password_sha1) +
                     " tb_white_list=*.*.* cluster_url=" + cluster_url);
  ObAccess ob_access;
  int ret = ob_access.init(config);
  ASSERT_EQ(0, ret);

  ret = ob_access.auth();
  ASSERT_EQ(OMS_OK, ret);
}