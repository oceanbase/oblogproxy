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

#include "jsonutil.hpp"

namespace oceanbase {
namespace logproxy {

// bool operator==(const JsonValues& l, const JsonValueType& r) {
//     return (int)l == (int)r;
// }
//
// bool operator==(const JsonValueType& l, const JsonValues& r) {
//     return (int)l == (int)r;
// }
//
// bool operator!=(const JsonValues& l, const JsonValueType& r) {
//     return (int)l != (int)r;
// }
//
// bool operator!=(const JsonValueType& l, const JsonValues& r) {
//     return (int)l != (int)r;
// }
//
// JsonValue::JsonValue(JsonValues type) : Json::Value((Json::ValueType)type) {}
//
// JsonValue::JsonValue(JsonInt value) : Json::Value(value) {}
//
// JsonValue::JsonValue(JsonUInt value) : Json::Value(value) {}
//
// JsonValue::JsonValue(JsonInt64 value) : Json::Value(value) {}
//
// JsonValue::JsonValue(JsonUInt64 value) : Json::Value(value) {}
//
// JsonValue::JsonValue(double value) : Json::Value(value) {}
//
// JsonValue::JsonValue(const char* value) : Json::Value(value) {}
//
// JsonValue::JsonValue(const std::string& value) : Json::Value(value) {}
//
// JsonValue::JsonValue(bool value) : Json::Value(value) {}
//
// JsonValue::JsonValue(const Json::Value& other) : Json::Value(other) {}
//
// JsonValue::JsonValue(Json::Value&& other) : Json::Value(std::move(other)) {}
//
// JsonValue& JsonValue::operator=(JsonValues type) {
//     Json::Value::operator=((Json::ValueType)type);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(JsonInt value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(JsonUInt value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(JsonInt64 value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(JsonUInt64 value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(double value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(const char* value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(const std::string& value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(bool value) {
//     Json::Value::operator=(value);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(const JsonValue& other) {
//     Json::Value::operator=((Json::Value)other);
//     return *this;
// }
//
// JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
//     Json::Value::operator=((Json::Value)other);
//     return *this;
// }
//
// bool JsonValue::operator<(const JsonValue& other) const {
//     return Json::Value::operator<(other);
// }
//
// bool JsonValue::operator<=(const JsonValue& other) const {
//     return Json::Value::operator<=(other);
// }
//
// bool JsonValue::operator>=(const JsonValue& other) const {
//     return Json::Value::operator>=(other);
// }
//
// bool JsonValue::operator>(const JsonValue& other) const {
//     return Json::Value::operator>(other);
// }
//
// bool JsonValue::operator==(const JsonValue& other) const {
//     return Json::Value::operator==(other);
// }
//
// bool JsonValue::operator!=(const JsonValue& other) const {
//     return Json::Value::operator!=(other);
// }
//
// JsonValue& JsonValue::operator[](JsonArrayIndex index) {
//     return (JsonValue&)Json::Value::operator[](index);
// }
//
// const JsonValue& JsonValue::operator[](JsonArrayIndex index) const {
//     return (const JsonValue&)Json::Value::operator[](index);
// }
//
// JsonValue& JsonValue::operator[](int index) {
//     return (JsonValue&)Json::Value::operator[](index);
// }
//
// const JsonValue& JsonValue::operator[](int index) const {
//     return (const JsonValue&)Json::Value::operator[](index);
// }
//
// JsonValue& JsonValue::operator[](const char* key) {
//     return (JsonValue&)Json::Value::operator[](key);
// }
//
// const JsonValue& JsonValue::operator[](const char* key) const {
//     return (const JsonValue&)Json::Value::operator[](key);
// }
//
// JsonValue& JsonValue::operator[](const std::string& key) {
//     return (JsonValue&)Json::Value::operator[](key);
// }
//
// const JsonValue& JsonValue::operator[](const std::string& key) const {
//     return (const JsonValue&)Json::Value::operator[](key);
// }
//
// JsonValue::const_iterator JsonValue::begin() const {
//     return Json::Value::begin();
// }
//
// JsonValue::const_iterator JsonValue::end() const {
//     return Json::Value::end();
// }
//
// JsonValue::iterator JsonValue::begin() {
//     return Json::Value::begin();
// }
//
// JsonValue::iterator JsonValue::end() {
//     return Json::Value::end();
// }
//
// JsonValue::iterator::iterator(const Json::Value::iterator& other) : Json::Value::iterator(other) {}
//
// JsonValue::iterator& JsonValue::iterator::operator=(const JsonValue::iterator& other) {
//     Json::Value::iterator::operator=(other);
//     return *this;
// }
//
// JsonValue::iterator JsonValue::iterator::operator++(int) {
//     Json::Value::iterator::operator++(1);
//     return *this;
// }
//
// JsonValue::iterator JsonValue::iterator::operator--(int) {
//     Json::Value::iterator::operator--(1);
//     return *this;
// }
//
// JsonValue::iterator& JsonValue::iterator::operator++() {
//     Json::Value::iterator::operator++();
//     return *this;
// }
//
// JsonValue::iterator& JsonValue::iterator::operator--() {
//     Json::Value::iterator::operator--();
//     return *this;
// }
//
// JsonValue& JsonValue::iterator::operator*() const {
//     return (JsonValue&)Json::Value::iterator::operator*();
// }
//
// JsonValue::const_iterator::const_iterator(const Json::Value::const_iterator& other) :
//         Json::Value::const_iterator(other) {}
//
// JsonValue::const_iterator&
// JsonValue::const_iterator::operator=(const JsonValue::const_iterator& other) {
//     Json::Value::const_iterator::operator=(other);
//     return *this;
// }
//
// JsonValue::const_iterator JsonValue::const_iterator::operator++(int) {
//     Json::Value::const_iterator::operator++(1);
//     return *this;
// }
//
// JsonValue::const_iterator JsonValue::const_iterator::operator--(int) {
//     Json::Value::const_iterator::operator--(1);
//     return *this;
// }
//
// JsonValue::const_iterator& JsonValue::const_iterator::operator++() {
//     Json::Value::const_iterator::operator++();
//     return *this;
// }
//
// JsonValue::const_iterator& JsonValue::const_iterator::operator--() {
//     Json::Value::const_iterator::operator--();
//     return *this;
// }
//
// JsonValue& JsonValue::const_iterator::operator*() const {
//     return (JsonValue&)Json::Value::const_iterator::operator*();
// }

}
}  // namespace oceanbase
