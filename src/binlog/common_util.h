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

#include <string>
#include <iomanip>
#include <random>
#include <execinfo.h>

#include "config.h"

#include <gdal.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include "codec/byte_decoder.h"
#include "common/msg_buf.h"
#include "conncpp.hpp"
#include "str.h"

#include <log_record.h>

namespace oceanbase::binlog {
static uint16_t g_file_name_width = logproxy::Config::instance().binlog_file_name_fill_zeroes_width.val();
static std::string g_binlog_file_prefix = logproxy::Config::instance().binlog_log_bin_prefix.val();
class CommonUtils {
public:
  /*
   * Fill Binlog file name with specified width
   */
  static std::string fill_binlog_file_name(uint64_t index)
  {
    std::stringstream string_stream;
    string_stream << std::setfill('0') << std::setw(g_file_name_width) << index;
    return g_binlog_file_prefix + "." + string_stream.str();
  }

  static std::map<sql::SQLString, sql::SQLString> serialized_kv_config(const std::string& str)
  {
    std::vector<std::string> kvs;
    std::map<sql::SQLString, sql::SQLString> k_pairs;
    logproxy::split(str, ' ', kvs);
    for (std::string& kv : kvs) {
      std::vector<std::string> kv_split;
      int count = split(kv, '=', kv_split, true);
      if (count != 2) {
        continue;
      }
      k_pairs.emplace(kv_split[0], kv_split[1]);
    }
    return k_pairs;
  }

  static uint64_t get_binlog_index(const std::string& binlog_file)
  {
    std::string index = binlog_file.substr(binlog_file.length() - g_file_name_width);
    return std::atoll(index.c_str());
  }

  static void hex_to_bin(const std::string& hex, unsigned char* ret)
  {
    int pos = 0;
    auto len = hex.size();
    for (decltype(len) i = 0; i < len; i += 2) {
      unsigned int element;
      std::istringstream str_hex(hex.substr(i, 2));
      str_hex >> std::hex >> element;
      memcpy(ret + pos, &element, 1);
      pos += 1;
    }
  }

  static std::string gtid_format(const unsigned char* gtid_uuid, uint64_t gtid_txn_id)
  {
    /*
 * 89fbcea2-db65-11e7-a851-fa163e618bac:1-5:999:1050-1052
  +-----+-----+-----+-----+-----+
  |4 bit|2 bit|2 bit|2 bit|6 bit|
  +-----+-----+-----+-----+-----+
 */
    //  unsigned char uuid_str[20];
    std::string uuid_str;
    std::stringstream stream;
    // uuid
    //  stream << this->_gtid_uuid.substr(0,4)<<"-"<< this->_gtid_uuid.substr(4,2);

    logproxy::dumphex(reinterpret_cast<const char*>(gtid_uuid), 16, uuid_str);
    int pos = 0;
    std::string item_first = uuid_str.substr(pos, 8);
    transform(item_first.begin(), item_first.end(), item_first.begin(), ::tolower);
    stream << item_first << "-";
    pos += 8;

    std::string item_second = uuid_str.substr(pos, 4);
    transform(item_second.begin(), item_second.end(), item_second.begin(), ::tolower);
    stream << item_second << "-";
    pos += 4;

    std::string item_third = uuid_str.substr(pos, 4);
    transform(item_third.begin(), item_third.end(), item_third.begin(), ::tolower);
    stream << item_third << "-";
    pos += 4;

    std::string item_fourth = uuid_str.substr(pos, 4);
    transform(item_fourth.begin(), item_fourth.end(), item_fourth.begin(), ::tolower);
    stream << item_fourth << "-";
    pos += 4;

    std::string item_fifth = uuid_str.substr(pos, 12);
    transform(item_fifth.begin(), item_fifth.end(), item_fifth.begin(), ::tolower);
    stream << item_fifth;
    stream << ":" << gtid_txn_id;
    return stream.str();
  }

  static unsigned int random()
  {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    return dis(gen);
  }

  static std::string generate_trace_id()
  {
    std::stringstream ss;
    for (auto i = 0; i < 16; i++) {
      const auto rc = random();
      std::stringstream hexstream;
      hexstream << std::hex << rc;
      auto hex = hexstream.str();
      ss << hex;
    }
    return ss.str();
  }

  /*
   * @params full_dbname tenant.dbname
   * @returns dbname
   * @description get database name does not contain tenant name
   * @date 2022/10/19 14:43
   */
  static std::string get_dbname_without_tenant(const std::string& full_dbname, const std::string& tenant_name)
  {
    if (full_dbname.empty() || tenant_name.empty()) {
      return full_dbname;
    }
    size_t pos = full_dbname.find(tenant_name);
    if (pos != std::string::npos && pos == 0 && full_dbname.size() > tenant_name.size()) {
      return full_dbname.substr(pos + tenant_name.size() + 1);
    } else {
      return full_dbname;
    }
  }

  static uint64_t get_timestamp_sec(ILogRecord* record)
  {
    return record->getTimestamp();
  }

  static uint64_t get_timestamp_usec(ILogRecord* record)
  {
    return record->getTimestamp() * 1000 * 1000 + record->getRecordUsec();
  }

  static uint64_t get_checkpoint_usec(ILogRecord* record)
  {
    return record->getCheckpoint1() * 1000 * 1000 + record->getCheckpoint2();
  }

  static std::string get_transaction_id(ILogRecord* record)
  {
    std::string ret;
    unsigned int count = 0;
    const BinLogBuf* binlog_buf = ((LogRecordImpl*)record)->filterValues(count);
    if (nullptr != binlog_buf) {
      ret.append(binlog_buf[1].buf);
    }
    return ret;
  }

  static std::string get_stack_trace()
  {
    string stack_trace_msg;
    int size = 16;
    void * array[16];
    int stack_num = backtrace(array, size);
    char ** stacktrace = backtrace_symbols(array, stack_num);
    for (int i = 0; i < stack_num; ++i)
    {
      stack_trace_msg.append(stacktrace[i]).append("\n");
    }
    free(stacktrace);
    return stack_trace_msg;
  }
};

class GeometryConverter {
public:
  GeometryConverter()
  {
    GDALAllRegister();  // Only register once globally
  }

  ~GeometryConverter()
  {
    OGRCleanupAll();  // Clean resources on exit
  }

  static int parse_ewkt(const std::string& ewkt, int& srid, std::string& wkt)
  {
    size_t pos_srid_end = ewkt.find(';');
    if (pos_srid_end == std::string::npos) {
      srid = 0;
      pos_srid_end = -1;
    } else {
      // Skip "SRID=" five characters
      std::string srid_str = ewkt.substr(5, pos_srid_end - 5);
      std::istringstream srid_stream(srid_str);
      if (!(srid_stream >> srid)) {
        return OMS_FAILED;
      }
    }
    // Get WKT part
    wkt = ewkt.substr(pos_srid_end + 1);
    return OMS_OK;
  }

  static uint64_t convert_wkt_to_wkb(const char* data, oceanbase::logproxy::MsgBuf& data_decode)
  {
    int srid;
    std::string wkt;
    if (parse_ewkt(data, srid, wkt) != OMS_OK) {
      OMS_ERROR("Failed to parse EWKT format:{}", data);
      return OMS_FAILED;
    }
    OGRSpatialReference spatial_ref;
    OGRGeometry* geometry;
    OGRErr err = OGRGeometryFactory::createFromWkt(wkt.c_str(), &spatial_ref, &geometry);
    if (err != OGRERR_NONE) {
      OMS_ERROR("Converting geometry from EWKT format failed");
      return OMS_FAILED;
    }
    // convert geometry to wkb
    size_t wkb_size = geometry->WkbSize();
    // Encode the SRID as a 4-byte integer and add it to the beginning of the WKB
    auto* buff = static_cast<unsigned char*>(malloc(wkb_size + 4 + 4));
    // Then add the standard WKB
    geometry->exportToWkb(wkbNDR, buff + 8);

    // Clean up resources
    OGRGeometryFactory::destroyGeometry(geometry);
    oceanbase::logproxy::int4store(buff, wkb_size + 4);
    oceanbase::logproxy::int4store(buff + 4, srid);
    data_decode.push_back(reinterpret_cast<char*>(buff), wkb_size + 4 + 4);
    return 4 + wkb_size + 4;
  }
};

}  // namespace oceanbase::binlog
