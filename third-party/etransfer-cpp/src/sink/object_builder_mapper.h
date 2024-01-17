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
#include <unordered_map>

#include "common/define.h"
#include "sink/object_builder.h"
namespace etransfer {
namespace sink {
using namespace common;
using BuilderMap =
    std::unordered_map<ObjectType, std::shared_ptr<ObjectBuilder>>;
class ObjectBuilderMapper {
 public:
  static const BuilderMap& SqlBuilderMapper() {
    return mysql_object_builder_mapper;
  }
  static std::shared_ptr<ObjectBuilder> GetObjectBuilder(
      ObjectType object_type);
  static const BuilderMap mysql_object_builder_mapper;
  static BuilderMap InitMapper();
};
}  // namespace sink

}  // namespace etransfer
