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
#include "common/raw_constant.h"
#include "common/util.h"
namespace etransfer {
namespace common {
class Catalog {
 public:
  Catalog(const RawConstant& catalog_name) : catalog_name_(catalog_name) {}
  Catalog() : catalog_name_("") {}

  std::string GetCatalogName() const {
    return Util::RawConstantValue(catalog_name_);
  }

  std::string GetCatalogName(bool use_orig_name) const {
    return Util::RawConstantValue(catalog_name_, use_orig_name);
  }

  const RawConstant& GetRawCatalogName() const { return catalog_name_; }

 private:
  // alias database name
  RawConstant catalog_name_;
};

};  // namespace common
};  // namespace etransfer