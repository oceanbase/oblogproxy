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

#pragma once
#include "log.h"
namespace oceanbase {
namespace logproxy {

class TableId {
private:
  /* In table map event and rows events, table id is 6 bytes.*/
  static const unsigned long long TABLE_ID_MAX = (~0ULL >> 16);
  uint64_t m_id;

public:
  TableId() : m_id(0)
  {}
  explicit TableId(unsigned long long id) : m_id(id)
  {}

  unsigned long long id() const
  {
    return m_id;
  }
  bool is_valid() const
  {
    return m_id <= TABLE_ID_MAX;
  }

  void operator=(const TableId& tid)
  {
    m_id = tid.m_id;
  }
  void operator=(unsigned long long id)
  {
    m_id = id;
  }

  bool operator==(const TableId& tid) const
  {
    return m_id == tid.m_id;
  }
  bool operator!=(const TableId& tid) const
  {
    return m_id != tid.m_id;
  }

  /* Support implicit type converting from Table_id to unsigned long long */
  operator unsigned long long() const
  {
    return m_id;
  }

  TableId operator++(int)
  {
    TableId id(m_id);

    /* m_id is reset to 0, when it exceeds the max value. */
    m_id = (m_id == TABLE_ID_MAX ? 0 : m_id + 1);
    assert(m_id <= TABLE_ID_MAX);
    return id;
  }
};
static TableId last_table_id;

struct TableName {
  std::string db_name;
  std::string tb_name;

  std::size_t operator()(const TableName& table_name) const
  {
    std::size_t h1 = std::hash<std::string>{}(table_name.db_name);
    std::size_t h2 = std::hash<std::string>{}(table_name.tb_name);
    return h1 ^ (h2 << 1);
  }
  bool operator==(const TableName& other) const
  {
    return (db_name == other.db_name) && (tb_name == other.tb_name);
  }
};
class TableCache {
private:
  std::unordered_map<TableName, uint64_t, TableName> _cache;

public:
  uint64_t assign_new_table_id();

  uint64_t get_table_id(const std::string& db_name, const std::string& tb_name);

  void refresh_table_id(const std::string& db_name, const std::string& tb_name);
};

}  // namespace logproxy
}  // namespace oceanbase