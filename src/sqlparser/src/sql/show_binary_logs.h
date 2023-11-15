#ifndef SQLPARSER_SET_STATEMENT_H
#define SQLPARSER_SET_STATEMENT_H

#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

// Represents SQL SET statements.
struct ShowBinaryLogsStatement : SQLStatement {
  ShowBinaryLogsStatement();
  ~ShowBinaryLogsStatement() override;
};

}  // namespace hsql
#endif
