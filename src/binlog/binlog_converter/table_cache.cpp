/**
 * Copyright (c) 2024 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "table_cache.h"
namespace oceanbase {
namespace logproxy {
uint64_t TableCache::assign_new_table_id()
{
  uint64_t table_id = last_table_id++;
  if (table_id == 0) {
    table_id = assign_new_table_id();
    _cache.clear();
  }
  return table_id;
}

uint64_t TableCache::get_table_id(const std::string& db_name, const std::string& tb_name)
{
  TableName table_name = TableName{db_name, tb_name};
  auto it = _cache.find(table_name);
  if (it != _cache.end()) {
    return it->second;
  } else {
    auto table_id = assign_new_table_id();
    _cache.insert({{db_name, tb_name}, table_id});
    return table_id;
  }
}

void TableCache::refresh_table_id(const std::string& db_name, const std::string& tb_name)
{
  if (db_name.empty() || tb_name.empty()) {
    return;
  }
  auto table_id = assign_new_table_id();
  _cache.insert_or_assign({db_name, tb_name}, table_id);
}

}  // namespace logproxy
}  // namespace oceanbase