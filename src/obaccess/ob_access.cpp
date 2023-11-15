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

#include "common.h"
#include "log.h"
#include "str.h"
#include "config.h"
#include "jsonutil.hpp"
#include "communication/http.h"
#include "ob_sha1.h"
#include "ob_access.h"

#define QUERY_OB_VERSION "show parameters like 'min_observer_version'"

namespace oceanbase {
namespace logproxy {

static int parse_cluster_url(const std::string& cluster_url, std::vector<ObAccess::ServerInfo>& servers)
{
  HttpResponse response;
  int ret = HttpClient::get(cluster_url, response);
  if (ret != OMS_OK || response.code != 200) {
    OMS_STREAM_ERROR << "Failed to request cluster url:" << cluster_url;
    return OMS_FAILED;
  }
  /*
   * syntax:
   * {
   *    "Success":true,
   *    "Code":200,"Cost":2,
   *    "Message":"successful",
   *    "Data":{
   *       "ObCluster":"metacluster","Type":"PRIMARY","ObRegionId":100000,"ObClusterId":100000,
   *       "RsList":[{
   *          "sql_port":2881, "address":"127.0.0.1:2882","role":"LEADER"
   *       }],
   *       "ReadonlyRsList":[],
   *       "ObRegion":"metacluster","timestamp":1639835299202361
   *    }
   * }
   */
  std::string message;
  Json::Value rsinfo;
  if (!str2json(response.payload, rsinfo, &message) || !rsinfo.isObject()) {
    OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload << ", error:" << message;
    return OMS_FAILED;
  }
  Json::Value node = rsinfo["Data"];
  if (!node.isObject()) {
    OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"Data\"";
    return OMS_FAILED;
  }
  node = node["RsList"];
  if (!node.isArray() || node.empty()) {
    OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"RsList\"";
    return OMS_FAILED;
  }
  for (const Json::Value& rs : node) {
    if (!rs.isObject()) {
      OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload
                       << ", Invalid or None \"RsList\" item";
      return OMS_FAILED;
    }
    ObAccess::ServerInfo server;
    Json::Value field = rs["address"];
    if (!field.isString() || field.empty()) {
      OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload
                       << ", Invalid or None \"address\"";
      return OMS_FAILED;
    }
    server.host = field.asString();

    field = rs["sql_port"];
    if (!field.isNumeric() || field.asUInt() == 0) {
      OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload
                       << ", Invalid or None \"sql_port\"";
      return OMS_FAILED;
    }
    server.port = field.asUInt();

    std::vector<std::string> ipports;
    split(server.host, ':', ipports);
    if (ipports.empty()) {
      OMS_STREAM_ERROR << "Failed to parse cluster url response:" << response.payload
                       << ", Invalid \"address\":" << server.host;
      return OMS_FAILED;
    }
    server.host = ipports.front();
    servers.emplace_back(server);
  }
  return OMS_OK;
}

int ObAccess::init(const OblogConfig& hs_config, const std::string& password_sha1, const std::string& sys_password_sha1)
{
  _user = hs_config.user.val();
  _user_to_conn = _user;
  _password_sha1 = password_sha1;
  _sys_password_sha1 = sys_password_sha1;

  std::vector<std::string> sections;
  if (!hs_config.cluster_url.empty()) {
    if (parse_cluster_url(hs_config.cluster_url.val(), _servers) != OMS_OK) {
      return OMS_FAILED;
    }
    _user_to_conn = ObUsername(_user).name_without_cluster();
  } else {
    const std::string& root_servers = hs_config.root_servers.val();
    if (root_servers.empty()) {
      OMS_STREAM_ERROR << "Failed to init ObAccess caused by empty root_servers";
      return OMS_FAILED;
    }

    int ret = split(root_servers, ';', sections);
    if (ret == 0) {
      OMS_STREAM_ERROR << "Failed to init ObAccess caused by invalid root_servers";
      return OMS_FAILED;
    }

    for (auto& sec : sections) {
      std::vector<std::string> ipports;
      ret = split(sec, ':', ipports);
      if (ret != 3) {
        OMS_STREAM_ERROR << "Failed to init ObAccess caused by invalid root_servers:" << sec;
        return OMS_FAILED;
      }

      ServerInfo server = {ipports[0], atoi(ipports[2].c_str())};
      if (server.host.empty()) {
        OMS_STREAM_ERROR << "Failed to init ObAccess caused by empty root_server: " << sec;
        return OMS_FAILED;
      }
      if (server.port < 0 || server.port >= 65536) {
        OMS_STREAM_ERROR << "Failed to init ObAccess caused by invalid port:" << server.port;
        return OMS_FAILED;
      }
      _servers.emplace_back(server);
    }
  }

  if (_user_to_conn.empty() || _password_sha1.empty()) {
    OMS_ERROR("Failed to init ObAccess caused by empty user or password,user:{},pwd:{}", _user_to_conn, _password_sha1);
    return OMS_FAILED;
  }

  _sys_user = !hs_config.sys_user.empty() ? hs_config.sys_user.val() : Config::instance().ob_sys_username.val();
  if (_sys_user.empty() || _sys_password_sha1.empty()) {
    OMS_ERROR("Failed to init ObAccess caused by empty sys_user or sys_password,sys_usr:{},sys_password:{}",
        _sys_user,
        _sys_password_sha1);
    return OMS_FAILED;
  }

  int ret = _table_whites.from(hs_config.table_whites.val());
  if (ret != OMS_OK) {
    return ret;
  }

  return OMS_OK;
}

int ObAccess::auth()
{
  for (auto& server : _servers) {
    bool auth_by_sys = _table_whites.all_tenant || _table_whites.with_sys;
    int ret = auth_by_sys ? auth_sys(server) : auth_tenant(server);
    if (ret != OMS_OK) {
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}

int ObAccess::auth_sys(const ServerInfo& server)
{
  OMS_STREAM_INFO << "About to auth sys: " << _user_to_conn << " for observer: " << server.host << ":" << server.port;

  MysqlProtocol auther;
  // all tenant must be sys tenant;
  int ret = auther.login(server.host, server.port, _user_to_conn, _password_sha1);
  if (ret != OMS_OK) {
    return ret;
  }

  MySQLResultSet rs;
  ret = auther.query("show tenant", rs);
  if (ret != OMS_OK || rs.rows.empty()) {
    OMS_STREAM_ERROR << "Failed to auth, show tenant for sys all match mode, ret:" << ret;
    return OMS_FAILED;
  }

  const MySQLRow& row = rs.rows.front();
  const std::string& tenant = row.fields().front();
  if (tenant.size() < 3 || strncasecmp("sys", tenant.c_str(), 3) != 0) {
    OMS_STREAM_ERROR << "Failed to auth, all tenant mode or sys tenant must be connected as sys tenant, current: "
                     << tenant;
    return OMS_FAILED;
  }
  return OMS_OK;
}

int ObAccess::auth_tenant(const ServerInfo& server)
{
  OMS_STREAM_INFO << "About to auth user: " << _user << " for observer: " << server.host << ":" << server.port;

  // 1. found tenant server using sys
  MysqlProtocol sys_auther;
  int ret = sys_auther.login(server.host, server.port, _sys_user, _sys_password_sha1);
  if (ret != OMS_OK) {
    return ret;
  }

  ObUsername ob_user(_user);

  // 2. for each of tenant servers, login it.
  MySQLResultSet rs;
  for (auto& tenant_entry : _table_whites.tenants) {
    OMS_STREAM_INFO << "About to auth tenant:" << tenant_entry.first << " of user:" << _user_to_conn;

    rs.reset();
    ret = sys_auther.query(
        "SELECT server.svr_ip, server.inner_port, server.zone, tenant.tenant_id, tenant.tenant_name FROM "
        "oceanbase.__all_resource_pool AS pool, oceanbase.__all_unit AS unit, oceanbase.__all_server AS "
        "server, oceanbase.__all_tenant AS tenant WHERE tenant.tenant_id=pool.tenant_id AND "
        "unit.resource_pool_id=pool.resource_pool_id AND unit.svr_ip=server.svr_ip AND "
        "unit.svr_port=server.svr_port AND tenant.tenant_name='" +
            tenant_entry.first + "'",
        rs);
    if (ret != OMS_OK) {
      OMS_STREAM_ERROR << "Failed to auth, failed to query tenant server for:" << tenant_entry.first << ", ret:" << ret;
      return OMS_FAILED;
    }
    if (rs.rows.empty() || rs.col_count < 3) {
      OMS_STREAM_ERROR << "Failed to auth, unexpected result set, row count:" << rs.rows.size()
                       << ", col count:" << rs.col_count << ", ret:" << ret;
      return OMS_FAILED;
    }
    const MySQLRow& row = rs.rows.front();
    const std::string& host = row.fields()[0];
    const uint16_t sql_port = atoi(row.fields()[1].c_str());

    MysqlProtocol user_auther;
    ret = user_auther.login(host, sql_port, ob_user.name_without_cluster(tenant_entry.first), _password_sha1);
    if (ret != OMS_OK) {
      OMS_STREAM_ERROR << "Failed to auth from tenant server: " << host << ":" << sql_port << ", ret:" << ret;
      return ret;
    }

    if (!tenant_entry.second.all_database) {
      if (auth_tables(tenant_entry.second.databases, ob_user.username, user_auther) != OMS_OK) {
        return OMS_FAILED;
      }
    }
  }
  return OMS_OK;
}

int ObAccess::auth_tables(
    const std::map<std::string, std::set<std::string>>& tables, const std::string& username, MysqlProtocol& connection)
{
  MySQLResultSet rs;
  int ret = connection.query("show tenant", rs);
  if (ret != OMS_OK || rs.rows.empty()) {
    if (rs.message.find("syntax") != std::string::npos) {
      // syntax error means oracle mode,
      // TODO...
      return OMS_OK;
    }
  }

  /**
    SELECT * FROM information_schema.user_privileges;
    +--------------------------------------+---------------+----------------------+--------------+
    | GRANTEE                              | TABLE_CATALOG | PRIVILEGE_TYPE       | IS_GRANTABLE |
    +--------------------------------------+---------------+----------------------+--------------+
    | 'root'@'%'                           | def           | ALTER                | YES          |
    | 'root'@'%'                           | def           | CREATE               | YES          |
    | 'root'@'%'                           | def           | CREATE USER          | YES          |
    | 'root'@'%'                           | def           | DELETE               | YES          |
    | 'root'@'%'                           | def           | DROP                 | YES          |
    | 'root'@'%'                           | def           | INSERT               | YES          |
    | 'root'@'%'                           | def           | UPDATE               | YES          |
    | 'root'@'%'                           | def           | SELECT               | YES          |
    | 'root'@'%'                           | def           | INDEX                | YES          |
    | 'root'@'%'                           | def           | CREATE VIEW          | YES          |
    | 'root'@'%'                           | def           | SHOW VIEW            | YES          |
    | 'root'@'%'                           | def           | SHOW DB              | YES          |
    | 'root'@'%'                           | def           | SUPER                | YES          |
    | 'root'@'%'                           | def           | PROCESS              | YES          |
    | 'root'@'%'                           | def           | CREATE SYNONYM       | YES          |
    | 'root'@'%'                           | def           | FILE                 | YES          |
    | 'root'@'%'                           | def           | ALTER TENANT         | YES          |
    | 'root'@'%'                           | def           | ALTER SYSTEM         | YES          |
    | 'root'@'%'                           | def           | CREATE RESOURCE POOL | YES          |
    | 'root'@'%'                           | def           | CREATE RESOURCE UNIT | YES          |
   */

  std::string username_pattern = "'" + username + "'@%";
  for (auto& db_entry : tables) {
    if (db_entry.first == "*") {
      continue;
    }
    for (const std::string& tb : db_entry.second) {
      if (tb == "*") {
        continue;
      }
      std::string tb_priv_sql = "SELECT upper(privilege_type) AS priv FROM information_schema.user_privileges WHERE "
                                "privilege_type='SELECT' AND grantee LIKE \"";
      tb_priv_sql.append(username_pattern);
      tb_priv_sql.append("\" UNION ");
      tb_priv_sql.append(" SELECT upper(privilege_type) AS priv FROM information_schema.schema_privileges WHERE "
                         "privilege_type='SELECT' AND grantee LIKE \"");
      tb_priv_sql.append(username_pattern + "\" AND table_schema='" + db_entry.first);
      tb_priv_sql.append("' UNION ");
      tb_priv_sql.append(" SELECT upper(privilege_type) AS priv FROM information_schema.table_privileges WHERE "
                         "privilege_type='SELECT' AND grantee LIKE \"");
      tb_priv_sql.append(username_pattern);
      tb_priv_sql.append("\" AND table_schema='" + db_entry.first + "' AND table_name='" + tb + "'");

      connection.query(tb_priv_sql, rs);
      if (ret != OMS_OK || rs.rows.empty()) {
        OMS_STREAM_ERROR << "Failed to auth, fetch user_privileges failure or empty result, ret:" << rs.code
                         << ", error: " << rs.message;
        return OMS_FAILED;
      }
    }
  }
  return OMS_OK;
}

int ObAccess::fetch_connection(MysqlProtocol& mysql_protocol)
{
  ServerInfo server_info;
  if (!_servers.empty()) {
    server_info = _servers.front();
  }
  return mysql_protocol.login(server_info.host, server_info.port, _sys_user, _sys_password_sha1);
}

int ObAccess::query_ob_version(const OblogConfig& config, std::string& ob_version)
{
  int ret = init(config, config.password_sha1, config.sys_password_sha1);
  if (OMS_OK != ret) {
    return ret;
  }

  MysqlProtocol sys_user;
  ret = fetch_connection(sys_user);
  if (OMS_OK != ret) {
    return ret;
  }

  MySQLResultSet rs;
  ret = sys_user.query(QUERY_OB_VERSION, rs);
  if (OMS_OK != ret) {
    OMS_ERROR("Failed to fetch OB version, code: {}, error: {}", rs.code, rs.message);
    return ret;
  }

  ob_version = rs.rows.front().fields()[6];
  if (ob_version.empty()) {
    OMS_ERROR("Failed to parse OB version from MySQLResultSet.");
    return OMS_FAILED;
  }
  OMS_INFO("OB version: {}", ob_version);

  return OMS_OK;
}

ObUsername::ObUsername(const std::string& full_name)
{
  static const char SEP_USER_AT_TENANT = '@';
  static const char SEP_TENANT_AT_CLUSTER = '#';
  static const char SEP = ':';

  auto cluster_entry = full_name.find(SEP_TENANT_AT_CLUSTER);
  auto tenant_entry = full_name.find(SEP_USER_AT_TENANT);
  auto sep_entry = full_name.find(SEP);

  std::vector<std::string> sections;
  split_all(full_name, {SEP_USER_AT_TENANT, SEP_TENANT_AT_CLUSTER, SEP}, sections);

  if (cluster_entry != std::string::npos && tenant_entry != std::string::npos) {
    // user@tenant#cluster
    cluster = std::move(sections[2]);
    tenant = std::move(sections[1]);
    username = std::move(sections[0]);
  } else if (cluster_entry == std::string::npos && tenant_entry != std::string::npos) {
    // user@tenant
    tenant = std::move(sections[1]);
    username = std::move(sections[0]);
  } else if (sep_entry != std::string::npos && sections.size() == 3) {
    // cluster:tenant:user
    cluster = std::move(sections[0]);
    tenant = std::move(sections[1]);
    username = std::move(sections[2]);
  } else {
    // username
    username = full_name;
  }
}

std::string ObUsername::name_without_cluster(const std::string& in_tenant)
{
  std::string conn_user = username;
  if (!tenant.empty()) {
    conn_user.append("@");
    conn_user.append(tenant);
  } else if (!in_tenant.empty()) {
    conn_user.append("@");
    conn_user.append(in_tenant);
  }
  return conn_user;
}

}  // namespace logproxy
}  // namespace oceanbase
