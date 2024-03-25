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

#ifdef _OBLOG_MSG_
#include "MsgHeader.h"
using namespace oceanbase::logmessage;
#else
#include <cstdint>

enum { MT_UNKNOWN = 0, MT_META, MT_FIXED, MT_VAR, MT_EXT };
struct MsgHeader {
  uint16_t m_msgType;
  uint16_t m_version;
  uint32_t m_size;
};

#endif