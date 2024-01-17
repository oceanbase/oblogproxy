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

#include "object/type_info.h"
namespace etransfer {
namespace object {
const FloatingTypeInfo FloatingTypeInfo::default_float_info(
    TypeInfo::ANONYMOUS_NUMBER, false);
const FloatingTypeInfo FloatingTypeInfo::default_double_info(
    TypeInfo::ANONYMOUS_NUMBER, true);
const BooleanTypeInfo BooleanTypeInfo::default_boolean_info;
const TimeTypeInfo TimeTypeInfo::default_time_info(TypeInfo::ANONYMOUS_NUMBER);
const CharacterTypeInfo CharacterTypeInfo::default_character_info(
    TypeInfo::ANONYMOUS_NUMBER, TypeInfo::ANONYMOUS_NUMBER, "", false);
const LOBTypeInfo LOBTypeInfo::default_lob_info(false,
                                                TypeInfo::ANONYMOUS_NUMBER,
                                                TypeInfo::ANONYMOUS_NUMBER, "");
const BinaryTypeInfo BinaryTypeInfo::default_binary_info(
    TypeInfo::ANONYMOUS_NUMBER);
const int TypeInfo::ANONYMOUS_NUMBER = -32769;
const long TypeInfo::ARBITRARY_LENGTH = 0x7fffffffffffffffL;
const int TypeInfo::MAX_NVARCHAR2_LEN = 2000;
}  // namespace object

}  // namespace etransfer
