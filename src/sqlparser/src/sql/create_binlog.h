#pragma once
#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

enum BinlogOptionType {
  CLUSTER_URL,
  SERVER_UUID,
  INITIAL_TRX_GTID_SEQ,
  INITIAL_TRX_XID,
  INITIAL_TRX_SEEKING_ABORT_TIMESTAMP
};

struct BinlogOption {
  BinlogOption(BinlogOptionType option_type, char* value);

  virtual ~BinlogOption();
  BinlogOptionType option_type;
  char* value;
};

struct CreateBinlogStatement : SQLStatement {
  CreateBinlogStatement();
  ~CreateBinlogStatement() override;

  TenantName* tenant;
  bool is_for_not_exists;
  Expr* ts;
  std::vector<BinlogOption*>* binlog_options;

  /*!
   * @brief   Create Binlog service and specify sys account password
   */

  UserInfo* user_info;
};

}  // namespace hsql
