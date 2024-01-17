/**
 * Copyright (c) 2024 OceanBase
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
#include <unordered_map>
namespace etransfer {
namespace common {
enum class FunctionType {
  ASCII,
  LOWER,
  UPPER,
  CAST,
  ABS,
  SUBSTR,
  SUBSTRING,
  SUBSTRING_INDEX,
  REPLACE,
  TRIM,
  LTRIM,
  RTRIM,
  LPAD,
  RPAD,
  NOW,
  LEFT,
  REPEAT,
  REVERSE,
  RIGHT,
  ADDDATE,
  DATE_ADD,
  ADDTIME,
  SUBDATE,
  DATE_SUB,
  SUBTIME,
  DATE,
  DATEDIFF,
  TIMESTAMPDIFF,
  TIMESTAMPADD,
  TIMEDIFF,
  DAY,
  DAYOFMONTH,
  DAYOFWEEK,
  DAYOFYEAR,
  MONTHNAME,
  MAKETIME,
  MAKEDATE,
  TIME,
  WEEK,
  YEAR,
  HOUR,
  MINUTE,
  MONTH,
  QUARTER,
  SECOND,
  INTERVAL,
  LOCALTIME,
  MICROSECOND,
  CHAR_LENGTH,
  CHARACTER_LENGTH,
  CHARACTER,
  CONCAT_WS,
  FIELD,
  FIND_IN_SET,
  FORMAT,
  INSERT,
  LOCATE,
  LCASE,
  POSITION,
  SPACE,
  STRCMP,
  UCASE,
  SEC_TO_TIME,
  COUNT,
  MAX,
  MIN,
  PERIOD_DIFF,
  TIME_TO_SEC,
  TO_DAYS,
  WEEKDAY,
  WEEKOFYEAR,
  CONCAT,
  EXTRACT,
  MOD,
  CURRENT_TIMESTAMP,
  CURRENT_TIME,
  CURTIME,
  TIMESTAMP,
  SYSDATE,
  LOCALTIMESTAMP,
  CURRENT_DATE,
  CURDATE,
  UTC_TIMESTAMP,
  SQRT,
  // build json array
  JSON_ARRAY,
  // build json object
  JSON_OBJECT,
  // enclose json_val with "
  JSON_QUOTE,
  JSON_UNQUOTE,
  JSON_EXTRACT,
  // Get all the key values of the json document under the specified path
  JSON_KEYS,
  JSON_SEARCH,
  JSON_APPEND,
  JSON_ARRAY_APPEND,
  JSON_MERGE,
  JSON_REMOVE,
  JSON_DEPTH,
  JSON_LENGTH,
  JSON_REPLACE,
  JSON_SET,
  JSON_MERGE_PRESERVE,
  JSON_MERGE_PATCH,

  AVG,
  SUM,

  // use to concat params with space in build params of function, this function
  // isn't defined in any database.
  CONCAT_PARAMS_WITH_SPACE,
  // use to concat params with dot in build params of function, this function
  // isn't defined in any database.
  CONCAT_PARAMS_WITH_DOT,
  // BASE FUNCTION
  UNKNOWN
};

class FunctionTypeUtil {
 public:
  const static std::unordered_map<std::string, FunctionType>
      string_to_functiontype;
  static const char* const function_type_names[];
  static FunctionType GetFunctionType(const std::string& function_name);
  static const char* GetFunctionName(FunctionType function_type);
};
}  // namespace common

}  // namespace etransfer
