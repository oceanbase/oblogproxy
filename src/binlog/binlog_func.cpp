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

#include <vector>
#include "binlog_func.h"
#include "str.h"
#include "guard.hpp"
#include "mysql_errno.h"
namespace oceanbase {
namespace logproxy {

static GtidSubsetFuncProcessor _func_gtid_subset_processor;
static GtidSubtractFuncProcessor _func_gtid_subtract_processor;

static std::unordered_map<Function, FuncProcessor*> _func_supported_processors = {
    {Function::gtid_subset, &_func_gtid_subset_processor},
    {Function::gtid_subtract, &_func_gtid_subtract_processor},
};

void free_gtid_msg(std::map<std::string, GtidMessage*>& m)
{
  for (auto iter = m.begin(); iter != m.end(); ++iter) {
    if (iter->second != nullptr) {
      delete iter->second;
      iter->second = nullptr;
    }
  }
  m.clear();
}

int BinlogFunction::gtid_subset(const std::string& set_sub, const std::string& set_target)
{
  // Parse the corresponding gtid collection
  std::map<std::string, GtidMessage*> sub_msg;
  ResourceGuard<std::map<std::string, GtidMessage*>> sub_guard(sub_msg, free_gtid_msg);
  deserialization(set_sub, sub_msg);

  std::map<std::string, GtidMessage*> target_msg;
  ResourceGuard<std::map<std::string, GtidMessage*>> target_guard(target_msg, free_gtid_msg);
  deserialization(set_target, target_msg);

  for (auto sub : sub_msg) {
    if (target_msg.find(sub.first) == target_msg.end()) {
      return 0;
    }
    auto* target = target_msg.find(sub.first)->second;
    if (!is_subset(sub.second->get_txn_range(), target->get_txn_range())) {
      return 0;
    }
  }
  return 1;
}

int BinlogFunction::deserialization(const std::string& gtid_str, std::map<std::string, GtidMessage*>& gtid_message)
{
  std::vector<std::string> gtid_sets;
  split(gtid_str, ',', gtid_sets);
  for (const auto& gtid_set : gtid_sets) {
    auto* message = new GtidMessage();
    message->deserialize_gtid_set(gtid_set);
    auto iter = gtid_message.find(message->get_gtid_uuid_str());
    if (gtid_message.find(message->get_gtid_uuid_str()) != gtid_message.end()) {
      merge_txn_range(message, iter->second);
      delete message;
    } else {
      gtid_message[message->get_gtid_uuid_str()] = message;
    }
  }
  return OMS_OK;
}

int BinlogFunction::merge_txn_range(GtidMessage* message, GtidMessage* target)
{
  uint64_t start = 0, end = 0;
  std::vector<txn_range> merge;
  std::vector<txn_range> full_range;
  full_range.insert(full_range.end(), message->get_txn_range().begin(), message->get_txn_range().end());
  full_range.insert(full_range.end(), target->get_txn_range().begin(), target->get_txn_range().end());
  for (auto const& range : full_range) {
    if (start == 0) {
      start = range.first;
      end = range.second;
    } else {
      if (range.first <= end + 1) {
        end = std::max(end, range.second);
      } else {
        merge.emplace_back(start, end);
        start = range.first;
        end = range.second;
      }
    }
  }
  if (start != 0) {
    merge.emplace_back(start, end);
  }
  target->set_txn_range(merge);
  return OMS_OK;
}

bool BinlogFunction::is_subset(const std::vector<txn_range>& subset, const std::vector<txn_range>& target)
{
  for (auto sub : subset) {
    bool found = false;
    for (auto tar : target) {
      if (sub.first >= tar.first && sub.second <= tar.second) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

int BinlogFunction::gtid_subtract(const string& set_origin, const string& set_target, string& result)
{
  // Parse the corresponding gtid collection
  std::map<std::string, GtidMessage*> origin_msg;
  ResourceGuard<std::map<std::string, GtidMessage*>> origin_guard(origin_msg, free_gtid_msg);
  deserialization(set_origin, origin_msg);

  std::map<std::string, GtidMessage*> target_msg;
  ResourceGuard<std::map<std::string, GtidMessage*>> target_guard(target_msg, free_gtid_msg);
  deserialization(set_target, target_msg);

  std::map<std::string, GtidMessage*> result_msg;
  ResourceGuard<std::map<std::string, GtidMessage*>> result_guard(result_msg, free_gtid_msg);

  for (auto origin : origin_msg) {
    if (target_msg.find(origin.first) == target_msg.end()) {
      GtidMessage* tmp_msg = new GtidMessage();
      auto* uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
      memset(uuid, 0, SERVER_UUID_LEN);
      memcpy(uuid, origin.second->get_gtid_uuid(), SERVER_UUID_LEN);
      tmp_msg->set_gtid_uuid(uuid);
      tmp_msg->set_txn_range(origin.second->get_txn_range());
      result_msg[origin.first] = tmp_msg;
      continue;
    }
    auto* target = target_msg.find(origin.first)->second;
    std::vector<txn_range> diff;
    difference(origin.second->get_txn_range(), target->get_txn_range(), diff);
    if (!diff.empty()) {
      std::sort(diff.begin(), diff.end());
      GtidMessage* tmp_msg = new GtidMessage();
      auto* uuid = static_cast<unsigned char*>(malloc(SERVER_UUID_LEN));
      memset(uuid, 0, SERVER_UUID_LEN);
      memcpy(uuid, origin.second->get_gtid_uuid(), SERVER_UUID_LEN);
      tmp_msg->set_gtid_uuid(uuid);
      tmp_msg->set_txn_range(diff);
      result_msg[origin.first] = tmp_msg;
    }
  }

  // Serialize gtid array to string
  int index = 0;
  for (const auto& gtid_msg : result_msg) {
    if (index > 0) {
      result.append(",");
    }
    result.append(gtid_msg.second->format_string());
    index++;
  }

  return OMS_OK;
}

int BinlogFunction::difference(
    std::vector<txn_range> origin, std::vector<txn_range> target, std::vector<txn_range>& result)
{
  // Sort origin and target according to the starting number from small to large
  std::sort(origin.begin(), origin.end());
  std::sort(target.begin(), target.end());

  for (const auto& range_target : target) {
    for (size_t i = 0; i < origin.size(); ++i) {
      OMS_INFO("target range [{},{}],origin range [{},{}]",
          range_target.first,
          range_target.second,
          origin[i].first,
          origin[i].second);
      if (range_target.second < origin[i].first) {
        break;
      } else if (range_target.first > origin[i].second) {
        continue;
      } else {
        // 1. When range_target is completely within the range of origin, all range_targets need to be dug out at this
        // time
        if (range_target.second < origin[i].second && range_target.first > origin[i].first) {
          uint64_t first = origin[i].first;
          origin[i].first = range_target.second + 1;
          origin.emplace_back(first, range_target.first - 1);
          break;
        }

        if (range_target.second >= origin[i].second) {
          if (range_target.first <= origin[i].first) {
            // delete all
            origin[i].first = 0;
            origin[i].second = 0;
            continue;
          }

          origin[i].second = range_target.first + 1;
          continue;
        }

        if (origin[i].first >= range_target.first) {
          origin[i].first = range_target.second + 1;
          break;
        }
        uint64_t end = origin[i].second;
        origin[i].second = range_target.first - 1;
        origin.emplace_back(range_target.second + 1, end);
        break;
      }
    }
  }

  std::copy_if(origin.begin(), origin.end(), std::back_inserter(result), [](txn_range range) {
    return range.first != 0 && range.second != 0;
  });

  for (const auto& range : result) {
    OMS_INFO("[{},{}]", range.first, range.second);
  }
  return OMS_OK;
}

IoResult GtidSubsetFuncProcessor::process(binlog::Connection* conn, hsql::SQLStatement* statement)
{
  OMS_INFO("Execute the gtid_subset function");
  auto* p_statement = (hsql::SelectStatement*)statement;
  int ret = 0;
  if (p_statement->selectList->at(0)->exprList->size() == 2) {
    auto gtid_sub = p_statement->selectList->at(0)->exprList->at(0)->getName();
    auto gtid_target = p_statement->selectList->at(0)->exprList->at(1)->getName();
    ret = logproxy::BinlogFunction::gtid_subset(gtid_sub, gtid_target);
  } else {
    OMS_ERROR("Incorrect parameter count in the call to native function gtid_subset");
    return conn->send_err_packet(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT,
        "Incorrect parameter count in the call to native function gtid_subset",
        "42000");
  }

  // row packet
  conn->start_row();
  conn->store_string(std::to_string(ret));
  if (conn->send_row() != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }
  return conn->send_eof_packet();
}

IoResult GtidSubtractFuncProcessor::process(binlog::Connection* conn, hsql::SQLStatement* statement)
{
  OMS_INFO("Execute the gtid_subtract function");
  auto* p_statement = (hsql::SelectStatement*)statement;
  std::string result;
  if (p_statement->selectList->at(0)->exprList->size() == 2) {
    auto gtid_sub = p_statement->selectList->at(0)->exprList->at(0)->getName();
    auto gtid_target = p_statement->selectList->at(0)->exprList->at(1)->getName();
    logproxy::BinlogFunction::gtid_subtract(gtid_sub, gtid_target, result);
  } else {
    OMS_ERROR("Incorrect parameter count in the call to native function gtid_subtract");
    return conn->send_err_packet(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT,
        "Incorrect parameter count in the call to native function gtid_subtract",
        "42000");
  }

  conn->start_row();
  conn->store_string(result);
  if (conn->send_row() != IoResult::SUCCESS) {
    return IoResult::FAIL;
  }
  return conn->send_eof_packet();
}

FuncProcessor* func_processor(string& func_name)
{
  Function func = get_function(func_name);
  auto iter = _func_supported_processors.find(func);
  if (iter != _func_supported_processors.end()) {
    return iter->second;
  }
  return nullptr;
}

std::string FuncProcessor::function_def(hsql::SQLStatement* statement)
{
  auto* p_statement = (hsql::SelectStatement*)statement;
  std::string result = "";
  if (p_statement->selectList->at(0)->exprList->size() == 2) {
    auto gtid_sub = p_statement->selectList->at(0)->exprList->at(0)->getName();
    auto gtid_target = p_statement->selectList->at(0)->exprList->at(1)->getName();
    result.append(p_statement->selectList->at(0)->getName())
        .append("('")
        .append(gtid_sub)
        .append("','")
        .append(gtid_target)
        .append("')");
  }
  return result;
}

}  // namespace logproxy
}  // namespace oceanbase