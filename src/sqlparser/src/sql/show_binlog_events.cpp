#include "show_binlog_events.h"
namespace hsql {

ShowBinlogEventsStatement::ShowBinlogEventsStatement()
    : SQLStatement(COM_SHOW_BINLOG_EVENTS), limit(nullptr), start_pos(nullptr), binlog_file(nullptr)
{}

ShowBinlogEventsStatement::~ShowBinlogEventsStatement()
{
  delete limit;
  if (start_pos) {
    delete start_pos;
  }

  if (binlog_file) {
    delete binlog_file;
  }
}
}  // namespace hsql