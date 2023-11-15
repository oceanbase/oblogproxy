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
bool str2json(const std::string& str, JT& json, std::string* errmsg = nullptr)
{
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  return reader->parse(str.data(), str.data() + str.size(), &json, errmsg);
}

template <class JT>
bool str2json(std::ifstream& ifs, JT& json, std::string* errmsg = nullptr)
{
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  return Json::parseFromStream(builder, (Json::IStream&)ifs, &json, errmsg);
}

template <typename T>
Json::Value to_json(const T& object)
{
  Json::Value data;

  // We first get the number of properties
  constexpr auto properties_count = std::tuple_size<decltype(T::properties)>::value;

  // We iterate on the index sequence of size `properties_count`
  for_sequence(std::make_index_sequence<properties_count>{}, [&](auto i) {
    // get the property
    constexpr auto property = std::get<i>(T::properties);

    // set the value to the member
    data[property.name] = static_cast<Json::Value>(object.*(property.member));

    //    data[property.name] = to_json(object.*(property.member));
  });

  return data;
}

template <typename T, T... S, typename F>
constexpr void for_sequence(std::integer_sequence<T, S...>, F&& f)
{
  (static_cast<void>(f(std::integral_constant<T, S>{})), ...);
}

template <typename Class, typename T>
struct PropertyImpl {
  constexpr PropertyImpl(T Class::*aMember, const char* aName) : member{aMember}, name{aName}
  {}

  using Type = T;

  T Class::*member;
  const char* name;
};

template <typename Class, typename T>
constexpr auto property(T Class::*member, const char* name)
{
  return PropertyImpl<Class, T>{member, name};
}

// unserialize function
template <typename T>
T from_json(const Json::Value& data)
{
  T object;

  // We first get the number of properties_count
  constexpr auto properties_count = std::tuple_size<decltype(T::properties)>::value;

  // We iterate on the index sequence of size `properties_count`
  for_sequence(std::make_index_sequence<properties_count>{}, [&](auto i) {
    // get the property
    constexpr auto property = std::get<i>(T::properties);

    // get the type of the property
    //    using Type = typename decltype(property)::Type;

    // set the value to the member
    // you can also replace `asAny` by `from_json` to recursively serialize
    object.*(property.member) = from_json(data[property.name]);
  });

  return object;
}

}  // namespace logproxy
}  // namespace oceanbase
