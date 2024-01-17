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
#include "object/object.h"
namespace etransfer {
namespace object {
class CommentObject : public Object {
 public:
  enum CommentTargetType { SCHEMA, TABLE, COLUMN, CONSTRAINT, UNKNOWN };

  struct CommentPair {
    RawConstant comment_target;
    std::string comment;
    CommentPair(const RawConstant& comment_target, const std::string& comment)
        : comment_target(comment_target), comment(comment) {}
  };

  CommentObject(const Catalog& catalog, const RawConstant& object_name,
                const std::string& raw_ddl,
                CommentTargetType comment_target_type,
                const CommentPair& comment_pair);

  CommentTargetType GetCommentTargetType() { return comment_target_type_; }

  CommentPair GetCommentPair() { return comment_pair_; }

  bool IsTableComment() {
    return comment_target_type_ == CommentObject::CommentTargetType::TABLE;
  }

  bool IsColumnComment() {
    return comment_target_type_ == CommentObject::CommentTargetType::COLUMN;
  }

  bool IsIndexComment() {
    return comment_target_type_ == CommentObject::CommentTargetType::CONSTRAINT;
  }

  bool IsCommentForObject(const std::string& object_name);

 private:
  CommentTargetType comment_target_type_;
  CommentPair comment_pair_;
};

}  // namespace object

}  // namespace etransfer
