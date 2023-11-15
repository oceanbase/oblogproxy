
#include "PrepareStatement.h"

namespace hsql {
// PrepareStatement
PrepareStatement::PrepareStatement() : SQLStatement(COM_PREPARE), name(nullptr), query(nullptr) {}

PrepareStatement::~PrepareStatement() {
  free(name);
  free(query);
}
}  // namespace hsql
