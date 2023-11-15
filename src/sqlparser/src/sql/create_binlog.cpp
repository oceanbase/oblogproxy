#include "create_binlog.h"
hsql::CreateBinlogStatement::CreateBinlogStatement() : SQLStatement(COM_CREATE_BINLOG), ts(nullptr), tenant(nullptr)
{}
hsql::CreateBinlogStatement::~CreateBinlogStatement()
{
  if (ts) {
    delete ts;
  }

  if (tenant) {
    delete tenant;
  }

  if (user_info) {
    delete user_info;
  }

  if (binlog_options) {
    for (BinlogOption* option : *binlog_options) {
      delete option;
    }
    delete binlog_options;
  }
}

hsql::BinlogOption::BinlogOption(BinlogOptionType option_type, char* value) : option_type(option_type), value(value)
{}

hsql::BinlogOption::~BinlogOption()
{
  if (value) {
    delete value;
  }
}
