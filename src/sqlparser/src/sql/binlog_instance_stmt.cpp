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

#include "binlog_instance_stmt.h"

#include <algorithm>

namespace hsql {

hsql::CreateBinlogInstanceStatement::CreateBinlogInstanceStatement()
    : SQLStatement(COM_CREATE_BINLOG_INSTANCE), instance_name(nullptr), tenant(nullptr), instance_options(nullptr)
{}

hsql::CreateBinlogInstanceStatement::~CreateBinlogInstanceStatement()
{
  free(instance_name);
  instance_name = nullptr;
  delete tenant;
  if (nullptr != instance_options) {
    for (SetClause* clause : *instance_options) {
      if (nullptr != clause->column) {
        free(clause->column);
      }
      if (nullptr != clause->value) {
        delete clause->value;
      }
      delete clause;
    }
    delete instance_options;
  }
}

AlterBinlogInstanceStatement::AlterBinlogInstanceStatement()
    : SQLStatement(COM_ALTER_BINLOG_INSTANCE), instance_name(nullptr), instance_options(nullptr)
{}

hsql::AlterBinlogInstanceStatement::~AlterBinlogInstanceStatement()
{
  free(instance_name);
  instance_name = nullptr;
  if (nullptr != instance_options) {
    for (SetClause* clause : *instance_options) {
      if (nullptr != clause->column) {
        free(clause->column);
      }
      if (nullptr != clause->value) {
        delete clause->value;
      }
      delete clause;
    }
    delete instance_options;
  }
}

ShowBinlogInstanceStatement::ShowBinlogInstanceStatement()
    : SQLStatement(COM_SHOW_BINLOG_INSTANCE),
      mode(ShowInstanceMode::UNKNOWN),
      instance_names(nullptr),
      tenant(nullptr),
      ip(nullptr),
      zone(nullptr),
      region(nullptr),
      group(nullptr)
{}

ShowBinlogInstanceStatement::~ShowBinlogInstanceStatement()
{
  if (nullptr != instance_names) {
    for (char* instance_name : *instance_names) {
      free(instance_name);
    }
    delete instance_names;
  }

  delete tenant;
  free(ip);
  ip = nullptr;
  free(zone);
  zone = nullptr;
  free(region);
  region = nullptr;
  free(group);
  group = nullptr;
}

StartBinlogInstanceStatement::StartBinlogInstanceStatement()
    : SQLStatement(COM_START_BINLOG_INSTANCE), instance_name(nullptr), flag(InstanceFlag::BOTH)
{}

StartBinlogInstanceStatement::~StartBinlogInstanceStatement()
{
  free(instance_name);
  instance_name = nullptr;
}

StopBinlogInstanceStatement::StopBinlogInstanceStatement()
    : SQLStatement(COM_STOP_BINLOG_INSTANCE), instance_name(nullptr), flag(InstanceFlag::BOTH)
{}

StopBinlogInstanceStatement::~StopBinlogInstanceStatement()
{
  free(instance_name);
  instance_name = nullptr;
}

DropBinlogInstanceStatement::DropBinlogInstanceStatement()
    : SQLStatement(COM_DROP_BINLOG_INSTANCE), instance_name(nullptr)
{}

DropBinlogInstanceStatement::~DropBinlogInstanceStatement()
{
  free(instance_name);
  instance_name = nullptr;
}

}  // namespace hsql
