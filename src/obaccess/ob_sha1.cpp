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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>

#include "obaccess/ob_sha1.h"

namespace oceanbase {
namespace logproxy {

#define SHA1CircularShift(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

static const uint32_t sha_const_key[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

/* Constants defined in SHA-1 */
static const uint32_t K[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

void SHA1::process_message_block()
{
  int t = 0;                                  /* Loop counter      */
  uint32_t temp = 0;                          /* Temporary word value    */
  uint32_t W[80];                             /* Word sequence     */
  uint32_t A = 0, B = 0, C = 0, D = 0, E = 0; /* Word buffers      */
  int idx = 0;

  /*
    Initialize the first 16 words in the array W
  */

  for (t = 0; t < 16; t++) {
    idx = t * 4;
    W[t] = _message_block[idx] << 24;
    W[t] |= _message_block[idx + 1] << 16;
    W[t] |= _message_block[idx + 2] << 8;
    W[t] |= _message_block[idx + 3];
  }

  for (t = 16; t < 80; t++) {
    W[t] = SHA1CircularShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
  }

  A = _intermediate_hash[0];
  B = _intermediate_hash[1];
  C = _intermediate_hash[2];
  D = _intermediate_hash[3];
  E = _intermediate_hash[4];

  for (t = 0; t < 20; t++) {
    temp = SHA1CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = temp;
  }

  for (t = 20; t < 40; t++) {
    temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = temp;
  }

  for (t = 40; t < 60; t++) {
    temp = (SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2]);
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = temp;
  }

  for (t = 60; t < 80; t++) {
    temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = temp;
  }

  _intermediate_hash[0] += A;
  _intermediate_hash[1] += B;
  _intermediate_hash[2] += C;
  _intermediate_hash[3] += D;
  _intermediate_hash[4] += E;

  _message_block_index = 0;
}

void SHA1::pad_message()
{
  /*
    Check to see if the current message block is too small to hold
    the initial padding bits and length.  If so, we will pad the
    block, process it, and then continue padding into a second
    block.
  */

  int i = _message_block_index;

  if (i > 55) {
    _message_block[i++] = 0x80;
    memset((char*)&_message_block[i], 0, sizeof(_message_block[0]) * (64 - i));
    _message_block_index = 64;

    /* This function sets _message_block_index to zero  */
    process_message_block();

    memset((char*)&_message_block[0], 0, sizeof(_message_block[0]) * 56);
    _message_block_index = 56;
  } else {
    _message_block[i++] = 0x80;
    memset((char*)&_message_block[i], 0, sizeof(_message_block[0]) * (56 - i));
    _message_block_index = 56;
  }

  /*
    Store the message length as the last 8 octets
  */

  _message_block[56] = (int8_t)(_length >> 56);
  _message_block[57] = (int8_t)(_length >> 48);
  _message_block[58] = (int8_t)(_length >> 40);
  _message_block[59] = (int8_t)(_length >> 32);
  _message_block[60] = (int8_t)(_length >> 24);
  _message_block[61] = (int8_t)(_length >> 16);
  _message_block[62] = (int8_t)(_length >> 8);
  _message_block[63] = (int8_t)(_length);

  process_message_block();
}

SHA1::SHA1()
{
  reset();
}

SHA1::~SHA1()
{
  reset();
}

SHA1::ResultCode SHA1::reset()
{
  _length = 0;
  _message_block_index = 0;

  _intermediate_hash[0] = sha_const_key[0];
  _intermediate_hash[1] = sha_const_key[1];
  _intermediate_hash[2] = sha_const_key[2];
  _intermediate_hash[3] = sha_const_key[3];
  _intermediate_hash[4] = sha_const_key[4];

  _computed = 0;
  _corrupted = SHA_SUCCESS;

  return SHA_SUCCESS;
}

SHA1::ResultCode SHA1::get_result(unsigned char Message_Digest[SHA1_HASH_SIZE])
{
  int i = 0;
  if (!_computed) {
    pad_message();
    /* message may be sensitive, clear it out */
    memset((void*)_message_block, 0, 64);
    _length = 0; /* and clear length  */
    _computed = 1;
  }

  for (i = 0; i < SHA1_HASH_SIZE; i++)
    Message_Digest[i] = (int8_t)((_intermediate_hash[i >> 2] >> 8 * (3 - (i & 0x03))));
  return SHA_SUCCESS;
}

SHA1::ResultCode SHA1::input(const unsigned char* message_array, unsigned int length)
{
  SHA1::ResultCode sha_ret = SHA_SUCCESS;
  if (!length) {
    sha_ret = SHA_SUCCESS;
  } else if (!message_array) {
    /* We assume client knows what it is doing in non-debug mode */
    sha_ret = SHA_NULL;
  } else if (_computed) {
    sha_ret = (_corrupted = SHA_STATE_ERROR);
  } else if (_corrupted) {
    sha_ret = _corrupted;
  } else {
    while (SHA_SUCCESS == sha_ret && length--) {
      _message_block[_message_block_index++] = (*message_array & 0xFF);
      _length += 8; /* _length is in bits */
      /*
        Then we're not debugging we assume we never will get message longer
        2^64 bits.
      */
      if (_length == 0) {
        sha_ret = (_corrupted = SHA_INPUT_TOO_LONG); /* Message is too long */
      } else {
        if (_message_block_index == 64) {
          process_message_block();
        }
        message_array++;
      }
    }
  }
  return sha_ret;
}

}  // namespace logproxy
}  // namespace oceanbase
