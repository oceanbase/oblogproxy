#include "show_slave_status.h"
namespace hsql {
ShowSlaveStatusStatement::ShowSlaveStatusStatement() : SQLStatement(COM_SHOW_SLAVE_STAT)
{}

ShowSlaveStatusStatement::~ShowSlaveStatusStatement()
{}
}  // namespace hsql