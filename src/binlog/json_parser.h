/**
 * Copyright (c) 2023 OceanBase
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
#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "msg_buf.h"
#include "codec/byte_decoder.h"
namespace oceanbase {
namespace binlog {

#define JSONB_TYPE_SMALL_OBJECT 0x0
#define JSONB_TYPE_LARGE_OBJECT 0x1
#define JSONB_TYPE_SMALL_ARRAY 0x2
#define JSONB_TYPE_LARGE_ARRAY 0x3
#define JSONB_TYPE_LITERAL 0x4
#define JSONB_TYPE_INT16 0x5
#define JSONB_TYPE_UINT16 0x6
#define JSONB_TYPE_INT32 0x7
#define JSONB_TYPE_UINT32 0x8
#define JSONB_TYPE_INT64 0x9
#define JSONB_TYPE_UINT64 0xA
#define JSONB_TYPE_DOUBLE 0xB
#define JSONB_TYPE_STRING 0xC
#define JSONB_TYPE_OPAQUE 0xF

#define JSONB_NULL_LITERAL '\x00'
#define JSONB_TRUE_LITERAL '\x01'
#define JSONB_FALSE_LITERAL '\x02'

/*
  The size of offset or size fields in the small and the large storage
  format for JSON objects and JSON arrays.
*/
#define SMALL_OFFSET_SIZE 2
#define LARGE_OFFSET_SIZE 4

/*
  The size of key entries for objects when using the small storage
  format or the large storage format. In the small format it is 4
  bytes (2 bytes for key length and 2 bytes for key offset). In the
  large format it is 6 (2 bytes for length, 4 bytes for offset).
*/
#define KEY_ENTRY_SIZE_SMALL (2 + SMALL_OFFSET_SIZE)
#define KEY_ENTRY_SIZE_LARGE (2 + LARGE_OFFSET_SIZE)

/*
  The size of value entries for objects or arrays. When using the
  small storage format, the entry size is 3 (1 byte for type, 2 bytes
  for offset). When using the large storage format, it is 5 (1 byte
  for type, 4 bytes for offset).
*/
#define VALUE_ENTRY_SIZE_SMALL (1 + SMALL_OFFSET_SIZE)
#define VALUE_ENTRY_SIZE_LARGE (1 + LARGE_OFFSET_SIZE)

#define JSON_DOCUMENT_MAX_DEPTH 100

class JsonParser {
private:
public:
  static bool is_large_json(size_t offset_or_size)
  {
    if (offset_or_size > UINT16_MAX) {
      return true;
    }
    return false;
  }

  static bool is_int16(int64_t num)
  {
    if (num >= INT16_MIN && num <= INT16_MAX) {
      return true;
    }
    return false;
  }

  static bool is_int32(int64_t num)
  {
    if (num >= INT32_MIN && num <= INT32_MAX) {
      return true;
    }
    return false;
  }

  static bool is_uint16(uint64_t num)
  {
    if (num <= UINT16_MAX) {
      return true;
    }
    return false;
  }

  static bool is_uint32(uint64_t num)
  {
    if (num >= UINT16_MAX && num <= UINT32_MAX) {
      return true;
    }
    return false;
  }

  static bool should_inline_value(
      const rapidjson::Value& value, bool large, int32_t* inlined_val, uint8_t* inlined_type)
  {
    if (value.IsNull()) {
      *inlined_val = JSONB_NULL_LITERAL;
      *inlined_type = JSONB_TYPE_LITERAL;
      return true;
    }

    if (value.IsBool()) {
      *inlined_val = value.GetBool() ? JSONB_TRUE_LITERAL : JSONB_FALSE_LITERAL;
      *inlined_type = JSONB_TYPE_LITERAL;
      return true;
    }

    // handle integer data
    {
      if (value.IsInt()) {
        *inlined_val = value.GetInt();
        if (is_int16(*inlined_val) || (large && is_int32(*inlined_val))) {
          *inlined_type = is_int16(value.GetInt()) ? JSONB_TYPE_INT16 : JSONB_TYPE_INT32;
          return true;
        }
        return false;
      }

      if (value.IsUint()) {
        *inlined_val = value.GetUint();
        if (is_uint16(*inlined_val) || (large && is_uint32(*inlined_val))) {
          *inlined_type = is_uint16(value.GetUint()) ? JSONB_TYPE_UINT16 : JSONB_TYPE_UINT32;
          return true;
        }
        return false;
      }
    }

    return false;
  }

  static char* fill_value(logproxy::MsgBuf& data_decode, unsigned int array_size, bool large, int64_t& offset)
  {
    if (large) {
      auto* json_size = static_cast<char*>(malloc(4));
      logproxy::int4store(reinterpret_cast<unsigned char*>(json_size), array_size);
      data_decode.push_back(json_size, 4);
      offset += 4;
      return json_size;
    } else {
      auto* json_size = static_cast<char*>(malloc(2));
      logproxy::int2store(reinterpret_cast<unsigned char*>(json_size), array_size);
      data_decode.push_back(json_size, 2);
      offset += 2;
      return json_size;
    }
  }

  static int64_t fill_value_for_header(char* header, unsigned int array_size, bool large)
  {
    if (large) {
      logproxy::int4store(reinterpret_cast<unsigned char*>(header), array_size);
      return 4;
    } else {
      logproxy::int2store(reinterpret_cast<unsigned char*>(header), array_size);
      return 2;
    }
  }

  static bool append_variable_length(logproxy::MsgBuf& data_decode, size_t length, int64_t& offset)
  {
    do {
      uint8_t ch = length & 0x7F;
      length >>= 7;
      if (length != 0) {
        ch |= 0x80;
      }
      auto* variable = reinterpret_cast<char*>(malloc(1));
      variable[0] = ch;
      data_decode.push_back(variable, 1);
      offset += 1;
    } while (length != 0);
    return true;
  }

  static void append_value(logproxy::MsgBuf& data_decode, size_t depth, const std::string& parent, int64_t& offset,
      bool large, const rapidjson::Value& val, char* header)
  {
    uint8_t element_type;
    int32_t inlined_value;
    if (should_inline_value(val, large, &inlined_value, &element_type)) {
      //      auto* element_type_dest = static_cast<char*>(malloc(1));
      //      element_type_dest[0] = element_type;
      //      data_decode.push_back(element_type_dest, 1);
      header[0] = element_type;
      //      offset += 1;
      //      fill_value(data_decode, inlined_value, large, offset);
      fill_value_for_header(header + 1, inlined_value, large);
      return;
    }
    //    fill_value(data_decode, offset, large, offset);
    fill_value_for_header(header + 1, offset, large);
    logproxy::MsgBuf element_decode;
    offset += parse_nested_object(val, element_decode, depth + 1, header, parent);
    auto element_decode_size = element_decode.byte_size();
    auto* element_buffer = static_cast<char*>(malloc(element_decode_size));
    element_decode.bytes(element_buffer);
    data_decode.push_back(element_buffer, element_decode_size);
  }
  static int64_t parse_nested_object(const rapidjson::Value& value, logproxy::MsgBuf& data_decode, size_t depth,
      char* header, const std::string& parent = "")
  {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    value.Accept(writer);
    OMS_STREAM_DEBUG << value.GetType() << ":" << sb.GetString();
    sb.Clear();
    int64_t offset = 0;
    switch (value.GetType()) {
      case rapidjson::kNullType: {
        if (header != nullptr) {
          header[0] = JSONB_TYPE_LITERAL;
        } else {
          auto* json_type = static_cast<char*>(malloc(1));
          json_type[0] = JSONB_TYPE_LITERAL;
          data_decode.push_back(json_type, 1);
        }
        auto* val = static_cast<char*>(malloc(1));
        val[0] = JSONB_NULL_LITERAL;
        data_decode.push_back(val, 1);
        offset += 1;
        break;
      }
      case rapidjson::kFalseType: {
        if (header != nullptr) {
          header[0] = JSONB_TYPE_LITERAL;
        } else {
          auto* json_type = static_cast<char*>(malloc(1));
          json_type[0] = JSONB_TYPE_LITERAL;
          data_decode.push_back(json_type, 1);
        }

        auto* val = static_cast<char*>(malloc(1));
        val[0] = JSONB_FALSE_LITERAL;
        data_decode.push_back(val, 1);
        offset += 1;
        break;
      }
      case rapidjson::kTrueType: {
        if (header != nullptr) {
          header[0] = JSONB_TYPE_LITERAL;
        } else {
          auto* json_type = static_cast<char*>(malloc(1));
          json_type[0] = JSONB_TYPE_LITERAL;
          data_decode.push_back(json_type, 1);
        }

        auto* val = static_cast<char*>(malloc(1));
        val[0] = JSONB_TRUE_LITERAL;
        data_decode.push_back(val, 1);
        offset += 1;
        break;
      }
      case rapidjson::kObjectType: {
        auto object_size = value.GetObject().MemberCount();
        bool large = is_large_json(object_size);
        if (header != nullptr) {
          if (large) {
            header[0] = JSONB_TYPE_LARGE_OBJECT;
          } else {
            header[0] = JSONB_TYPE_SMALL_OBJECT;
          }
        } else {
          auto* object_type = static_cast<char*>(malloc(1));
          if (large) {
            object_type[0] = JSONB_TYPE_LARGE_OBJECT;
          } else {
            object_type[0] = JSONB_TYPE_SMALL_OBJECT;
          }
          data_decode.push_back(object_type, 1);
          //          offset += 1;
        }

        fill_value(data_decode, object_size, large, offset);

        // fill object bytes,this is a pre-occupied place, and finally filled with real data
        char* object_bytes = fill_value(data_decode, 0, large, offset);

        size_t key_size = large ? KEY_ENTRY_SIZE_LARGE : KEY_ENTRY_SIZE_SMALL;
        size_t value_size = large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL;

        offset += object_size * (key_size + value_size);

        for (auto& member : value.GetObject()) {
          const auto& key = member.name.GetString();
          size_t key_len = strlen(key);
          int64_t tmp;
          fill_value(data_decode, offset, large, tmp);

          fill_value(data_decode, key_len, false, tmp);
          offset += key_len;
        }

        auto* all_entry_size = static_cast<char*>(malloc(object_size * value_size));
        memset(all_entry_size, 0, object_size * value_size);
        data_decode.push_back(all_entry_size, object_size * value_size);

        for (auto& member : value.GetObject()) {
          const auto& key = member.name.GetString();
          size_t key_len = strlen(key);
          auto* key_value = static_cast<char*>(malloc(key_len + 1));
          strcpy(key_value, key);
          data_decode.push_back(key_value, key_len);
        }

        int64_t object_index = 0;
        for (auto& member : value.GetObject()) {
          const auto& member_value = member.value;
          append_value(
              data_decode, depth, parent, offset, large, member_value, all_entry_size + object_index * value_size);
          object_index++;
        }

        // Fill the number of bytes occupied by the object as a whole
        int64_t object_byte_size = (header == nullptr) ? (data_decode.byte_size() - 1) : data_decode.byte_size();
        if (large) {
          logproxy::int4store(reinterpret_cast<unsigned char*>(object_bytes), object_byte_size);
        } else {
          logproxy::int2store(reinterpret_cast<unsigned char*>(object_bytes), object_byte_size);
        }
        break;
      }
      case rapidjson::kArrayType: {
        auto array_size = value.GetArray().Size();
        bool large = is_large_json(array_size);
        if (header != nullptr) {
          if (large) {
            header[0] = JSONB_TYPE_LARGE_ARRAY;
          } else {
            header[0] = JSONB_TYPE_SMALL_ARRAY;
          }
        } else {
          auto* json_type = static_cast<char*>(malloc(1));
          if (large) {
            json_type[0] = JSONB_TYPE_LARGE_ARRAY;
          } else {
            json_type[0] = JSONB_TYPE_SMALL_ARRAY;
          }
          data_decode.push_back(json_type, 1);
          //          offset += 1;
        }

        fill_value(data_decode, array_size, large, offset);

        // fill json array bytes,this is a pre-occupied place, and finally filled with real data
        char* array_bytes = fill_value(data_decode, 0, large, offset);

        auto entry_size = large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL;
        auto* all_entry_size = static_cast<char*>(malloc(array_size * entry_size));
        memset(all_entry_size, 0, array_size * entry_size);
        data_decode.push_back(all_entry_size, array_size * entry_size);
        offset += array_size * entry_size;

        int64_t array_index = 0;
        for (auto& val : value.GetArray()) {
          if (depth >= JSON_DOCUMENT_MAX_DEPTH) {
            OMS_STREAM_ERROR << "Maximum depth of json object exceeded " << JSON_DOCUMENT_MAX_DEPTH;
            return offset;
          }
          append_value(data_decode, depth, parent, offset, large, val, all_entry_size + array_index * entry_size);
          array_index++;
        }
        // Fill the number of bytes occupied by the object as a whole
        int64_t array_byte_size = (header == nullptr) ? (data_decode.byte_size() - 1) : data_decode.byte_size();
        if (large) {
          logproxy::int4store(reinterpret_cast<unsigned char*>(array_bytes), array_byte_size);
        } else {
          logproxy::int2store(reinterpret_cast<unsigned char*>(array_bytes), array_byte_size);
        }

        break;
      }
      case rapidjson::kStringType: {
        if (header != nullptr) {
          header[0] = JSONB_TYPE_STRING;
        } else {
          auto* json_type = static_cast<char*>(malloc(1));
          json_type[0] = JSONB_TYPE_STRING;
          data_decode.push_back(json_type, 1);
        }

        size_t str_size = value.GetStringLength();
        append_variable_length(data_decode, str_size, offset);

        char* str_value = static_cast<char*>(malloc(value.GetStringLength()));
        memcpy(str_value, value.GetString(), value.GetStringLength());
        data_decode.push_back(str_value, str_size);
        offset += str_size;
        break;
      }
      case rapidjson::kNumberType:
        if (value.IsInt()) {
          if (is_int16(value.GetInt())) {
            if (header != nullptr) {
              header[0] = JSONB_TYPE_INT16;
            } else {
              auto* json_type = static_cast<char*>(malloc(1));
              json_type[0] = JSONB_TYPE_INT16;
              data_decode.push_back(json_type, 1);
            }

            auto* val = static_cast<char*>(malloc(2));
            logproxy::int2store(reinterpret_cast<unsigned char*>(val), value.GetInt());
            data_decode.push_back(val, 2);
            offset += 2;
            break;
          }

          if (header != nullptr) {
            header[0] = JSONB_TYPE_INT32;
          } else {
            auto* json_type = static_cast<char*>(malloc(1));
            json_type[0] = JSONB_TYPE_INT32;
            data_decode.push_back(json_type, 1);
          }
          auto* val = static_cast<char*>(malloc(4));
          logproxy::int4store(reinterpret_cast<unsigned char*>(val), value.GetInt());
          data_decode.push_back(val, 4);
          offset += 4;
          break;

        } else if (value.IsInt64()) {

          if (header != nullptr) {
            header[0] = JSONB_TYPE_INT64;
          } else {
            auto* json_type = static_cast<char*>(malloc(1));
            json_type[0] = JSONB_TYPE_INT64;
            data_decode.push_back(json_type, 1);
          }

          auto* val = static_cast<char*>(malloc(8));
          logproxy::int8store(reinterpret_cast<unsigned char*>(val), value.GetInt64());
          data_decode.push_back(val, 8);
          offset += 8;
        } else if (value.IsUint()) {
          if (is_uint16(value.GetUint())) {

            if (header != nullptr) {
              header[0] = JSONB_TYPE_UINT16;
            } else {
              auto* json_type = static_cast<char*>(malloc(1));
              json_type[0] = JSONB_TYPE_UINT16;
              data_decode.push_back(json_type, 1);
            }
            auto* val = static_cast<char*>(malloc(2));
            logproxy::int2store(reinterpret_cast<unsigned char*>(val), value.GetUint());
            data_decode.push_back(val, 2);
            offset += 2;
            break;
          }

          if (header != nullptr) {
            header[0] = JSONB_TYPE_UINT32;
          } else {
            auto* json_type = static_cast<char*>(malloc(1));
            json_type[0] = JSONB_TYPE_UINT32;
            data_decode.push_back(json_type, 1);
          }

          auto* val = static_cast<char*>(malloc(4));
          logproxy::int4store(reinterpret_cast<unsigned char*>(val), value.GetUint());
          data_decode.push_back(val, 4);
          offset += 4;
          break;
        } else if (value.IsUint64()) {

          if (header != nullptr) {
            header[0] = JSONB_TYPE_UINT64;
          } else {
            auto* json_type = static_cast<char*>(malloc(1));
            json_type[0] = JSONB_TYPE_UINT64;
            data_decode.push_back(json_type, 1);
          }

          auto* val = static_cast<char*>(malloc(8));
          logproxy::int8store(reinterpret_cast<unsigned char*>(val), value.GetUint64());
          data_decode.push_back(val, 8);
          offset += 8;
        } else if (value.IsDouble()) {

          if (header != nullptr) {
            header[0] = JSONB_TYPE_DOUBLE;
          } else {
            auto* json_type = static_cast<char*>(malloc(1));
            json_type[0] = JSONB_TYPE_DOUBLE;
            data_decode.push_back(json_type, 1);
          }

          auto* val = static_cast<char*>(malloc(8));
          logproxy::float8store(reinterpret_cast<unsigned char*>(val), value.GetDouble());
          data_decode.push_back(val, 8);
          offset += 8;
        }
        break;
    }
    return offset;
  }

  static void parser(char* data, logproxy::MsgBuf& data_decode)
  {
    rapidjson::Document document;
    document.Parse(data);
    parse_nested_object(document, data_decode, 1, nullptr);
  }
};

}  // namespace binlog
}  // namespace oceanbase