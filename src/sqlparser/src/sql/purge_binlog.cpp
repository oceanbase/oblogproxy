#include "purge_binlog.h"
// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

PurgeBinlogStatement::PurgeBinlogStatement()
    : SQLStatement(COM_PURGE_BINLOG), tenant(nullptr), binlog_file(nullptr), purge_ts(nullptr)
{}

PurgeBinlogStatement::~PurgeBinlogStatement()
{
  if (tenant) {
    delete tenant;
  }
  if (binlog_file) {
    delete binlog_file;
  }

  if (purge_ts) {
    delete purge_ts;
  }
}
}  // namespace hsql