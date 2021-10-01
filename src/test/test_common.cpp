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
#include "log.h"
#include "config.h"
#include "common.h"

using namespace oceanbase::logproxy;

TEST(COMMON, hex2bin)
{
	const char* text = "this is a text";
	int len = strlen(text);

	std::string hexstr;
	dumphex(text, len, hexstr);

	std::string binstr;
	hex2bin(hexstr.data(), hexstr.size(), binstr);
	ASSERT_STREQ(binstr.c_str(), text);

	binstr.clear();
	hexstr.insert(0, 1, ' ');
	hexstr.insert(hexstr.size()/2, 1, ' ');
	hexstr.insert(hexstr.size(), 1, ' ');
	hex2bin(hexstr.data(), hexstr.size(), binstr);
	ASSERT_STREQ(binstr.c_str(), text);
}
