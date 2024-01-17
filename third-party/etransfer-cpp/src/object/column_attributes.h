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
#include <memory>

#include "common/define.h"
#include "object/expr_token.h"
namespace etransfer {
namespace object {
using namespace common;

/**
 * mysql has two store type for generated column:
 *      VIRTUAL : It is automatically calculated when the row record is
 * accessed, and does not occupy storage space STORED : When the row record is
 * inserted or updated, it is automatically calculated and saved, occupying
 * storage space. other database doesn't has this type.
 */
enum GenStoreType { STORED, VIRTUAL, INVALID };
class ColumnAttributes {
 public:
  static std::string GetGenStoreName(GenStoreType type);

  ColumnAttributes() { set_bits_ = 0; }

  bool IsNullableSet() { return (set_bits_ & IS_NULL_BIT) != 0; }

  bool GetIsNullable() { return is_nullable_; }

  bool IsDefaultValueSet() { return (set_bits_ & DEFAULT_VALUE_BIT) != 0; }

  std::shared_ptr<ExprToken> GetDefaultValue() { return default_value_; }

  void SetDefaultValue(std::shared_ptr<ExprToken> default_value);

  std::shared_ptr<ExprToken> GetExpGen() { return exp_gen_; }

  GenStoreType GetGenStoreType() { return gen_store_; }

  bool IsCharsetSet() { return (set_bits_ & CHARSET_BIT) != 0; }

  std::string GetCharset() { return charset_; }

  bool IsCommentSet() { return (set_bits_ & COMMENT_BIT) != 0; }

  std::string GetComment() { return comment_; }

  void SetComment(const std::string& comment);

  bool IsVisibleSet() { return (set_bits_ & VISIBLE_BIT) != 0; }

  bool IsVisible() { return visible_; }

  bool IsGeneratedSet() { return (set_bits_ & GENERATED_BIT) != 0; }

  bool IsGenerated() { return (set_bits_ & GENERATED_BIT) != 0; }

  bool IsBinarySet() { return (set_bits_ & BINARY_BIT) != 0; }

  void SetBinary(bool is_binary);

  bool GetIsBinary() { return is_binary_; }

  static long GetGeneratedBit() { return GENERATED_BIT; }

  bool IsAutoInc() { return (set_bits_ & AUTO_INC_BIT) != 0; }

  void SetAutoInc() { set_bits_ |= AUTO_INC_BIT; }

  bool HasOnUpdate() { return (set_bits_ & ON_UPDATE_BIT) != 0; }

  void SetOnUpdate(const std::string& on_update_func);

  std::string GetOnUpdateFunc() { return on_update_func_; }

  bool IsKey() { return (set_bits_ & KEY_BIT) != 0; }

  bool IsAnonymousKey() { return (set_bits_ & ANONYMOUS_KEY_TYPE_BIT) != 0; }

  IndexType GetAnonymousKeyType() { return anonymous_key_type_; }

  void SetAnonymousKeyType(IndexType index_type);

  bool IsCollectionSet() { return (set_bits_ & COLLATION_BIT) != 0; }

  std::string GetCollation() { return collation_; }

  bool IsColumnFormatSet() { return (set_bits_ & COLUMN_FORMAT_BIT) != 0; }

  std::string GetSpatialRefId() { return spatial_ref_id_; }

  void SetSpatialRefId(const std::string& spatial_ref_id);

  bool IsSpatialRefIdSet() { return (set_bits_ & SRID_BIT) != 0; }

  void SetIsNullable(bool is_nullable);

  void SetGenerated(std::shared_ptr<ExprToken> exp_gen,
                    enum GenStoreType gen_store);

 private:
  const static long IS_NULL_BIT = 1;
  const static long DEFAULT_VALUE_BIT = 1 << 1;
  const static long CHARSET_BIT = 1 << 2;
  const static long COMMENT_BIT = 1 << 3;
  const static long VISIBLE_BIT = 1 << 4;
  const static long GENERATED_BIT = 1 << 5;
  const static long BINARY_BIT = 1 << 6;
  const static long AUTO_INC_BIT = 1 << 7;
  const static long ON_UPDATE_BIT = 1 << 8;
  const static long KEY_BIT = 1 << 9;
  const static long COLLATION_BIT = 1 << 10;
  const static long ANONYMOUS_KEY_TYPE_BIT = 1 << 11;
  const static long COLUMN_FORMAT_BIT = 1 << 12;
  const static long SRID_BIT = 1 << 13;

  const static long IDENTITY_COLUMN_BIT = 1 << 14;

  bool is_nullable_;
  std::shared_ptr<ExprToken> default_value_;
  std::string charset_;
  std::string collation_;
  std::string comment_;
  bool visible_;
  bool is_binary_;
  long set_bits_;
  std::string on_update_func_;
  IndexType anonymous_key_type_;
  std::shared_ptr<ExprToken> exp_gen_;
  GenStoreType gen_store_;
  // mysql columns with a spatial data type can have an SRID attribute,
  // to explicitly indicate the spatial reference system (SRS) for values stored
  // in the column
  // this attribute is supported starting from 8.0
  std::string spatial_ref_id_;

  // identity column
  bool is_identity_column_;
};

}  // namespace object

}  // namespace etransfer
