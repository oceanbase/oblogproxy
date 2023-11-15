#include "set_statement.h"
// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

SetStatement::SetStatement() : SQLStatement(COM_SET), sets(nullptr)
{}

SetStatement::~SetStatement()
{
  if (sets) {
    for (SetClause* set : *sets) {
      free(set->column);
      delete set->value;
      delete set;
    }
    delete sets;
  }
}
}  // namespace hsql
