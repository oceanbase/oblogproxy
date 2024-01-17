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
#include <vector>

#include "common/catalog.h"
#include "common/define.h"
#include "common/raw_constant.h"
#include "object/object_filter.h"
namespace etransfer {
namespace object {

using namespace common;

// Object is an intermediate structure in the DDL conversion process.
// Each type of DDL statement has a corresponding implementation class
// for the object, such as the CreateTableObject class for the
// create table DDL statement.
class Object {
 public:
  using ObjectPtrs = std::vector<std::shared_ptr<Object>>;

  Object(const Catalog& catalog, const RawConstant& object_name,
         ObjectType object_type)
      : catalog(catalog), object_name(object_name), object_type(object_type) {}

  Object(const Catalog& catalog, const RawConstant& object_name,
         const std::string& raw_info, ObjectType object_type)
      : catalog(catalog),
        object_name(object_name),
        object_type(object_type),
        raw_info(raw_info) {}

  common::ObjectType GetObjectType() const;

  // catalog of object
  common::Catalog GetCatalog() const;

  // object name
  common::RawConstant GetRawObjectName() const;

  void SetSubObjects(const ObjectPtrs& sub_objects);

  // simple resort it
  ObjectPtrs ResortSubObjects(const ObjectPtrs& sub_objects);

  std::string GetRawInfo() { return raw_info; }

  // return true, if object is filtered.
  bool DoFilter(const ObjectFilter& object_filter);

  virtual bool FilterInner(const ObjectFilter& object_filter);

  ObjectPtrs& GetSubObjects() { return sub_objects; }

 protected:
  common::Catalog catalog;
  common::RawConstant object_name;
  common::ObjectType object_type;
  std::string raw_info;
  ObjectPtrs sub_objects;
};

using ObjectPtr = std::shared_ptr<Object>;
using ObjectPtrs = std::vector<ObjectPtr>;

};  // namespace object
};  // namespace etransfer