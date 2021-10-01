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

#include <map>
#include "oblogreader_jni.h"
#include "oblog_access.h"
#include "jni_hint.h"
#include "common.h"

using namespace oceanbase::logproxy;

static OblogAccess oblog;

static const jint JNI_VERSION = JNI_VERSION_1_8;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
  JNIEnv* env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
    return JNI_ERR;
  }
  return JNI_VERSION;
}

static inline void register_jmethod(JNIEnv* env)
{
  // Map
  JniClassGuard map(env, "java/util/Map");
  JniMethod::MapSize = map.reg_method("count", "()I");
  JniMethod::MapEntrySet = map.reg_method("entrySet", "()Ljava/util/Set;");

  // Set
  JniClassGuard set(env, "java/util/Set");
  JniMethod::SetSize = set.reg_method("count", "()I");
  JniMethod::SetIterator = set.reg_method("iterator", "()Ljava/util/Iterator;");

  // Iterator
  JniClassGuard iterator(env, "java/util/Iterator");
  JniMethod::IteratorHasNext = iterator.reg_method("hasNext", "()Z");
  JniMethod::IteratorNext = iterator.reg_method("next", "()Ljava/lang/Object;");

  // Map.Entry
  JniClassGuard map_entry(env, "java/util/Map$Entry");
  JniMethod::MapEntryGetKey = map_entry.reg_method("getKey", "()Ljava/lang/Object;");
  JniMethod::MapEntryGetValue = map_entry.reg_method("getValue", "()Ljava/lang/Object;");

  // Interger
  JniClassGuard integer(env, "java/lang/Integer");
  JniMethod::IntValue = integer.reg_method("intValue", "()I");
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{}

/**
 *
 * @param env
 * @param jconfigs          HashMap<String, String>
 * @param start_timestamp   long
 * @return
 */
jboolean JNICALL Java_com_oceanbase_logclient_LibOblogReaderJni_init(
    JNIEnv* env, jclass, jobject jconfigs, jlong start_timestamp)
{

  std::map<std::string, std::string> configs;
  int ret = JniHint::to_map(env, jconfigs, configs);
  if (ret != OMS_OK) {
    return JNI_FALSE;
  }
  if (oblog.init(configs, start_timestamp) != 0) {
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

jboolean JNICALL Java_com_oceanbase_logclient_LibOblogReaderJni_start(JNIEnv* env, jclass)
{
  return oblog.start() == OMS_OK;
}

/*
 * Class:     com_oceanbase_logclient_LibOblogReaderJni
 * Method:    stop
 * Signature: ()V
 */
void JNICALL Java_com_oceanbase_logclient_LibOblogReaderJni_stop(JNIEnv* env, jclass)
{
  oblog.stop();
}

/*
 * Class:     com_oceanbase_logclient_LibOblogReaderJni
 * Method:    fetch
 * Signature: ()Lcom/oceanbase/oms/store/client/message/drcmessage/DrcNETBinaryRecord;
 */
jobject JNICALL Java_com_oceanbase_logclient_LibOblogReaderJni_next__(JNIEnv* env, jclass)
{
  return nullptr;
}

/*
 * Class:     com_oceanbase_logclient_LibOblogReaderJni
 * Method:    fetch
 * Signature: (J)Lcom/oceanbase/oms/store/client/message/drcmessage/DrcNETBinaryRecord;
 */
jobject JNICALL Java_com_oceanbase_logclient_LibOblogReaderJni_next__J(JNIEnv* env, jclass, jlong timeout_us)
{
  return nullptr;
}
