/**
 * Copyright (c) 2023 OceanBase
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

#include <regex>

#include "sql_cmd.h"
#include "common.h"
#include "connection.h"
#include "binlog_state_machine.h"
#include "sql/SQLStatement.h"
#include "binlog_func.h"
#include "sql/show_statement.h"
#include "log.h"

namespace oceanbase {
namespace binlog {
using IoResult = Connection::IoResult;

class SqlCmdProcessor {
public:
  virtual IoResult process(Connection* conn, const hsql::SQLStatement* statement) = 0;
};

class ShowBinaryLogsProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinaryLogsProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowBinlogEventsProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinlogEventsProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowMasterStatusProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowMasterStatusProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class PurgeBinaryLogsProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(PurgeBinaryLogsProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowBinlogServerProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinlogServerProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class CreateBinlogProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(CreateBinlogProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
  static void init_oblog_config(logproxy::OblogConfig& config);
};

class DropBinlogProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(DropBinlogProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowBinlogStatusProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinlogStatusProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
  static void serialize_binlog_metrics(string& status, const oceanbase::binlog::StateMachine* state_machine);
};

class SetVarProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(SetVarProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowVarProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowVarProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

  static int query_var(
      Connection* conn, std::string& var_name, std::string& value, hsql::VarLevel type, std::string& err_msg);

  /*!
   * @brief Query the value of the specified variable
   * @param conn
   * @param p_statement
   * @return
   */
  static IoResult show_specified_var(Connection* conn, const hsql::ShowStatement* p_statement);

  /*!
   * @brief Query non-specified variable query, return all variable values without limit
   * @param conn
   * @param p_statement
   * @return
   */
  static IoResult show_non_specified_var(Connection* conn, const hsql::ShowStatement* p_statement);
};

class SelectProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(SelectProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

  /*!
   * @brief Handle requests for variable queries, such as select @@system_variable_name;
   * @param conn
   * @param p_statement
   * @return
   */
  static IoResult handle_query_var(Connection* conn, const hsql::SelectStatement* p_statement);

  /*!
   * @brief The processing function executes the result query, such as SELECT
   * GTID_SUBTRACT('4849646f-4dc7-11ed-85a9-7cd30abc99b4:1-9', '4849646f-4dc7-11ed-85a9-7cd30abc99b4:1-9');
   * @param conn
   * @param p_statement
   * @return
   */
  static IoResult handle_function(Connection* conn, hsql::SelectStatement* p_statement);
};

SqlCmdProcessor* sql_cmd_processor(hsql::StatementType type);

}  // namespace binlog
}  // namespace oceanbase