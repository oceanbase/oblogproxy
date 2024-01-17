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

#include "object/column_attributes.h"
namespace etransfer {
namespace object {

std::string ColumnAttributes::GetGenStoreName(GenStoreType type) {
  switch (type) {
    case GenStoreType::STORED:
      return "STORED";
    case GenStoreType::VIRTUAL:
      return "VIRTUAL";
    default:
      return "";
  }
}
void ColumnAttributes::SetDefaultValue(
    std::shared_ptr<ExprToken> default_value) {
  this->default_value_ = default_value;
  set_bits_ |= DEFAULT_VALUE_BIT;
}

void ColumnAttributes::SetComment(const std::string& comment) {
  this->comment_ = comment;
  set_bits_ |= COMMENT_BIT;
}
void ColumnAttributes::SetBinary(bool is_binary) {
  this->is_binary_ = is_binary;
  set_bits_ |= BINARY_BIT;
}

void ColumnAttributes::SetOnUpdate(const std::string& on_update_func) {
  this->on_update_func_ = on_update_func;
  set_bits_ |= ON_UPDATE_BIT;
}

void ColumnAttributes::SetAnonymousKeyType(IndexType index_type) {
  this->anonymous_key_type_ = index_type;
  set_bits_ |= ANONYMOUS_KEY_TYPE_BIT;
}
void ColumnAttributes::SetSpatialRefId(const std::string& spatial_ref_id) {
  set_bits_ |= SRID_BIT;
  this->spatial_ref_id_ = spatial_ref_id;
}

void ColumnAttributes::SetIsNullable(bool is_nullable) {
  this->is_nullable_ = is_nullable;
  set_bits_ |= IS_NULL_BIT;
}

void ColumnAttributes::SetGenerated(std::shared_ptr<ExprToken> exp_gen,
                                    GenStoreType gen_store) {
  this->set_bits_ |= GENERATED_BIT;
  this->exp_gen_ = exp_gen;
  this->gen_store_ = gen_store;
}

}  // namespace object
}  // namespace etransfer
