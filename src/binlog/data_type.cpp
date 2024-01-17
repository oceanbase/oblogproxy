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

#include <cassert>
#include <bitset>
#include "log.h"
#include "common.h"
#include "data_type.h"

namespace oceanbase {
namespace logproxy {

int set_column_metadata(unsigned char* begin, IColMeta& col_meta, std::string table_name)
{
  long col_len = col_meta.getLength();
  switch (col_meta.getType()) {
    case OB_TYPE_BIT:
      int1store(begin, col_meta.getPrecision() % 8);
      int1store(begin + 1, col_meta.getPrecision() / 8);
      return 2;
    case OB_TYPE_FLOAT:
      /*!
       * @brief https://dev.mysql.com/doc/refman/8.0/en/floating-point-types.html
       * According to the rules, it can be determined that when the precision is greater than or equal to 24,
       * mysql actually uses double to store data, and the expression in binlog is also double
       */
      if (col_meta.getPrecision() > 24) {
        int1store(begin, sizeof(double));
        return 1;
      }
      int1store(begin, sizeof(float));
      return 1;
    case OB_TYPE_DOUBLE:
      int1store(begin, sizeof(double));
      return 1;
    case OB_TYPE_STRING:
      col_len = col_len * charset_encoding_bytes(col_meta.getEncoding(), table_name, col_meta.getName());
      *begin = (col_meta.getType() ^ ((col_len & 0x300) >> 4));
      *(begin + 1) = col_len;
      return 2;
    case OB_TYPE_VAR_STRING:
      int2store(begin, col_len * charset_encoding_bytes(col_meta.getEncoding(), table_name, col_meta.getName()));
      return 2;
    case OB_TYPE_DECIMAL:
    case OB_TYPE_NEWDECIMAL:
      assert(col_meta.getScale() >= 0 && col_meta.getPrecision() >= 0);
      int1store(begin, col_meta.getPrecision());
      int1store(begin + 1, col_meta.getScale());
      return 2;
    case OB_TYPE_ENUM: {
      *begin = OB_TYPE_ENUM;
      int len;
      if (col_meta.getValuesOfEnumSet()->size() > 255) {
        len = 2;
      } else {
        len = 1;
      }
      *(begin + 1) = len;
      return 2;
    }
    case OB_TYPE_SET: {
      *begin = OB_TYPE_SET;
      int len;
      if (col_meta.getValuesOfEnumSet()->size() > 255) {
        len = 2;
      } else {
        len = 1;
      }
      *(begin + 1) = len;
      return 2;
    }
    case OB_TYPE_TINY_BLOB:
      *begin = 1;
      return 1;
    case OB_TYPE_MEDIUM_BLOB:
      *begin = 3;
      return 1;
    case OB_TYPE_JSON:
    case OB_TYPE_LONG_BLOB:
    case OB_TYPE_GEOMETRY:
      *begin = 4;
      return 1;
    case OB_TYPE_BLOB:
      // len of blob,The number of bytes used to represent the length of the blob
      *begin = 2;
      return 1;
    case OB_TYPE_VARCHAR:
      // default utf-8
      int2store(begin, col_len * charset_encoding_bytes(col_meta.getEncoding(), table_name, col_meta.getName()));
      return 2;

    case OB_TYPE_TIMESTAMP:
    case OB_TYPE_DATETIME:
    case OB_TYPE_TIME: {
      *begin = col_meta.getScale();
      return 1;
    }
    case OB_TYPE_TINY:
    case OB_TYPE_NULL:
    case OB_TYPE_LONGLONG:
    case OB_TYPE_INT24:
    case OB_TYPE_DATE:
    case OB_TYPE_YEAR:
    case OB_TYPE_NEWDATE:
    case OB_TYPE_SHORT:
    case OB_TYPE_LONG:
      // no metadata
      return 0;
    default:
      OMS_STREAM_ERROR << "Unsupported data type:" << col_meta.getType();
      return 0;
  }
}

int remainder_bytes(int remainder)
{
  assert(remainder >= 0 && remainder <= 9);
  switch (remainder) {
    case 0:
      return 0;
    case 1:
    case 2:
      return 1;
    case 3:
    case 4:
      return 2;
    case 5:
    case 6:
      return 3;
    case 7:
    case 8:
      return 4;
    default:
      return -1;
  }
}

/*!
* @brief
* +----------+---------------------------------+---------------------+--------+
| Charset  | Description                     | Default collation   | Maxlen |
+----------+---------------------------------+---------------------+--------+
| big5     | Big5 Traditional Chinese        | big5_chinese_ci     |      2 |
| dec8     | DEC West European               | dec8_swedish_ci     |      1 |
| cp850    | DOS West European               | cp850_general_ci    |      1 |
| hp8      | HP West European                | hp8_english_ci      |      1 |
| koi8r    | KOI8-R Relcom Russian           | koi8r_general_ci    |      1 |
| latin1   | cp1252 West European            | latin1_swedish_ci   |      1 |
| latin2   | ISO 8859-2 Central European     | latin2_general_ci   |      1 |
| swe7     | 7bit Swedish                    | swe7_swedish_ci     |      1 |
| ascii    | US ASCII                        | ascii_general_ci    |      1 |
| ujis     | EUC-JP Japanese                 | ujis_japanese_ci    |      3 |
| sjis     | Shift-JIS Japanese              | sjis_japanese_ci    |      2 |
| hebrew   | ISO 8859-8 Hebrew               | hebrew_general_ci   |      1 |
| tis620   | TIS620 Thai                     | tis620_thai_ci      |      1 |
| euckr    | EUC-KR Korean                   | euckr_korean_ci     |      2 |
| koi8u    | KOI8-U Ukrainian                | koi8u_general_ci    |      1 |
| gb2312   | GB2312 Simplified Chinese       | gb2312_chinese_ci   |      2 |
| greek    | ISO 8859-7 Greek                | greek_general_ci    |      1 |
| cp1250   | Windows Central European        | cp1250_general_ci   |      1 |
| gbk      | GBK Simplified Chinese          | gbk_chinese_ci      |      2 |
| latin5   | ISO 8859-9 Turkish              | latin5_turkish_ci   |      1 |
| armscii8 | ARMSCII-8 Armenian              | armscii8_general_ci |      1 |
| utf8     | UTF-8 Unicode                   | utf8_general_ci     |      3 |
| ucs2     | UCS-2 Unicode                   | ucs2_general_ci     |      2 |
| cp866    | DOS Russian                     | cp866_general_ci    |      1 |
| keybcs2  | DOS Kamenicky Czech-Slovak      | keybcs2_general_ci  |      1 |
| macce    | Mac Central European            | macce_general_ci    |      1 |
| macroman | Mac West European               | macroman_general_ci |      1 |
| cp852    | DOS Central European            | cp852_general_ci    |      1 |
| latin7   | ISO 8859-13 Baltic              | latin7_general_ci   |      1 |
| utf8mb4  | UTF-8 Unicode                   | utf8mb4_general_ci  |      4 |
| cp1251   | Windows Cyrillic                | cp1251_general_ci   |      1 |
| utf16    | UTF-16 Unicode                  | utf16_general_ci    |      4 |
| utf16le  | UTF-16LE Unicode                | utf16le_general_ci  |      4 |
| cp1256   | Windows Arabic                  | cp1256_general_ci   |      1 |
| cp1257   | Windows Baltic                  | cp1257_general_ci   |      1 |
| utf32    | UTF-32 Unicode                  | utf32_general_ci    |      4 |
| binary   | Binary pseudo charset           | binary              |      1 |
| geostd8  | GEOSTD8 Georgian                | geostd8_general_ci  |      1 |
| cp932    | SJIS for Windows Japanese       | cp932_japanese_ci   |      2 |
| eucjpms  | UJIS for Windows Japanese       | eucjpms_japanese_ci |      3 |
| gb18030  | China National Standard GB18030 | gb18030_chinese_ci  |      4 |
+----------+---------------------------------+---------------------+--------+
* @param charset
* @return
*/

int charset_encoding_bytes(std::string charset, std::string table_name, std::string col_name)
{
  // OB default character set is utf8mb4
  if (charset.empty()) {
    return 4;
  }

  if (ANY_STRING_EQUAL(charset.c_str(),
          "dec8",
          "cp850",
          "hp8",
          "koi8r",
          "latin1",
          "latin2",
          "swe7",
          "ascii",
          "hebrew",
          "tis620",
          "koi8u",
          "greek",
          "cp1250",
          "latin5",
          "armscii8",
          "cp866",
          "keybcs2",
          "macce",
          "macroman",
          "cp852",
          "latin7",
          "cp1251",
          "cp1256",
          "cp1257",
          "binary",
          "geostd8")) {
    return 1;
  }

  if (ANY_STRING_EQUAL(charset.c_str(), "big5", "sjis", "euckr", "gb2312", "gbk", "ucs2", "cp932")) {
    return 2;
  }

  if (ANY_STRING_EQUAL(charset.c_str(), "ujis", "utf8", "eucjpms")) {
    return 3;
  }

  if (ANY_STRING_EQUAL(charset.c_str(), "utf8mb4", "utf16", "utf16le", "utf32", "gb18030")) {
    return 4;
  }

  // It is not expected to go here. If it is a non-mysql character set, it is currently expressed with a maximum length
  // of 4
  OMS_WARN("non-mysql character set:{},table:{},column:{}", charset, table_name, col_name);
  return 4;
}

size_t year_gap(size_t real_year)
{
  return real_year - 1900;
}

size_t binary_to_hex(const std::string& binary, char* buff, int len)
{
  size_t i = 0;
  size_t index = 0;
  std::bitset<64> bit_max(atoi(binary.c_str()));
  std::string real = bit_max.to_string().substr(64 - len * 8, len * 8);
  for (; i < real.size(); i += 8) {
    std::bitset<8> bit_set{real.substr(i, 8)};
    int1store(reinterpret_cast<unsigned char*>(buff + i / 8), bit_set.to_ulong());
    index += 8;
  }
  if (index <= real.size()) {
    std::bitset<8> bit_set{real.substr(index, real.size() - index)};
    int1store(reinterpret_cast<unsigned char*>(buff + i / 8), bit_set.to_ulong());
  }
  return len;
}

/*!
 * \brief
 * \param number target value
 * \param target_length target value expected numeric length of clipping and complementing
 * \return
 */
size_t cutout_or_pad_zero(size_t number, int target_length)
{
  assert(number >= 0);
  // Calculate the current length of a number
  int length = get_number_len(number);

  if (length <= target_length) {
    // If the current length is less than the target length, pad with zeros
    while (length < target_length) {
      number *= 10;  // Fill in zeros by multiplying by 10
      length++;
    }

    // If the current length is greater than the target length, zero padding cannot be performed and the original number
    // is returned.
    return number;
  } else {
    while (length > target_length) {
      number /= 10;  // Divide the value by 10 and shift it right
      length--;
    }

    // If the current length is greater than the target length, zero padding cannot be performed and the original number
    // is returned.
    return number;
  }
}

int get_number_len(size_t number)
{
  int length = number == 0 ? 1 : std::log10(number) + 1;
  return length;
}

size_t get_column_val_bytes(
    IColMeta& col_meta, size_t data_len, char* data, MsgBuf& data_decode, std::string table_name)
{
  size_t col_len = col_meta.getLength();
  switch (col_meta.getType()) {
    case OB_TYPE_BIT: {
      return convert_binlog_bit(col_meta, data_len, data, data_decode, col_len);
    }
    case OB_TYPE_FLOAT: {
      return convert_binlog_float(col_meta, data, data_decode);
    }

    case OB_TYPE_DOUBLE: {
      return convert_binlog_double(data, data_decode);
    }

    case OB_TYPE_STRING:
    case OB_TYPE_VAR_STRING:
    case OB_TYPE_VARCHAR: {
      return convert_binlog_var_string(col_meta, data_len, data, data_decode, table_name, col_len);
    }
    case OB_TYPE_DECIMAL:
    case OB_TYPE_NEWDECIMAL: {
      return convert_binlog_decimal(col_meta, data_len, data, data_decode);
    }
    case OB_TYPE_ENUM: {
      return convert_binlog_enum(col_meta, data, data_decode);
    }

    case OB_TYPE_SET: {
      return convert_binlog_set(col_meta, data, data_decode);
    }
    case OB_TYPE_TINY_BLOB: {
      return convert_tiny_blob(data_len, data, data_decode);
    }

    case OB_TYPE_MEDIUM_BLOB: {
      return convert_binlog_medium_blob(data_len, data, data_decode);
    }

    case OB_TYPE_LONG_BLOB: {
      return convert_binlog_longblob(data_len, data, data_decode);
    }

    case OB_TYPE_BLOB: {
      return convert_binlog_blob(data_len, data, data_decode);
    }

    case OB_TYPE_TINY: {
      return convert_binlog_tiny(data, data_decode);
    }

    case OB_TYPE_NULL:
      return 1;
    case OB_TYPE_TIMESTAMP: {
      // MYSQL_TYPE_TIMESTAMP2 and MYSQL_TYPE_TIMESTAMP
      return convert_binlog_timestamp(col_meta, data_len, data, data_decode);
    }

    case OB_TYPE_LONGLONG: {
      return convert_binlog_longlong(data, data_decode);
    }

    case OB_TYPE_INT24: {
      return convert_binlog_int24(data, data_decode);
    }

    case OB_TYPE_DATE:
    case OB_TYPE_NEWDATE: {
      return convert_binlog_date(data_len, data, data_decode);
    }
    case OB_TYPE_TIME: {
      return convert_binlog_time(col_meta, data, data_decode);
    }

    case OB_TYPE_DATETIME: {
      return convert_binlog_datetime(col_meta, data_len, data, data_decode);
    }

    case OB_TYPE_YEAR: {
      return convert_binlog_year(data_len, data, data_decode);
    }
    case OB_TYPE_SHORT: {
      return convert_binlog_short(data, data_decode);
    }

    case OB_TYPE_LONG: {
      return convert_binlog_long(data, data_decode);
    }
    case OB_TYPE_JSON: {
      return convert_binlog_json(data, data_decode);
    }
    case OB_TYPE_GEOMETRY: {
      return convert_binlog_geometry(data_len, data, data_decode);
    }

    default:
      OMS_STREAM_ERROR << "Unsupported data type:" << col_meta.getType();
      return 0;
  }
}

size_t convert_binlog_bit(IColMeta& col_meta, size_t data_len, const char* data, MsgBuf& data_decode, size_t col_len)
{
  col_len = col_meta.getPrecision();
  if (col_len <= 0) {
    col_len = data_len;
  }

  auto* buff = static_cast<char*>(malloc((col_len + 7) / 8));
  size_t ret = binary_to_hex(data, buff, (col_len + 7) / 8);
  data_decode.push_back(buff, ret);
  assert(ret == (col_len + 7) / 8);
  return ret;
}

size_t convert_binlog_float(IColMeta& col_meta, const char* data, MsgBuf& data_decode)
{
  /*!
   * @brief https://dev.mysql.com/doc/refman/8.0/en/floating-point-types.html
   * According to the rules, it can be determined that when the precision is greater than or equal to 24,
   * mysql actually uses double to store data, and the expression in binlog is also double
   */
  if (col_meta.getPrecision() > 24) {
    double value = std::stod(data);
    char* buff = reinterpret_cast<char*>(&value);
    data_decode.push_back_copy(buff, sizeof(double));
    return sizeof(double);
  }

  float value = (float)std::stod(data);
  char* buff = reinterpret_cast<char*>(&value);
  data_decode.push_back_copy(buff, sizeof(float));
  return sizeof(float);
}

size_t convert_binlog_double(const char* data, MsgBuf& data_decode)
{
  double value = std::stod(data);
  char* buff = reinterpret_cast<char*>(&value);
  data_decode.push_back_copy(buff, sizeof(double));
  return sizeof(double);
}

size_t convert_binlog_var_string(
    IColMeta& col_meta, size_t data_len, const char* data, MsgBuf& data_decode, std::string& table_name, size_t col_len)
{
  size_t ret = 0;
  char* buff;
  // The encoding of DB is set to empty, because there is currently an ambiguity, it can be either database or
  // tenant to avoid ambiguity, it is set to empty here
  col_len *= charset_encoding_bytes(col_meta.getEncoding(), table_name, col_meta.getName());

  if (col_len > 255) {
    ret = 2;
    buff = static_cast<char*>(malloc(ret + data_len));
    int2store(reinterpret_cast<unsigned char*>(buff), data_len);
  } else {
    ret = 1;
    buff = static_cast<char*>(malloc(ret + data_len));
    int1store(reinterpret_cast<unsigned char*>(buff), data_len);
  }

  memcpy(buff + ret, data, data_len);
  data_decode.push_back(buff, ret + data_len);
  return ret + data_len;
}

size_t convert_binlog_decimal(IColMeta& col_meta, size_t data_len, const char* data, MsgBuf& data_decode)
{
  // 1. get decimal and precision
  int precision = 10;
  int frac = 0;
  int frac_max = 0;

  if (col_meta.getScale() >= 0) {
    frac = col_meta.getScale();
    frac_max = frac;
  }

  if (col_meta.getPrecision() >= 0) {
    precision = col_meta.getPrecision();
  }
  int32_t mask = (data[0] == '-' ? -1 : 0);

  // sign bit occupied
  int sign_size = 0;
  if (mask < 0) {
    sign_size = 1;
  }

  int intg_max = precision - frac_max;

  // Integer part digits
  int intg = intg_max;

  // find .
  int index = index_of(data, '.');
  if (index > 0) {
    intg = (index - sign_size);
  } else {
    intg = (data_len - sign_size);
  }

  frac = data_len - (intg + sign_size) - ((index > 0) ? 1 : 0);

  // The integer part satisfies the number of 9 digits
  int intg0 = intg / DECODE_BASE_LEN;
  int intg_max0 = intg_max / DECODE_BASE_LEN;

  // The number of decimal parts satisfying 9 digits
  int frac0 = frac / DECODE_BASE_LEN;
  int frac_max0 = frac_max / DECODE_BASE_LEN;

  // The integer part does not satisfy the number of 9 digits
  int intg0x = intg - intg0 * DECODE_BASE_LEN;
  int intg_max0x = intg_max - intg_max0 * DECODE_BASE_LEN;

  // The decimal part does not satisfy 9 digits
  int frac0x = frac - frac0 * DECODE_BASE_LEN;
  int frac_max0x = frac_max - frac_max0 * DECODE_BASE_LEN;
  // The total number of bytes occupied by the integer part
  int isize0 = intg0 * DECIMAL_STORE_BASE_LEN + dig2bytes[intg0x];
  int isize_max0 = intg_max0 * DECIMAL_STORE_BASE_LEN + dig2bytes[intg_max0x];
  // The total number of bytes occupied by the fractional part
  int fsize0 = frac0 * DECIMAL_STORE_BASE_LEN + dig2bytes[frac0x];
  int fsize_max0 = frac_max0 * DECIMAL_STORE_BASE_LEN + dig2bytes[frac_max0x];

  const int orig_isize0 = isize_max0;
  const int orig_fsize0 = fsize_max0;

  auto* data_buff = static_cast<unsigned char*>(malloc(orig_isize0 + orig_fsize0));
  int offset = 0;

  if (isize_max0 > isize0) {
    while (isize_max0-- > isize0) {
      *(data_buff + offset) = (char)mask;
      offset++;
    }
  }

  if (fsize_max0 > fsize0 && frac0x) {
    if (frac0 == frac_max0) {
      frac0x = frac_max0x;
      fsize_max0 = fsize0;
    } else {
      frac0++;
      frac0x = 0;
    }
  }

  if (intg0x > 0) {
    std::string intg0x_str(data + sign_size, intg0x);
    int bytes_size = dig2bytes[intg0x];
    int32_t value = (atoi(intg0x_str.c_str()) % powers10[intg0x]) ^ mask;
    switch (bytes_size) {
      case 1:
        hf_int1store(data_buff + offset, value);
        break;
      case 2:
        hf_int2store(data_buff + offset, value);
        break;
      case 3:
        hf_int3store(data_buff + offset, value);
        break;
      case 4:
        hf_int4store(data_buff + offset, value);
        break;
      default:
        OMS_STREAM_ERROR << "Unexpected value";
        break;
    }
    offset += bytes_size;
  }

  int from = intg0x + sign_size;

  for (int i = 0; i < intg0; ++i) {
    std::string intg0_str(data + from, DECODE_BASE_LEN);
    from += DECODE_BASE_LEN;
    int32_t value = (atoi(intg0_str.c_str())) ^ mask;
    hf_int4store(data_buff + offset, value);
    offset += DECIMAL_STORE_BASE_LEN;
  }

  // skip .
  from++;

  for (int i = 0; i < frac0; i++) {
    std::string frac0_str(data + from, DECODE_BASE_LEN);
    from += DECODE_BASE_LEN;
    int32_t value = (atoi(frac0_str.c_str())) ^ mask;
    hf_int4store(data_buff + offset, value);
    offset += DECIMAL_STORE_BASE_LEN;
  }

  if (frac0x > 0) {
    std::string frac_str(data + from, frac0x);

    int bytes_size = dig2bytes[frac0x];
    int lim = (frac0 < frac_max / DECODE_BASE_LEN ? DECODE_BASE_LEN
                                                  : (frac_max - (frac_max / DECODE_BASE_LEN) * DECODE_BASE_LEN));

    while (frac0x < lim && dig2bytes[frac0x] == bytes_size) {
      frac0x++;
    }

    int32_t value = (atoi(frac_str.c_str())) ^ mask;
    switch (bytes_size) {
      case 1:
        hf_int1store(data_buff + offset, value);
        break;
      case 2:
        hf_int2store(data_buff + offset, value);
        break;
      case 3:
        hf_int3store(data_buff + offset, value);
        break;
      case 4:
        hf_int4store(data_buff + offset, value);
        break;
      default:
        OMS_STREAM_ERROR << "Unexpected value";
        break;
    }
    offset += bytes_size;
  }

  if (fsize_max0 > fsize0) {
    while (fsize_max0-- > fsize0 && offset < orig_isize0 + orig_fsize0) {
      *(data_buff + offset) = (unsigned char)mask;
      offset++;
    }
  }

  data_buff[0] ^= 0x80;
  data_decode.push_back(reinterpret_cast<char*>(data_buff), orig_isize0 + orig_fsize0);
  return orig_isize0 + orig_fsize0;
}

size_t convert_binlog_enum(IColMeta& col_meta, const char* data, MsgBuf& data_decode)
{
  StrArray* array = col_meta.getValuesOfEnumSet();
  const char* enum_val;
  size_t len;
  int ret = 0;
  unsigned char* buff = nullptr;
  for (size_t i = 0; i < array->size(); ++i) {
    array->elementAt(i, enum_val, len);
    if (memcmp(enum_val, data, len) == 0) {
      if (array->size() < 256) {
        ret = 1;
        buff = static_cast<unsigned char*>(malloc(ret));
        int1store(buff, i + 1);
      } else {
        ret = 2;
        buff = static_cast<unsigned char*>(malloc(ret));
        int2store(buff, i + 1);
      }
      break;
    }
  }
  data_decode.push_back(reinterpret_cast<char*>(buff), ret);
  return ret;
}

size_t convert_tiny_blob(size_t data_len, const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(data_len + 1));
  int1store(buff, data_len);
  memcpy(buff + 1, data, data_len);
  data_decode.push_back(reinterpret_cast<char*>(buff), data_len + 1);
  return 1 + data_len;
}

size_t convert_binlog_set(IColMeta& col_meta, const char* data, MsgBuf& data_decode)
{
  StrArray* array = col_meta.getValuesOfEnumSet();
  const char* set_val;
  size_t len;
  std::vector<std::string> set;
  split(data, ',', set);
  uint64_t bitmap_len = (array->size() + 7) / 8;

  std::bitset<64> bitmap;
  bitmap.reset();
  for (size_t i = 0; i < array->size(); ++i) {
    array->elementAt(i, set_val, len);
    for (const std::string& val : set) {
      if (memcmp(set_val, val.c_str(), len) == 0) {
        bitmap.set(i);
        break;
      }
    }
  }
  auto* buff = static_cast<unsigned char*>(malloc(bitmap_len));
  std::string real = bitmap.to_string().substr(64 - bitmap_len * 8, bitmap_len * 8);
  for (size_t i = 0; i < real.size(); i += 8) {
    std::bitset<8> bit_set{real.substr(i, 8)};
    int1store(reinterpret_cast<unsigned char*>(buff + i / 8), bit_set.to_ulong());
  }
  data_decode.push_back(reinterpret_cast<char*>(buff), bitmap_len);
  return bitmap_len;
}

size_t convert_binlog_medium_blob(size_t data_len, const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(data_len + 3));
  int3store(buff, data_len);
  memcpy(buff + 3, data, data_len);
  data_decode.push_back(reinterpret_cast<char*>(buff), data_len + 3);
  return 3 + data_len;
}

size_t convert_binlog_longblob(size_t data_len, const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(data_len + 4));
  int4store(buff, data_len);
  memcpy(buff + 4, data, data_len);
  data_decode.push_back(reinterpret_cast<char*>(buff), data_len + 4);
  return 4 + data_len;
}

size_t convert_binlog_blob(size_t data_len, const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(data_len + 2));
  // len of blob,The number of bytes used to represent the length of the blob
  int2store(buff, data_len);
  memcpy(buff + 2, data, data_len);
  data_decode.push_back(reinterpret_cast<char*>(buff), data_len + 2);
  return 2 + data_len;
}

size_t convert_binlog_tiny(const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(1));
  int_two_complement(buff, 1, data);
  data_decode.push_back(reinterpret_cast<char*>(buff), 1);
  return 1;
}

size_t convert_binlog_timestamp(IColMeta& col_meta, size_t data_len, const char* data, MsgBuf& data_decode)
{
  // set enable_convert_timestamp_to_unix_timestamp=1，1662034855.000000
  std::string str(data, data_len);
  IUnixTime unix_time = str_2_unix_time(str);
  int precision = col_meta.getScale();
  int64_t buff_len = 4 + remainder_bytes(precision);
  auto* buff = static_cast<unsigned char*>(malloc(buff_len));
  int pos = 0;
  be_int4store(buff + pos, unix_time.sec);
  pos += 4;
  // This is because obcdc will always output a timestamp with a precision of 6 no matter what the precision is
  switch (precision) {
    case 0:
      break;
    case 1:
    case 2:
      int1store(buff + pos, cutout_or_pad_zero(unix_time.us, 2 - unix_time.prefix_zero_num));
      break;
    case 3:
    case 4:
      be_int2store(buff + pos, cutout_or_pad_zero(unix_time.us, 4 - unix_time.prefix_zero_num));
      break;
    case 5:
    case 6: {
      be_int3store(buff + pos, cutout_or_pad_zero(unix_time.us, 6 - unix_time.prefix_zero_num));
      break;
    }
    default:
      OMS_ERROR("Unexpected value :{}", str);
      break;
  }
  data_decode.push_back(reinterpret_cast<char*>(buff), buff_len);
  return 4 + remainder_bytes(precision);
}

size_t convert_binlog_longlong(const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(8));
  int_two_complement(buff, 8, data);
  data_decode.push_back(reinterpret_cast<char*>(buff), 8);
  return 8;
}

size_t convert_binlog_int24(const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(3));
  int_two_complement(buff, 3, data);
  data_decode.push_back(reinterpret_cast<char*>(buff), 3);
  return 3;
}

size_t convert_binlog_geometry(size_t data_len, const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(data_len + 4));
  int4store(buff, data_len);
  memcpy(buff + 4, data, data_len);
  data_decode.push_back(reinterpret_cast<char*>(buff), data_len + 4);
  return 4 + data_len;
}

size_t convert_binlog_json(char* data, MsgBuf& data_decode)
{
  MsgBuf json;
  binlog::JsonParser::parser(data, json);
  auto byte_size = json.byte_size();
  char* temp = static_cast<char*>(malloc(byte_size));
  json.bytes(temp);
  auto* len = static_cast<unsigned char*>(malloc(4));
  int4store(len, byte_size);
  data_decode.push_back(reinterpret_cast<char*>(len), 4);
  data_decode.push_back(temp, byte_size);
  return 4 + byte_size;
}

size_t convert_binlog_long(const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(4));
  int_two_complement(buff, 4, data);
  // no metadata
  data_decode.push_back(reinterpret_cast<char*>(buff), 4);
  return 4;
}

size_t convert_binlog_short(const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(2));
  int_two_complement(buff, 2, data);
  data_decode.push_back(reinterpret_cast<char*>(buff), 2);
  return 2;
}

size_t convert_binlog_year(size_t data_len, const char* data, MsgBuf& data_decode)
{
  std::string year_str(data, data_len);
  size_t real_year = atoi(year_str.c_str());
  assert(!(real_year < 0 || real_year > 2155));
  auto* buff = static_cast<unsigned char*>(malloc(1));
  // Handle years like '0000'
  if (real_year == 0) {
    int1store(buff, real_year);
    data_decode.push_back(reinterpret_cast<char*>(buff), 1);
    return 1;
  }
  int1store(buff, year_gap(atoi(year_str.c_str())));
  data_decode.push_back(reinterpret_cast<char*>(buff), 1);
  return 1;
}

size_t convert_binlog_datetime(IColMeta& col_meta, size_t data_len, const char* data, MsgBuf& data_decode)
{
  std::string str(data, data_len);
  IDate date = str_2_idate(str);
  //      int precision = date.precision;
  int precision = col_meta.getScale();
  int64_t buff_len = 5 + remainder_bytes(precision);
  auto* buff = static_cast<unsigned char*>(malloc(buff_len));
  int pos = 0;
  uint64_t time = 0;
  time |= date.sign;
  time = ((time << 17) | (date.year * 13 + date.month)) << 5;
  time |= date.day;

  time <<= 5;
  time |= date.hour;

  time <<= 6;
  time |= date.minute;

  time <<= 6;
  time |= date.second;

  if (precision % 2 != 0) {
    date.mill_second *= 10;
  }

  switch (precision) {
    case 1:
    case 2:
      be_int5store(buff + pos, time + TIME_ZERO_FIVE);
      pos += 5;
      int1store(buff + pos, cutout_or_pad_zero(date.mill_second, 2 - date.prefix_zero_num));
      pos += 1;
      break;
    case 3:
    case 4:
      be_int5store(buff + pos, time + TIME_ZERO_FIVE);
      pos += 5;
      be_int2store(buff + pos, cutout_or_pad_zero(date.mill_second, 4 - date.prefix_zero_num));
      pos += 2;
      break;
    case 5:
    case 6:
      be_int5store(buff + pos, time + TIME_ZERO_FIVE);
      pos += 5;
      be_int3store(buff + pos, cutout_or_pad_zero(date.mill_second, 6 - date.prefix_zero_num));
      pos += 3;
      break;
    default:
      be_int5store(buff + pos, time + TIME_ZERO_FIVE);
      pos += 5;
  }
  data_decode.push_back(reinterpret_cast<char*>(buff), buff_len);

  return 5 + remainder_bytes(precision);
}

size_t convert_binlog_date(size_t data_len, const char* data, MsgBuf& data_decode)
{
  auto* buff = static_cast<unsigned char*>(malloc(3));
  std::string str(data, data_len);
  IDate i_date = str_2_idate(str);
  int64_t date = i_date.day + i_date.month * 32 + i_date.year * 16 * 32;
  int3store(buff, date);
  data_decode.push_back(reinterpret_cast<char*>(buff), 3);
  return 3;
}

size_t convert_binlog_time(IColMeta& col_meta, char* data, MsgBuf& data_decode)
{
  //  IDate date{};
  //  str_2_hhmmss(data, date);
  IDate date = str_2_idate(data);
  //      int precision = date.precision;
  int precision = col_meta.getScale();
  int64_t buff_len = 3 + remainder_bytes(precision);
  auto* buff = static_cast<unsigned char*>(malloc(buff_len));
  int pos = 0;
  int sign = 1;
  int64_t time = (((date.month > 0 ? 0 : date.day * 24L) + date.hour) << 12) | (date.minute << 6) | date.second;
  if (date.sign < 0) {
    time = date.mill_second == 0 ? (-time) : -(time + 1);
    sign = -1;
  }

  switch (precision) {
    case 1:
    case 2:
      be_int3store(buff + pos, TIME_ZERO_THREE + time);
      pos += 3;
      int1store(buff + pos, sign * cutout_or_pad_zero(date.mill_second, 2 - date.prefix_zero_num));
      pos += 1;
      break;
    case 3:
    case 4:
      be_int3store(buff + pos, TIME_ZERO_THREE + time);
      pos += 3;
      be_int2store(buff + pos, sign * cutout_or_pad_zero(date.mill_second, 4 - date.prefix_zero_num));
      pos += 2;
      break;
    case 5:
    case 6:
      be_int3store(buff + pos, TIME_ZERO_THREE + time);
      pos += 3;
      be_int3store(buff + pos, sign * cutout_or_pad_zero(date.mill_second, 6 - date.prefix_zero_num));
      pos += 3;
      break;
    default:
      be_int3store(buff + pos, TIME_ZERO_THREE + time);
      pos += 3;
  }
  data_decode.push_back(reinterpret_cast<char*>(buff), buff_len);
  return 3 + remainder_bytes(precision);
}

int get_packed_integer(size_t val)
{
  if (val < 251) {
    return 1;
  }
  if (val < (2 << 16)) {
    return 3;
  }
  if (val < (2 << 24)) {
    return 4;
  }
  return 9;
}

size_t int_two_complement(unsigned char* val, size_t len, const char* data)
{
  auto num = atoll(data);
  //  if (num < 0) {
  //    num += pow(2, len);
  //  }
  switch (len) {
    case 1:
      int1store(val, num);
      break;
    case 2:
      int2store(val, num);
      break;
    case 3:
      int3store(val, num);
      break;
    case 4:
      int4store(val, num);
      break;
    case 5:
      int5store(val, num);
      break;
    case 6:
      int6store(val, num);
      break;
    case 8:
      int8store(val, num);
      break;
    default:
      return 0;
  }
  return len;
}

size_t millis_to_seconds(int64_t ms)
{
  return ms / 1000;
}

size_t seconds_to_millis(int64_t s)
{
  return s * 1000;
}

void str_2_yyyymmdd(std::string str, IDate& date)
{
  std::vector<std::string> dates;
  split(str, '-', dates);
  assert(dates.size() == 3);
  date.year = atoi(dates.at(0).c_str());
  date.month = atoi(dates.at(1).c_str());
  date.day = atoi(dates.at(2).c_str());
}

void str_2_hhmmss(std::string str, IDate& date)
{
  if (str.empty()) {
    return;
  }
  std::vector<std::string> dates;
  split(str, '.', dates);

  assert(dates.size() > 0);

  std::string part = dates.at(0);
  std::vector<std::string> parts;
  split(part, ':', parts);
  assert(parts.size() == 3);
  date.hour = atoi(parts.at(0).c_str());
  date.minute = atoi(parts.at(1).c_str());
  date.second = atoi(parts.at(2).c_str());

  if (dates.size() == 2) {
    date.mill_second = atoi(dates.at(1).c_str());
    date.precision = dates.at(1).size();
    date.prefix_zero_num = date.precision - get_number_len(date.mill_second);
  }
}

IDate str_2_idate(std::string str)
{
  IDate date{};
  if (str.empty()) {
    return date;
  }
  if (str.at(0) == '-') {
    date.sign = -1;
    str = str.substr(1, str.size() - 1);
  }
  std::vector<std::string> dates;
  split(str, ' ', dates);
  if (dates.size() == 2) {
    // parse yyyymmdd
    str_2_yyyymmdd(dates.at(0), date);
    // parse hhmmss
    str_2_hhmmss(dates.at(1), date);
  } else if (str.find('-') != std::string::npos) {
    // parse yyyymmdd
    str_2_yyyymmdd(str, date);
  } else if (str.find(':') != std::string::npos) {
    // parse hhmmss
    str_2_hhmmss(str, date);
  }
  return date;
}

IUnixTime str_2_unix_time(const std::string& str)
{
  //  OMS_STREAM_DEBUG << "unix time str:" << str;
  IUnixTime unix_time = IUnixTime();

  if (str.empty()) {
    return unix_time;
  }
  std::vector<std::string> dates;
  split(str, '.', dates);
  unix_time.sec = atol(dates.at(0).c_str());
  if (dates.size() == 2) {
    unix_time.us = atol(dates.at(1).c_str());
    unix_time.precision = strlen(dates.at(1).c_str());
    // For the timestamp type, the data accuracy of the data spit out by obcdc defaults to TIMESTAMP (6).
    // Defenses need to be made here to prevent accuracy problems caused by changes in the accuracy of the data spit out
    // by OBCDC.
    unix_time.prefix_zero_num = unix_time.precision - get_number_len(unix_time.us);
  }
  return unix_time;
}

}  // namespace logproxy
}  // namespace oceanbase
