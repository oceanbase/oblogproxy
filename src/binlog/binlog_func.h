/**
 * Copyright (c) 2023 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#pragma once
#include <map>
#include "ob_log_event.h"
#include "connection.h"
#include "sql/SQLStatement.h"
#include "sql/SelectStatement.h"
namespace oceanbase {
namespace logproxy {

using IoResult = binlog::Connection::IoResult;
enum class Function : uint8_t {
  gtid_subset,
  gtid_subtract,
  wait_for_executed_gtid_set,
  /* Must be last */
  end,
};

static std::map<std::string, Function> _s_function_mapping = {
    {"GTID_SUBSET", Function::gtid_subset},
    {"GTID_SUBTRACT", Function::gtid_subtract},
    {"WAIT_FOR_EXECUTED_GTID_SET", Function::wait_for_executed_gtid_set},
};

inline Function get_function(const std::string& func_name)
{
  if (_s_function_mapping.find(func_name) != _s_function_mapping.end()) {
    return _s_function_mapping[func_name];
  }
  return Function::end;
};

class FuncProcessor {
public:
  explicit FuncProcessor(Function func) : _func(func)
  {}
  virtual IoResult process(binlog::Connection* conn, hsql::SQLStatement* statement) = 0;

  virtual std::string function_def(hsql::SQLStatement* statement);

protected:
  const Function _func;
};

class GtidSubsetFuncProcessor : public FuncProcessor {
public:
  explicit GtidSubsetFuncProcessor() : FuncProcessor(Function::gtid_subset)
  {}

  IoResult process(binlog::Connection* conn, hsql::SQLStatement* statement) override;
};

class GtidSubtractFuncProcessor : public FuncProcessor {
public:
  explicit GtidSubtractFuncProcessor() : FuncProcessor(Function::gtid_subtract)
  {}

  IoResult process(binlog::Connection* conn, hsql::SQLStatement* statement) override;
};

FuncProcessor* func_processor(std::string& func_name);

class BinlogFunction {
public:
  static int gtid_subset(const std::string& set_sub, const std::string& set_target);

  static int deserialization(const std::string& gtid_str, std::map<std::string, GtidMessage*>& gtid_message);

  static int merge_txn_range(GtidMessage* message, GtidMessage* target);

  /*!
   * @brief  Determine whether the transaction set subset corresponding
   * to the same uuid is a subset of target
   * @param subset
   * @param target
   * @return
   */
  static bool is_subset(const std::vector<txn_range>& subset, const std::vector<txn_range>& target);

  /*!
   * @brief Get the difference between set_target and set_origin
   * @param set_origin
   * @param set_target
   * @param result return difference
   * @return Whether the calculation was successful
   */
  static int gtid_subtract(const std::string& set_origin, const std::string& set_target, std::string& result);

  /*!
   * @brief Obtain the gtid difference corresponding to the unified instance ID
   * @param origin
   * @param target
   * @param result
   * @return
   */
  static int difference(
      std::vector<txn_range> origin, std::vector<txn_range> target, std::vector<txn_range>& result);
};
}  // namespace logproxy
}  // namespace oceanbase