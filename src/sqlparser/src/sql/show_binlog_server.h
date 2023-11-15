#pragma once
#include "SQLStatement.h"
// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

struct ShowBinlogServerStatement : SQLStatement {
  ShowBinlogServerStatement();
  ~ShowBinlogServerStatement() override;
  TenantName* tenant;
};

}  // namespace hsql
