#pragma once
#include <vector>

#include "Expr.h"

namespace hsql {
enum StatementType {
  COM_ERROR,  // unused
  COM_SELECT,
  COM_IMPORT,
  COM_INSERT,
  COM_UPDATE,
  COM_DELETE,
  COM_CREATE,
  COM_CREATE_BINLOG,
  COM_DROP,
  COM_DROP_BINLOG,
  COM_PREPARE,
  COM_EXECUTE,
  COM_EXPORT,
  COM_RENAME,
  COM_ALTER,
  COM_SHOW,
  COM_SHOW_VARIABLES,
  COM_SHOW_BINLOGS,
  COM_SHOW_BINLOG_EVENTS,
  COM_SHOW_MASTER_STAT,
  COM_SHOW_SLAVE_STAT,
  COM_SHOW_BINLOG_SERVER,
  COM_SHOW_BINLOG_STAT,
  COM_TRANSACTION,
  COM_SET,
  COM_PURGE_BINLOG
};

// Description of the limit clause within a select statement.
struct LimitDescription {
  LimitDescription(Expr* limit, Expr* offset);
  virtual ~LimitDescription();

  Expr* limit;
  Expr* offset;
};

struct TenantName {
  TenantName(char* cluster, char* tenant);
  virtual ~TenantName();
  char* cluster;
  char* tenant;
};

struct UserInfo {
  UserInfo(char* user, char* password);
  virtual ~UserInfo();
  char* user;
  char* password;
};


// Base struct for every SQL statement
struct SQLStatement {
  SQLStatement(StatementType type);

  virtual ~SQLStatement();

  StatementType type() const;

  bool isType(StatementType type) const;

  // Shorthand for isType(type).
  bool is(StatementType type) const;

  // Length of the string in the SQL query string
  size_t stringLength;

  std::vector<Expr*>* hints;

private:
  StatementType type_;
};

}  // namespace hsql
