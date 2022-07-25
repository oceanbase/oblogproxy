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
#include <sstream>
#include "common/log.h"
#include "common/str.h"
#include "obaccess/oblog_config.h"

namespace oceanbase {
namespace logproxy {

OblogConfig::OblogConfig(const std::string& str)
{
  // syntax: "k1=v1 k2=v2 k3=v3"
  std::vector<std::string> kvs;
  split(str, ' ', kvs);
  for (std::string& kv : kvs) {
    std::vector<std::string> kv_split;
    int count = split(kv, '=', kv_split, true);
    if (count != 2) {
      continue;
    }
    add(kv_split[0], kv_split[1]);
  }
}

void OblogConfig::add(const std::string& key, const std::string& value)
{
  auto entry = _configs.find(key);
  if (entry == _configs.end()) {
    _extras.emplace(key, value);
  } else {
    entry->second->from_str(value);
  }
}

void OblogConfig::set(const std::string& key, const std::string& value)
{
  auto entry = _configs.find(key);
  if (entry == _configs.end()) {
    auto eentry = _extras.find(key);
    if (eentry == _extras.end()) {
      _extras.emplace(key, value);
    } else {
      eentry->second = value;
    }
  } else {
    entry->second->from_str(value);
  }
}

void OblogConfig::generate_configs(std::map<std::string, std::string>& configs) const
{
  for (auto& entry : _configs) {
    const std::string& val = entry.second->debug_str();
    if (val.empty()) {
      continue;
    }
    configs.emplace(entry.first, val);
  }
  for (auto& entry : _extras) {
    if (entry.second.empty()) {
      continue;
    }
    configs.emplace(entry.first, entry.second);
  }
}

std::string OblogConfig::generate_config_str() const
{
  std::stringstream ss;
  for (auto& entry : _configs) {
    ss << entry.first << '=' << entry.second->debug_str() << ' ';
  }
  for (auto& entry : _extras) {
    ss << entry.first << '=' << entry.second << ' ';
  }
  return ss.str();
}

std::string OblogConfig::debug_str(bool formatted) const
{
  std::stringstream ss;
  for (auto& entry : _configs) {
    ss << entry.first << ":" << entry.second->debug_str() << ",";
    if (formatted) {
      ss << '\n';
    }
  }
  return ss.str();
}

int TenantDbTable::from(const std::string& table_whites)
{
  std::vector<std::string> sections;
  split(table_whites, '|', sections);
  for (auto& section : sections) {
    std::vector<std::string> items;
    int count = split(section, '.', items);
    if (count != 3) {
      OMS_ERROR << "Failed to parse table_white_list, invalid syntax:" << section;
      return OMS_FAILED;
    }

    const std::string& tenant = items[0];
    if (tenant == "sys") {
      with_sys = true;
    }
    if (tenant == "*") {
      with_sys = true;
      all_tenant = true;
      tenants.clear();
      return OMS_OK;
    }

    auto tenant_entry = tenants.find(tenant);
    if (tenant_entry == tenants.end()) {
      tenants.emplace(tenant, DbTable());
      tenant_entry = tenants.find(tenant);
    } else if (tenant_entry->second.all_database) {
      continue;
    }

    const std::string& database = items[1];
    if (database == "*") {
      tenant_entry->second.all_database = true;
      tenant_entry->second.databases.clear();
      continue;
    }

    const std::string& table = items[2];
    auto table_entry = tenant_entry->second.databases.find(database);
    if (table_entry == tenant_entry->second.databases.end()) {
      tenant_entry->second.databases.emplace(database, std::set<std::string>{table});
    } else if (!table_entry->second.count("*")) {
      table_entry->second.emplace(table);
    }
  }
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
