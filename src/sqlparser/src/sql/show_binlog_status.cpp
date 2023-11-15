#include "show_binlog_status.h"
namespace hsql {

ShowBinlogStatusStatement::ShowBinlogStatusStatement() : SQLStatement(COM_SHOW_BINLOG_STAT), tenant(nullptr)
{}

ShowBinlogStatusStatement::~ShowBinlogStatusStatement()
{
  if (tenant) {
    delete tenant;
  }
}
}  // namespace hsql