#include "show_master_status.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {
ShowMasterStatusStatement::ShowMasterStatusStatement() : SQLStatement(COM_SHOW_MASTER_STAT)
{}

ShowMasterStatusStatement::~ShowMasterStatusStatement()
{}
}  // namespace hsql
