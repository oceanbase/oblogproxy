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
#include <string>

#include "common/define.h"
namespace etransfer {
namespace object {
using namespace common;
class Option {
 public:
  Option(OptionType option_type) : option_type_(option_type) {}
  virtual OptionType GetOptionType() { return option_type_; }

 private:
  OptionType option_type_;
};

class CommentOption : public Option {
 public:
  CommentOption(std::string comment)
      : Option(OptionType::COMMENT_OPTION), comment_(comment) {}

  std::string GetComment() { return comment_; }

 private:
  std::string comment_;
};

class TableCharsetOption : public Option {
 public:
  TableCharsetOption(const std::string& charset)
      : Option(OptionType::TABLE_CHARSET), charset(charset) {}

  std::string charset;
};

class TableCollationOption : public Option {
 public:
  std::string collation;

  TableCollationOption(const std::string& collation)
      : Option(OptionType::TABLE_COLLATION), collation(collation) {}
};

};  // namespace object
};  // namespace etransfer