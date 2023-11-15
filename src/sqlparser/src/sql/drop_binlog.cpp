#include "drop_binlog.h"
namespace hsql {
DropBinlogStatement::DropBinlogStatement() : SQLStatement(COM_DROP_BINLOG), tenant_info(nullptr)
{}

DropBinlogStatement::~DropBinlogStatement()
{
  if (tenant_info) {
    delete tenant_info;
  }
}
}  // namespace hsql
