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
#include <set>

#include "common/define.h"
namespace etransfer {
namespace object {
using namespace common;
class ObjectFilter {
 public:
  // index type to filter
  std::set<IndexType> index_type_to_filter;

 public:
  std::set<IndexType>& GetIndexTypeToFilter();
  void AddIndexTypeFilter(IndexType index_type);
  void Clear();
};
}  // namespace object

}  // namespace etransfer
