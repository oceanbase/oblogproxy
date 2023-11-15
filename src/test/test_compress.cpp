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
#include "lz4.h"
#include "log.h"
#include "common/common.h"

using namespace oceanbase::logproxy;

TEST(COMPRESS, lz4_flow)
{
  //  std::vector<std::string> texts = {{"01234567899876543210"}, {"abcdefghijabcdefghij"}};
  std::vector<std::string> texts = {{"11111111111111111111"}, {"abcdefghijabcdefghij"}};

  char compressed[100] = "\0";

  LZ4_stream_t lz4s;
  LZ4_resetStream(&lz4s);
  uint32_t compressed_size = 0;
  for (auto& text : texts) {
    size_t block_size = text.size();
    memcpy(compressed + compressed_size, &block_size, 4);

    int bound_size = LZ4_COMPRESSBOUND(block_size);
    char* block_compressed = (char*)malloc(bound_size);
    int compressed_block_size =
        LZ4_compress_fast_continue(&lz4s, text.c_str(), block_compressed, block_size, bound_size, 1);
    ASSERT_TRUE(compressed_block_size > 0);
    memcpy(compressed + compressed_size + 4, &compressed_block_size, 4);
    memcpy(compressed + compressed_size + 8, block_compressed, compressed_block_size);

    compressed_size += (compressed_block_size + 8);

    free(block_compressed);
  }

  std::string compressed_hex;
  dumphex(compressed, compressed_size, compressed_hex);
  OMS_STREAM_INFO << "compressed buffer:" << compressed_hex << ", size: " << compressed_size;

  size_t offset = 0;
  while (offset < compressed_size) {
    uint32_t block_size = 0;
    uint32_t compressed_block_size = 0;
    memcpy(&block_size, compressed + offset, 4);
    memcpy(&compressed_block_size, compressed + offset + 4, 4);

    char* raw_block = (char*)malloc(block_size + 1);
    ASSERT_TRUE(raw_block != nullptr);
    int decompressed_size = LZ4_decompress_safe(compressed + offset + 8, raw_block, compressed_block_size, block_size);
    ASSERT_EQ((uint32_t)decompressed_size, block_size);
    raw_block[decompressed_size] = '\0';
    OMS_STREAM_INFO << "decompress block: " << raw_block << ", size:" << block_size
             << ", compressed size:" << compressed_block_size;

    free(raw_block);
    offset += (compressed_block_size + 8);
  }
}
