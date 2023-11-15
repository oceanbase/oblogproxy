#pragma once
#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

struct ShowSlaveStatusStatement : SQLStatement {
  ShowSlaveStatusStatement();
  ~ShowSlaveStatusStatement() override;
};

}  // namespace hsql
