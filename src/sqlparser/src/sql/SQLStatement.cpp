
#include "SQLStatement.h"

namespace hsql {

// SQLStatement
SQLStatement::SQLStatement(StatementType type) : hints(nullptr), type_(type)
{}

SQLStatement::~SQLStatement()
{
  if (hints) {
    for (Expr* hint : *hints) {
      delete hint;
    }
  }
  delete hints;
}

StatementType SQLStatement::type() const
{
  return type_;
}

bool SQLStatement::isType(StatementType type) const
{
  return (type_ == type);
}

bool SQLStatement::is(StatementType type) const
{
  return isType(type);
}

TenantName::TenantName(char* cluster, char* tenant) : cluster(cluster), tenant(tenant)
{}

TenantName::~TenantName()
{
  if (cluster != nullptr) {
    delete cluster;
  }

  if (tenant != nullptr) {
    delete tenant;
  }
}

UserInfo::UserInfo(char* user, char* password) : user(user), password(password)
{}

UserInfo::~UserInfo()
{
  if (user != nullptr) {
    delete user;
  }

  if (password != nullptr) {
    delete password;
  }
}

}  // namespace hsql
