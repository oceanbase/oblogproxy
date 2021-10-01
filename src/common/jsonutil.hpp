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

// enum JsonValues {
//     JsonNullValue = Json::nullValue,
//     JsonIntValue = Json::intValue,
//     JsonUIntValue = Json::uintValue,
//     JsonRealValue = Json::realValue,
//     JsonStringValue = Json::stringValue,
//     JsonBooleanValue = Json::booleanValue,
//     JsonArrayValue = Json::arrayValue,
//     JsonObjectValue = Json::objectValue
// };
//
// typedef ::Json::ValueType JsonValueType;
//
////bool operator==(const JsonValues& l, const JsonValueType& r);
////
////bool operator==(const JsonValueType& l, const JsonValues& r);
////
////bool operator!=(const JsonValues& l, const JsonValueType& r);
////
////bool operator!=(const JsonValueType& l, const JsonValues& r);
//
// typedef ::Json::Int JsonInt;
// typedef ::Json::UInt JsonUInt;
// typedef ::Json::Int64 JsonInt64;
// typedef ::Json::UInt64 JsonUInt64;
// typedef ::Json::ArrayIndex JsonArrayIndex;
// typedef ::Json::Value JsonValue;

// wrap and forward JSONCPP API that support JsonValues type constructor
// class JsonValue : public Json::Value {
// public:
//    explicit JsonValue(JsonValues type = JsonNullValue);
//
//    explicit JsonValue(JsonInt value);
//
//    explicit JsonValue(JsonUInt value);
//
//    explicit JsonValue(JsonInt64 value);
//
//    explicit JsonValue(JsonUInt64 value);
//
//    explicit JsonValue(double value);
//
//    explicit JsonValue(const char* value);
//
//    explicit JsonValue(const std::string& value);
//
//    explicit JsonValue(bool value);
//
//    JsonValue(const JsonValue& other) = default;
//
//    JsonValue(JsonValue&& other) = default;
//
//    explicit JsonValue(const Json::Value& other);
//
//    explicit JsonValue(Json::Value&& other);
//
//    JsonValue& operator=(JsonValues type);
//
//    JsonValue& operator=(JsonInt value);
//
//    JsonValue& operator=(JsonUInt value);
//
//    JsonValue& operator=(JsonInt64 value);
//
//    JsonValue& operator=(JsonUInt64 value);
//
//    JsonValue& operator=(double value);
//
//    JsonValue& operator=(const char* value);
//
//    JsonValue& operator=(const std::string& value);
//
//    JsonValue& operator=(bool value);
//
//    JsonValue& operator=(const JsonValue& other);
//
//    JsonValue& operator=(JsonValue&& other) noexcept;
//
//    bool operator<(const JsonValue& other) const;
//
//    bool operator<=(const JsonValue& other) const;
//
//    bool operator>=(const JsonValue& other) const;
//
//    bool operator>(const JsonValue& other) const;
//
//    bool operator==(const JsonValue& other) const;
//
//    bool operator!=(const JsonValue& other) const;
//
//    JsonValue& operator[](JsonArrayIndex index);
//
//    const JsonValue& operator[](JsonArrayIndex index) const;
//
//    JsonValue& operator[](int index);
//
//    const JsonValue& operator[](int index) const;
//
//    JsonValue& operator[](const char* key);
//
//    const JsonValue& operator[](const char* key) const;
//
//    JsonValue& operator[](const std::string& key);
//
//    const JsonValue& operator[](const std::string& key) const;
//
//    class iterator : public Json::Value::iterator {
//    public:
//        iterator(const Json::Value::iterator& other);
//
//        iterator& operator=(const iterator& other);
//
//        iterator operator++(int);
//
//        iterator operator--(int);
//
//        iterator& operator++();
//
//        iterator& operator--();
//
//        JsonValue& operator*() const;
//    };
//
//    class const_iterator : public Json::Value::const_iterator {
//    public:
//        const_iterator(const Json::Value::const_iterator& other);
//
//        const_iterator& operator=(const const_iterator& other);
//
//        const_iterator operator++(int);
//
//        const_iterator operator--(int);
//
//        const_iterator& operator++();
//
//        const_iterator& operator--();
//
//        JsonValue& operator*() const;
//    };
//
//    const_iterator begin() const;
//
//    const_iterator end() const;
//
//    iterator begin();
//
//    iterator end();
//};

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
