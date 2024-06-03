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

#include "SQLStatement.h"

namespace hsql {

enum ShowInstanceMode { INSTANCE, TENANT, IP, ZONE, REGION, GROUP, UNKNOWN };
enum InstanceFlag { BOTH, PROCESS_ONLY, OBCDC_ONLY };

struct CreateBinlogInstanceStatement : SQLStatement {
  CreateBinlogInstanceStatement();
  ~CreateBinlogInstanceStatement() override;

  char* instance_name;
  TenantName* tenant;
  std::vector<SetClause*>* instance_options;
};

struct ShowBinlogInstanceStatement : SQLStatement {
  ShowBinlogInstanceStatement();
  ~ShowBinlogInstanceStatement() override;

  bool history = false;
  ShowInstanceMode mode;
  std::vector<char*>* instance_names;
  TenantName* tenant;
  char* ip;
  char* zone;
  char* region;
  char* group;
};

struct AlterBinlogInstanceStatement : SQLStatement {
  AlterBinlogInstanceStatement();
  ~AlterBinlogInstanceStatement() override;

  char* instance_name;
  std::vector<SetClause*>* instance_options;
};

struct StartBinlogInstanceStatement : SQLStatement {
  StartBinlogInstanceStatement();
  ~StartBinlogInstanceStatement() override;

  char* instance_name;
  InstanceFlag flag;
};

struct StopBinlogInstanceStatement : SQLStatement {
  StopBinlogInstanceStatement();
  ~StopBinlogInstanceStatement() override;

  char* instance_name;
  InstanceFlag flag;
};

struct DropBinlogInstanceStatement : SQLStatement {
  DropBinlogInstanceStatement();
  ~DropBinlogInstanceStatement() override;

  char* instance_name;
  bool force = false;
};

struct SwitchMasterInstanceStatement : SQLStatement {
  SwitchMasterInstanceStatement();
  ~SwitchMasterInstanceStatement() override;

  bool full = false;
  char* instance_name;
  TenantName* tenant_name;
};

}  // namespace hsql
