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

#include <string>
#include <map>
#include <unordered_map>
#include "jni.h"

namespace oceanbase {
namespace logproxy {
struct JniMethod {
public:
  static jmethodID MapSize;
  static jmethodID MapEntrySet;
  static jmethodID SetSize;
  static jmethodID SetIterator;
  static jmethodID IteratorHasNext;
  static jmethodID IteratorNext;
  static jmethodID MapEntryGetKey;
  static jmethodID MapEntryGetValue;
  static jmethodID IntValue;
};

class JniClassGuard {
public:
  JniClassGuard(JNIEnv* env, const std::string& name);

  virtual ~JniClassGuard();

  const std::string& name() const
  {
    return _name;
  }

  jmethodID reg_method(const std::string& method, const std::string& sig);

private:
  JNIEnv* _env;
  std::string _name;
  jclass _clazz = nullptr;
};

class JniHint {
public:
  static jmethodID reg_method(
      JNIEnv* env, jclass jclazz, const std::string& clazz, const std::string& name, const std::string& sig);

  static int to_map(JNIEnv* env, jobject jmap, std::map<std::string, std::string>& out_map);

private:
  static std::unordered_map<std::string, jmethodID> g_jmethod_ids;
};

}  // namespace logproxy
}  // namespace oceanbase
