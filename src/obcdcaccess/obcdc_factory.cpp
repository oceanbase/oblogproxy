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

#include "dlfcn.h"

#include "log.h"
#include "obaccess/ob_access.h"
#include "obcdc_factory.h"
#include "fs_util.h"
#include "str.h"

#define INCOMPATIBLE_VERSION "4.2.1"

namespace oceanbase {
namespace logproxy {

typedef IObCdcAccess* (*fn_create)(void*);
typedef void (*fn_destroy)(IObCdcAccess*);

int ObCdcAccessFactory::load(const OblogConfig& config, IObCdcAccess*& _obcdc)
{
  std::string ob_version = config.ob_version.val();
  if (ob_version.empty()) {
    ObAccess ob_access;
    int ret = ob_access.query_ob_version(config, ob_version);
    if (OMS_OK != ret) {
      OMS_ERROR("Failed to obtain the version of OB.");
      return ret;
    }
  }

  std::string obcdc_so_path;
  if (OMS_OK != locate_obcdc_library(ob_version, obcdc_so_path)) {
    OMS_FATAL("Failed to obtain the so library path of obcdc.");
    return OMS_FAILED;
  }

  OMS_INFO("Try to load the so library of obcdc, path: {}", obcdc_so_path);

  void* so_handle = dlopen(obcdc_so_path.c_str(), RTLD_LAZY);
  if (nullptr == so_handle) {
    OMS_FATAL("Failed to load so library: {}, error: {}", obcdc_so_path, dlerror());
    return OMS_FAILED;
  }

  auto create = (fn_create)dlsym(so_handle, "create");
  if (nullptr == create) {
    OMS_ERROR("Failed to find func [create], error: {}", dlerror());
    dlclose(so_handle);
    return OMS_FAILED;
  }

  _obcdc = create(so_handle);
  if (nullptr == _obcdc) {
    OMS_ERROR("Failed to call [create] func for obtaining the pointer of IObCdcAccess.");
    dlclose(so_handle);
    return OMS_FAILED;
  }

  OMS_INFO("Successfully load the pointer of IObCdcAccess from so library");
  return OMS_OK;
}

void ObCdcAccessFactory::unload(IObCdcAccess* obcdc)
{
  /** ！！！Don't release temporarily to prevent obcdc not existing ！！！**/
  /*
  if (nullptr != obcdc) {
    void* so_handle = obcdc->get_handle();
    if (nullptr != so_handle) {
      dlclose(so_handle);
    }

  if (nullptr == so_handle) {
    OMS_WARN("Failed to get so library handle, try to delete the pointer of IObCdcAccess in place.");
    delete obcdc;
  } else {
    auto destroy = (fn_destroy)dlsym(so_handle, "destroy");
    if (nullptr == destroy) {
      OMS_WARN(
          "Failed to find func [destroy], error: {}, try to delete the pointer of IObCdcAccess in place.", dlerror());
      delete obcdc;
    } else {
      destroy(obcdc);
    }

    dlclose(so_handle);
  }

  OMS_INFO("Successfully close the so handle of obcdc.");
 }
  */
}

int ObCdcAccessFactory::locate_obcdc_library(const std::string& ob_version, std::string& obcdc_so_path)
{
  uint8_t ob_major_version = atoi(ob_version.substr(0, 1).c_str());

  std::string obcdc_so_path_template;
  Config& s_config = Config::instance();
  bool binlog_mode = s_config.binlog_mode.val();
#ifdef _COMMUNITY_
  obcdc_so_path_template =
      binlog_mode ? s_config.binlog_obcdc_ce_path_template.val() : s_config.oblogreader_obcdc_ce_path_template.val();
#else
#endif

  char path[256];
  switch (ob_major_version) {
    case 1: {
      sprintf(path, obcdc_so_path_template.c_str(), "2");
      break;
    }
    case 2:
    case 3: {
      sprintf(path, obcdc_so_path_template.c_str(), "3");
      break;
    }
    case 4: {
      std::vector<std::string> segments;
      split(ob_version.substr(0, 5), '.', segments);
      if (segments.size() != 3) {
        OMS_ERROR("Unknown OB major version: {}", ob_version);
        return OMS_FAILED;
      }
      // For OB versions less than 421, we uniformly use the 421 version of OBCDC.
      if (atoi(segments[1].c_str()) < 2 || (atoi(segments[1].c_str()) == 2 && atoi(segments[2].c_str()) <= 1)) {
        sprintf(path, obcdc_so_path_template.c_str(), INCOMPATIBLE_VERSION);
      } else {
        sprintf(path, obcdc_so_path_template.c_str(), ob_version.substr(0, 5).c_str());
      }
      break;
    }
    default: {
      OMS_ERROR("Unknown OB major version: {}", ob_major_version);
      return OMS_FAILED;
    }
  }
  obcdc_so_path = path;
  return OMS_OK;
}

}  // namespace logproxy
}  // namespace oceanbase
