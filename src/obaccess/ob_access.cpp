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

#include "common/common.h"
#include "common/log.h"
#include "common/str.h"
#include "common/config.h"
#include "common/jsonutil.hpp"
#include "communication/http.h"
#include "obaccess/ob_sha1.h"
#include "obaccess/ob_access.h"
#include "obaccess/mysql_protocol.h"

namespace oceanbase {
namespace logproxy {

static int parse_cluster_url(const std::string& cluster_url, std::vector<ObAccess::ServerInfo>& servers)
{
  HttpResponse response;
  int ret = HttpClient::get(cluster_url, response);
  if (ret != OMS_OK || response.code != 200) {
    OMS_ERROR << "Failed to request cluster url:" << cluster_url;
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
    OMS_ERROR << "Failed to parse cluster url response:" << response.payload << ", error:" << message;
    return OMS_FAILED;
  }
  Json::Value node = rsinfo["Data"];
  if (!node.isObject()) {
    OMS_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"Data\"";
    return OMS_FAILED;
  }
  node = node["RsList"];
  if (!node.isArray() || node.empty()) {
    OMS_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"RsList\"";
    return OMS_FAILED;
  }
  for (const Json::Value& rs : node) {
    if (!rs.isObject()) {
      OMS_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"RsList\" item";
      return OMS_FAILED;
    }
    ObAccess::ServerInfo server;
    Json::Value field = rs["address"];
    if (!field.isString() || field.empty()) {
      OMS_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"address\"";
      return OMS_FAILED;
    }
    server.host = field.asString();

    field = rs["sql_port"];
    if (!field.isNumeric() || field.asUInt() == 0) {
      OMS_ERROR << "Failed to parse cluster url response:" << response.payload << ", Invalid or None \"sql_port\"";
      return OMS_FAILED;
    }
    server.port = field.asUInt();

    std::vector<std::string> ipports;
    split(server.host, ':', ipports);
    if (ipports.empty()) {
      OMS_ERROR << "Failed to parse cluster url response:" << response.payload
                << ", Invalid \"address\":" << server.host;
      return OMS_FAILED;
    }
    server.host = ipports.front();
    servers.emplace_back(server);
  }
  return OMS_OK;
}

int ObAccess::init(const OblogConfig& config)
{
  if (!config.cluster_url.empty()) {
    if (parse_cluster_url(config.cluster_url.val(), _servers) != OMS_OK) {
      return OMS_FAILED;
    }
  } else {

    const std::string& root_servers = config.root_servers.val();
    if (root_servers.empty()) {
      OMS_ERROR << "Failed to init ObAccess caused by empty root_servers";
      return OMS_FAILED;
    }

    std::vector<std::string> sections;
    int ret = split(root_servers, ';', sections);
    if (ret == 0) {
      OMS_ERROR << "Failed to init ObAccess caused by invalid root_servers";
      return OMS_FAILED;
    }

    for (auto& sec : sections) {
      std::vector<std::string> ipports;
      ret = split(sec, ':', ipports);
      if (ret != 3) {
        OMS_ERROR << "Failed to init ObAccess caused by invalid root_servers:" << sec;
        return OMS_FAILED;
      }

      ServerInfo server = {ipports[0], atoi(ipports[2].c_str())};
      if (server.host.empty()) {
        OMS_ERROR << "Failed to init ObAccess caused by empty root_server: " << sec;
        return OMS_FAILED;
      }
      if (server.port < 0 || server.port >= 65536) {
        OMS_ERROR << "Failed to init ObAccess caused by invalid port:" << server.port;
        return OMS_FAILED;
      }
      _servers.emplace_back(server);
    }
  }

  _user = config.user.val();
  hex2bin(config.password.val().c_str(), config.password.val().size(), _password_sha1);

  if (_user.empty() || _password_sha1.empty()) {
    OMS_ERROR << "Failed to init ObAccess caused by empty user or password";
    return OMS_FAILED;
  }

  _sys_user = config.sys_user.empty() ? Config::instance().ob_sys_username.val() : config.sys_user.val();
  if (config.sys_password.empty()) {
    MysqlProtocol::do_sha_password(Config::instance().ob_sys_password.val(), _sys_password_sha1);
  } else {
    MysqlProtocol::do_sha_password(config.sys_password.val(), _sys_password_sha1);
  }
  if (_sys_user.empty() || _sys_password_sha1.empty()) {
    OMS_ERROR << "Failed to init ObAccess caused by empty sys_user or sys_password";
    return OMS_FAILED;
  }

  int ret = _table_whites.from(config.table_whites.val());
  if (ret != OMS_OK) {
    return ret;
  }

  return OMS_OK;
}

int ObAccess::auth()
{
  for (auto& server : _servers) {
    int ret = _table_whites.all_tenant ? auth_sys(server) : auth_tenant(server);
    if (ret != OMS_OK) {
      return OMS_FAILED;
    }
  }
  return OMS_OK;
}

int ObAccess::auth_sys(const ServerInfo& server)
{
  MysqlProtocol auther;
  // all tenant must be sys tenant;
  int ret = auther.login(server.host, server.port, _user, _password_sha1);
  if (ret != OMS_OK) {
    return ret;
  }

  MysqlResultSet rs;
  ret = auther.query("show tenant", rs);
  if (ret != OMS_OK || rs.rows.empty()) {
    OMS_ERROR << "Failed to auth, show tenant for sys all match mode, ret:" << ret;
    return OMS_FAILED;
  }

  const MysqlRow& row = rs.rows.front();
  const std::string& tenant = row.fields().front();
  if (tenant.size() < 3 || strncasecmp("sys", tenant.c_str(), 3) != 0) {
    OMS_ERROR << "Failed to auth, all tenant mode must be sys tenant, current: " << tenant;
    return OMS_FAILED;
  }
  return OMS_OK;
}

int ObAccess::auth_tenant(const ServerInfo& server)
{
  // 1. found tenant server using sys
  MysqlProtocol sys_auther;
  int ret = sys_auther.login(server.host, server.port, _sys_user, _sys_password_sha1);
  if (ret != OMS_OK) {
    return ret;
  }

  ObUsername ob_user(_user);

  // 2. for each of tenant servers, login it.
  MysqlResultSet rs;
  for (auto& tenant_entry : _table_whites.tenants) {
    OMS_INFO << "About to auth tenant:" << tenant_entry.first << " of user:" << _user;

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
      OMS_ERROR << "Failed to auth, failed to query tenant server for:" << tenant_entry.first << ", ret:" << ret;
      return OMS_FAILED;
    }
    if (rs.rows.empty() || rs.col_count < 3) {
      OMS_ERROR << "Failed to auth, unexpected result set, row count:" << rs.rows.size()
                << ", col count:" << rs.col_count << ", ret:" << ret;
      return OMS_FAILED;
    }
    const MysqlRow& row = rs.rows.front();
    const std::string& host = row.fields()[0];
    const uint16_t sql_port = atoi(row.fields()[1].c_str());

    MysqlProtocol user_auther;
    std::string conn_user = ob_user.username;
    conn_user.append("@");
    conn_user.append(ob_user.tenant.empty() ? tenant_entry.first : ob_user.tenant);
    ret = user_auther.login(host, sql_port, conn_user, _password_sha1);
    if (ret != OMS_OK) {
      OMS_ERROR << "Failed to auth from tenant server: " << host << ":" << sql_port << ", ret:" << ret;
      return ret;
    }
  }
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

}  // namespace logproxy
}  // namespace oceanbase
