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

#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <map>
#include "common/common.h"

namespace oceanbase {
namespace logproxy {

class ConfigItemBase {
public:
  virtual void from_str(const std::string& val) = 0;

  virtual std::string debug_str() const = 0;
};

class ConfigBase {
public:
  void add_item(const std::string& key, ConfigItemBase* item);

protected:
  std::map<std::string, ConfigItemBase*> _configs;
};

template <typename T>
class ConfigItem : protected ConfigItemBase {
public:
  friend class Config;

  explicit ConfigItem(ConfigBase* config, const std::string& key, const T dft, bool optional = false)
      : _key(key), _optional(optional)
  {
    _val = dft;
    config->add_item(key, this);
  }

  const std::string& key() const
  {
    return _key;
  }

  void set(const T v)
  {
    _val = v;
  }

  T val() const
  {
    return _val;
  }

  bool empty() const
  {
    return empty_type(_val);
  }

  std::string debug_str() const override
  {
    return std::to_string(_val);
  }

private:
  void from_str(const std::string& val) override
  {
    _val = from_str(val, _val);
  }

  uint16_t from_str(const std::string& val, uint16_t v)
  {
    return atoi(val.c_str());
  }

  uint32_t from_str(const std::string& val, uint32_t v)
  {
    return atoi(val.c_str());
  }

  uint64_t from_str(const std::string& val, uint64_t v)
  {
    return strtoll(val.c_str(), nullptr, 10);
  }

  int16_t from_str(const std::string& val, int16_t v)
  {
    return atoi(val.c_str());
  }

  int32_t from_str(const std::string& val, int32_t v)
  {
    return atoi(val.c_str());
  }

  int64_t from_str(const std::string& val, int64_t v)
  {
    return strtoull(val.c_str(), nullptr, 10);
  }

  bool from_str(const std::string& val, bool v)
  {
    return strcasecmp("true", val.c_str()) == 0;
  }

  static bool empty_type(T v)
  {
    return false;
  }

private:
  std::string _key;
  T _val;
  bool _optional = false;
};

// specific for std::string
template <>
class ConfigItem<std::string> : protected ConfigItemBase {
public:
  friend class Config;

  explicit ConfigItem(ConfigBase* config, const std::string& key, const std::string& dft, bool optional = false)
      : _key(key), _optional(optional)
  {
    _val = dft;
    config->add_item(key, this);
  }

  const std::string& key() const
  {
    return _key;
  }

  void set(const std::string& v)
  {
    _val = v;
  }

  const std::string& val() const
  {
    return _val;
  }

  bool empty() const
  {
    return _val.empty();
  }

  std::string debug_str() const override
  {
    return _val;
  }

protected:
  void from_str(const std::string& val) override
  {
    _val = val;
  }

protected:
  std::string _key;
  std::string _val;
  bool _optional = false;
};

class EncryptedConfigItem : public ConfigItem<std::string> {
public:
  friend class Config;

  EncryptedConfigItem(ConfigBase* config, const std::string& key, const std::string& dft, bool optional = false,
      const std::string& encrypt_key = "")
      : ConfigItem<std::string>(config, key, dft, optional), _encrypt_key(encrypt_key)
  {
    _val = dft;
    config->add_item(key, this);
  }

  std::string debug_str() const override
  {
    return "******";
  }

private:
  void from_str(const std::string& val) override;

private:
  std::string _encrypt_key;
};

#define OMS_CONFIG_UINT16(key, args...) \
  ConfigItem<uint16_t> key              \
  {                                     \
    this, #key, ##args                  \
  }
#define OMS_CONFIG_UINT32(key, args...) \
  ConfigItem<uint32_t> key              \
  {                                     \
    this, #key, ##args                  \
  }
#define OMS_CONFIG_UINT64(key, args...) \
  ConfigItem<uint64_t> key              \
  {                                     \
    this, #key, ##args                  \
  }
#define OMS_CONFIG_UINT64_K(name, key, args...) \
  ConfigItem<uint64_t> name                     \
  {                                             \
    this, key, ##args                           \
  }
#define OMS_CONFIG_INT16(key, args...) \
  ConfigItem<int16_t> key              \
  {                                    \
    this, #key, ##args                 \
  }
#define OMS_CONFIG_INT32(key, args...) \
  ConfigItem<int32_t> key              \
  {                                    \
    this, #key, ##args                 \
  }
#define OMS_CONFIG_INT64(key, args...) \
  ConfigItem<int64_t> key              \
  {                                    \
    this, #key, ##args                 \
  }
#define OMS_CONFIG_BOOL(key, args...) \
  ConfigItem<bool> key                \
  {                                   \
    this, #key, ##args                \
  }

#define OMS_CONFIG_STR(key, args...) \
  ConfigItem<std::string> key        \
  {                                  \
    this, #key, ##args               \
  }

#define OMS_CONFIG_STR_K(name, key, args...) \
  ConfigItem<std::string> name               \
  {                                          \
    this, key, ##args                        \
  }

#define OMS_CONFIG_ENCRYPT(key, args...) \
  EncryptedConfigItem key                \
  {                                      \
    this, #key, ##args                   \
  }

}  // namespace logproxy
}  // namespace oceanbase
