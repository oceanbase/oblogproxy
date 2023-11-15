#pragma once
#include "SQLStatement.h"
#include "Table.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

struct ShowBinlogStatusStatement : SQLStatement {
  ShowBinlogStatusStatement();
  ~ShowBinlogStatusStatement() override;
  bool for_tenant_exists;
  TenantName* tenant;
};

}  // namespace hsql
