#include "show_binlog_server.h"
namespace hsql {

ShowBinlogServerStatement::ShowBinlogServerStatement() : SQLStatement(COM_SHOW_BINLOG_SERVER), tenant(nullptr)
{}

ShowBinlogServerStatement::~ShowBinlogServerStatement()
{
  delete tenant;
}
}  // namespace hsql