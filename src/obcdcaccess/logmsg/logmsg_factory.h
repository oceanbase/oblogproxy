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
#include "LogMsgFactory.h"
using namespace oceanbase::logmessage;
#else
#include "DRCMessageFactory.h"

#include "meta_info.h"
#include "log_record.h"

static std::string DFT_LR = "LogRecordImpl";
class LogMsgFactory {
public:
  static IColMeta* createColMeta(const std::string& type = DRCMessageFactory::DFT_ColMeta)
  {
    return DRCMessageFactory::createColMeta(type);
  }

  static IColMeta* createColMeta(const std::string& type, const char* ptr, size_t size)
  {
    return DRCMessageFactory::createColMeta(type, ptr, size);
  }

  static ITableMeta* createTableMeta(const std::string& type = DRCMessageFactory::DFT_TableMeta)
  {
    return DRCMessageFactory::createTableMeta(type);
  }

  static ITableMeta* createTableMeta(const std::string& type, const char* ptr, size_t size)
  {
    return DRCMessageFactory::createTableMeta(type, ptr, size);
  }

  static IDBMeta* createDBMeta(const std::string& type = DRCMessageFactory::DFT_DBMeta)
  {
    return DRCMessageFactory::createDBMeta(type);
  }

  static IDBMeta* createDBMeta(const std::string& type, const char* ptr, size_t size)
  {
    return DRCMessageFactory::createDBMeta(type, ptr, size);
  }

  static IMetaDataCollections* createMetaDataCollections(const std::string& type = DRCMessageFactory::DFT_METAS)
  {
    return DRCMessageFactory::createMetaDataCollections(type);
  }

  static IMetaDataCollections* createMetaDataCollections(
      const std::string& type, const char* ptr, size_t size, bool freeMem)
  {
    return DRCMessageFactory::createMetaDataCollections(type, ptr, size, freeMem);
  }

  static ILogRecord* createLogRecord(const std::string& type = DFT_LR, bool creating = true)
  {
    std::string type_corrected = (DFT_LR == type) ? DRCMessageFactory::DFT_BR : type;
    return DRCMessageFactory::createBinlogRecord(type_corrected, creating);
  }

  static ILogRecord* createLogRecord(const std::string& type, const char* ptr, size_t size)
  {
    std::string type_corrected = (DFT_LR == type) ? DRCMessageFactory::DFT_BR : type;
    return DRCMessageFactory::createBinlogRecord(type, ptr, size);
  }

  static void destroyWithUserMemory(ILogRecord*& record)
  {
    DRCMessageFactory::destroyWithUserMemory(record);
  }

  static void destroy(IColMeta*& colMeta)
  {
    DRCMessageFactory::destroy(colMeta);
  }

  static void destroy(ITableMeta*& tableMeta)
  {
    DRCMessageFactory::destroy(tableMeta);
  }

  static void destroy(IDBMeta*& dbMeta)
  {
    DRCMessageFactory::destroy(dbMeta);
  }

  static void destroy(IMetaDataCollections*& metaColls)
  {
    DRCMessageFactory::destroy(metaColls);
  }
};

#endif
