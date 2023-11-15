#pragma once

#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

struct ShowBinlogEventsStatement : SQLStatement {
  ShowBinlogEventsStatement();
  ~ShowBinlogEventsStatement() override;

  Expr* binlog_file;
  Expr* start_pos;
  LimitDescription* limit;
};

}  // namespace hsql
