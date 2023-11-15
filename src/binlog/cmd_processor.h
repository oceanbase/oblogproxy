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

#include "connection.h"

namespace oceanbase {
namespace binlog {
using IoResult = Connection::IoResult;

class CmdProcessor {
public:
  explicit CmdProcessor(ServerCommand cmd) : cmd_(cmd)
  {}

  virtual IoResult process(Connection* conn, PacketBuf& payload) = 0;

protected:
  const ServerCommand cmd_;
};

class UnsupportedCmdProcessor : public CmdProcessor {
public:
  explicit UnsupportedCmdProcessor(ServerCommand cmd) : CmdProcessor(cmd)
  {}

  IoResult process(Connection* conn, PacketBuf& payload) override
  {
    OMS_STREAM_WARN << "Received unsupported command " << static_cast<uint8_t>(cmd_) << "("
                    << server_command_names(cmd_) << ") on connection " << conn->trace_id();
    return IoResult::FAIL;
  }
};

class QueryCmdProcessor : public CmdProcessor {
public:
  QueryCmdProcessor() : CmdProcessor(ServerCommand::query)
  {}

  IoResult process(Connection* conn, PacketBuf& payload) override;
};

class BinlogDumpCmdProcessor : public CmdProcessor {
public:
  BinlogDumpCmdProcessor() : CmdProcessor(ServerCommand::binlog_dump)
  {}

  IoResult process(Connection* conn, PacketBuf& payload) override;
};

class BinlogDumpGtidCmdProcessor : public CmdProcessor {
public:
  BinlogDumpGtidCmdProcessor() : CmdProcessor(ServerCommand::binlog_dump_gtid)
  {}

  IoResult process(Connection* conn, PacketBuf& payload) override;
};

class RegisterSlaveCmdProcessor : public CmdProcessor {
public:
  RegisterSlaveCmdProcessor() : CmdProcessor(ServerCommand::register_slave)
  {}

  IoResult process(Connection* conn, PacketBuf& payload) override;
};

class QuitCmdProcessor : public CmdProcessor {
public:
  QuitCmdProcessor() : CmdProcessor(ServerCommand::quit)
  {}

  IoResult process(Connection* conn, PacketBuf& payload) override;
};

CmdProcessor* cmd_processor(ServerCommand server_command);

}  // namespace binlog
}  // namespace oceanbase
