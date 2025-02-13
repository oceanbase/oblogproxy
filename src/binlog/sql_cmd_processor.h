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

#include <regex>
#include <utility>

#include "sql_cmd.h"
#include "common.h"
#include "connection.h"
#include "binlog_state_machine.h"
#include "sql/statements.h"
#include "sql/SQLStatement.h"
#include "sql/set_statement.h"
#include "binlog-instance/binlog_func.h"
#include "log.h"
#include "task.h"

namespace oceanbase::binlog {

using IoResult = Connection::IoResult;

class DataPacketHolder {
public:
  DataPacketHolder() = default;

  void err_packet(uint16_t err_code, std::string err_msg, const std::string& sql_state)
  {
    _is_ok_packet = false;
    _err_packet = ErrPacket(err_code, std::move(err_msg), sql_state);
  }

  void ok_packet(uint16_t warnings, const std::string& info)
  {
    _is_ok_packet = true;
    _ok_packet = OkPacket(0, 0, ob_binlog_server_status_flags, warnings, info);
  }

  void ok_packet()
  {
    _is_ok_packet = true;
    _ok_packet = OkPacket(0, 0, ob_binlog_server_status_flags, 0, "");
  }

  IoResult send(Connection* conn)
  {
    return _is_ok_packet ? conn->send(_ok_packet) : conn->send(_err_packet);
  }

private:
  bool _is_ok_packet = true;
  ErrPacket _err_packet;
  OkPacket _ok_packet;
};

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

class ShowSlaveStatusProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowSlaveStatusProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};


class PurgeBinaryLogsProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(PurgeBinaryLogsProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

private:
  static std::string build_purge_sql(const std::string& binlog_file, const std::string& purge_ts);

  static int purge_binlog_file(const std::vector<std::string>& file_paths);
};

class ShowBinlogServerProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinlogServerProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class BaseCreateProcessor : public SqlCmdProcessor {
protected:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

protected:
  virtual void obtain_options(const hsql::SQLStatement*, std::map<std::string, std::string>&, std::string&,
      std::string&, std::string&, uint16_t&) = 0;

  virtual bool can_create(const std::string&, const std::string&, const std::string&, const std::vector<BinlogEntry>&,
      DataPacketHolder&) = 0;

protected:
  virtual void pre_action(const std::string&, const std::string&){};
};

class CreateBinlogProcessor : public BaseCreateProcessor {
  OMS_SINGLETON(CreateBinlogProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override
  {
    return BaseCreateProcessor::process(conn, statement);
  }

private:
  void obtain_options(const hsql::SQLStatement*, std::map<std::string, std::string>&, std::string&, std::string&,
      std::string&, uint16_t&) override;

  bool can_create(const std::string&, const std::string&, const std::string&, const std::vector<BinlogEntry>&,
      DataPacketHolder&) override;

  void pre_action(const std::string&, const std::string&) override;
};

class CreateBinlogInstanceProcessor : public BaseCreateProcessor {
public:
  OMS_SINGLETON(CreateBinlogInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override
  {
    return BaseCreateProcessor::process(conn, statement);
  }

private:
  void obtain_options(const hsql::SQLStatement*, std::map<std::string, std::string>&, std::string&, std::string&,
      std::string&, uint16_t&) override;

  bool can_create(const std::string&, const std::string&, const std::string&, const std::vector<BinlogEntry>&,
      DataPacketHolder&) override;
};

class AlterBinlogProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(AlterBinlogProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class BaseDropProcessor : public SqlCmdProcessor {
protected:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

protected:
  virtual bool obtain_instances(
      const hsql::SQLStatement*, std::vector<BinlogEntry*>&, bool&, bool&, int32_t&, DataPacketHolder&) = 0;

  virtual void postAction(std::vector<BinlogEntry*>&){};

private:
  static void generate_drop_task(BinlogEntry&, bool, int32_t, std::vector<Task>&);
};

class DropBinlogProcessor : public BaseDropProcessor {
  OMS_SINGLETON(DropBinlogProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override
  {
    return BaseDropProcessor::process(conn, statement);
  }

protected:
  bool obtain_instances(
      const hsql::SQLStatement*, std::vector<BinlogEntry*>&, bool&, bool&, int32_t&, DataPacketHolder&) override;

  void postAction(std::vector<BinlogEntry*>&) override;
};

class DropBinlogInstanceProcessor : public BaseDropProcessor {
  OMS_SINGLETON(DropBinlogInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override
  {
    return BaseDropProcessor::process(conn, statement);
  }

protected:
  bool obtain_instances(
      const hsql::SQLStatement*, std::vector<BinlogEntry*>&, bool&, bool&, int32_t&, DataPacketHolder&) override;
};

class ShowBinlogStatusProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinlogStatusProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

private:
  static void serialize_binlog_metrics(std::string& status, const BinlogEntry& entry);

  static void collect_metric(const BinlogEntry& instance, ProcessMetric& metric);

  static void collect_binlog(const BinlogEntry& instance, MySQLResultSet& result_set);
};

class AlterBinlogInstanceProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(AlterBinlogInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

  static bool update_instance_options(
      BinlogEntry& instance, std::vector<hsql::SetClause*>& instance_options, std::string& update_res);
};

class StartBinlogInstanceProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(StartBinlogInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class StopBinlogInstanceProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(StopBinlogInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowBinlogInstanceProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowBinlogInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class SetVarProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(SetVarProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;

private:
  void sync_set_if_throttle_options(const std::string& col, const std::string& value);
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

  static std::string gen_show_var_sql(const hsql::ShowStatement* p_statement);
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

class ShowProcesslistProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowProcesslistProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowDumplistProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowDumplistProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class KillProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(KillProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ReportProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ReportProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class ShowNodesProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(ShowNodesProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class SwitchMasterInstanceProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(SwitchMasterInstanceProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

class SetPasswordProcessor : public SqlCmdProcessor {
  OMS_SINGLETON(SetPasswordProcessor);

public:
  IoResult process(Connection* conn, const hsql::SQLStatement* statement) override;
};

SqlCmdProcessor* sql_cmd_processor(Connection::ConnectionType conn_type, hsql::StatementType type);

}  // namespace oceanbase::binlog
