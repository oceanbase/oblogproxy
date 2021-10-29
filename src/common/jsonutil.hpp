/**
 * Copyright (c) 2021 OceanBase
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

#include "json/json.h"

namespace oceanbase {
namespace logproxy {

template <class JT>
std::string json2str(const JT& json, bool formate = false)
{
  Json::StreamWriterBuilder builder;
  if (!formate) {
    builder.settings_["indentation"] = "";
  }
  return Json::writeString(builder, json);
}

template <class JT>
bool str2json(const std::string& str, JT& json)
{
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  return reader->parse(str.data(), str.data() + str.size(), &json, nullptr);
}

template <class JT>
bool str2json(const std::string& str, JT& json, std::string* errmsg = nullptr)
{
  std::string err;
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  return Json::parseFromStream(str.data(), str.data() + str.size(), &json, errmsg);
}

template <class JT>
bool str2json(std::ifstream& ifs, JT& json, std::string* errmsg = nullptr)
{
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  return Json::parseFromStream(builder, (Json::IStream&)ifs, &json, errmsg);
}

}  // namespace logproxy
}  // namespace oceanbase
