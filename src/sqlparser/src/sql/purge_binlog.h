#pragma once
#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

struct PurgeBinlogStatement : SQLStatement {
  PurgeBinlogStatement();
  ~PurgeBinlogStatement() override;

  Expr* binlog_file;
  Expr* purge_ts;
  bool for_tenant_exit;
  TenantName* tenant;
};

}  // namespace hsql
