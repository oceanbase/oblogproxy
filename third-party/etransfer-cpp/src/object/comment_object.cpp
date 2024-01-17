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

#include "object/comment_object.h"
namespace etransfer {
namespace object {
bool CommentObject::IsCommentForObject(const std::string& object_name) {
  if (comment_pair_.comment_target.value == object_name) {
    return true;
  }
  return false;
}
CommentObject::CommentObject(const Catalog& catalog,
                             const RawConstant& object_name,
                             const std::string& raw_ddl,
                             CommentTargetType comment_target_type,
                             const CommentPair& comment_pair)
    : Object(catalog, object_name, raw_ddl, ObjectType::COMMENT_OBJECT),
      comment_target_type_(comment_target_type),
      comment_pair_(comment_pair) {}

}  // namespace object

}  // namespace etransfer
