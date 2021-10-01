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

#include <assert.h>
#include "common.h"
#include "jni_hint.h"

namespace oceanbase {
namespace logproxy {

jmethodID JniMethod::MapSize;
jmethodID JniMethod::MapEntrySet;
jmethodID JniMethod::SetSize;
jmethodID JniMethod::SetIterator;
jmethodID JniMethod::IteratorHasNext;
jmethodID JniMethod::IteratorNext;
jmethodID JniMethod::MapEntryGetKey;
jmethodID JniMethod::MapEntryGetValue;
jmethodID JniMethod::IntValue;

std::unordered_map<std::string, jmethodID> JniHint::g_jmethod_ids;

JniClassGuard::JniClassGuard(JNIEnv* env, const std::string& name) : _env(env), _name(name)
{
  _clazz = _env->FindClass(_name.c_str());
}

JniClassGuard::~JniClassGuard()
{
  assert(_env != nullptr);
  if (_clazz != nullptr) {
    _env->DeleteLocalRef(_clazz);
    _clazz = nullptr;
  }
}

jmethodID JniClassGuard::reg_method(const std::string& name, const std::string& sig)
{
  assert(_env != nullptr && _clazz != nullptr);
  return JniHint::reg_method(_env, _clazz, _name, name, sig);
}

jmethodID JniHint::reg_method(
    JNIEnv* env, jclass jclazz, const std::string& clazz, const std::string& name, const std::string& sig)
{
  jmethodID mid = env->GetMethodID(jclazz, name.c_str(), sig.c_str());
  std::string key = clazz + "." + name + sig;
  g_jmethod_ids.emplace(name, mid);
  return mid;
}

int JniHint::to_map(JNIEnv* env, jobject jmap, std::map<std::string, std::string>& out_map)
{
  if (jmap == nullptr) {
    return OMS_FAILED;
  }

  jobject jset = env->CallObjectMethod(jmap, JniMethod::MapEntrySet);
  jobject jiter = env->CallObjectMethod(jset, JniMethod::SetIterator);
  while (env->CallBooleanMethod(jiter, JniMethod::IteratorHasNext)) {
    jobject jentry = env->CallObjectMethod(jiter, JniMethod::IteratorNext);

    jstring jkey = (jstring)env->CallObjectMethod(jentry, JniMethod::MapEntryGetKey);
    if (jkey == nullptr) {  // HashMap允许null类型
      continue;
    }
    const char* key = env->GetStringUTFChars(jkey, nullptr);

    jstring jval = (jstring)env->CallObjectMethod(jentry, JniMethod::MapEntryGetValue);
    if (jval == nullptr) {
      out_map.emplace(key, "");
      env->ReleaseStringUTFChars(jkey, key);
      continue;
    }
    const char* val = env->GetStringUTFChars(jval, nullptr);
    out_map.emplace(key, val);
    env->ReleaseStringUTFChars(jkey, key);
    env->ReleaseStringUTFChars(jval, val);

    env->DeleteLocalRef(jentry);
    env->DeleteLocalRef(jkey);
    env->DeleteLocalRef(jval);
  }

  env->DeleteLocalRef(jset);
  env->DeleteLocalRef(jiter);
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
