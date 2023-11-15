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

#include "gtest/gtest.h"
#include "common/log.h"
#include "communication/http.h"

using namespace oceanbase::logproxy;

void get(const std::string& url)
{
  HttpResponse response;
  HttpClient::get(url, response);
  OMS_INFO << "response:" << response.code << " " << response.message;
  OMS_INFO << "header:\n";
  for (auto& entry : response.headers) {
    OMS_INFO << "\t" << entry.first << ": " << entry.second;
  }
  OMS_INFO << "payload:" << response.payload;
}

TEST(HTTP, get)
{
  get("http://127.0.0.1:8080/services?Action=xxxx");
}
