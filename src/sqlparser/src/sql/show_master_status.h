#pragma once
#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

struct ShowMasterStatusStatement : SQLStatement {
  ShowMasterStatusStatement();
  ~ShowMasterStatusStatement() override;
};

}  // namespace hsql
