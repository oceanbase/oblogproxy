#pragma once
#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

enum ShowType { kShowColumns, kShowTables, kShowVar };

// Represents SQL SHOW statements.
// Example "SHOW TABLES;"
struct ShowStatement : SQLStatement {
  ShowStatement(ShowType type);
  ~ShowStatement() override;

  ShowType type;
  char* schema;
  char* name;
  char* _var_name;
  char* _var_value;
  VarLevel var_type = Global;
};

}  // namespace hsql
