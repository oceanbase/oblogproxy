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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

namespace oceanbase {
namespace logproxy {

class SHA1 {
public:
  static constexpr int SHA1_HASH_SIZE = 20; /* Hash size in bytes */

  enum ResultCode {
    SHA_SUCCESS = 0,
    SHA_NULL,           /* Null pointer parameter */
    SHA_INPUT_TOO_LONG, /* input data too long */
    SHA_STATE_ERROR     /* called Input after Result */
  };

public:
  SHA1();
  ~SHA1();

  SHA1(SHA1& other) = delete;
  SHA1& operator=(SHA1& other) = delete;

  /*
    Initialize SHA1Context

    SYNOPSIS
      reset()

   DESCRIPTION
     This function will initialize the SHA1Context in preparation
     for computing a new SHA1 message digest.

   RETURN
     SHA_SUCCESS    ok
     != SHA_SUCCESS sha Error Code.
  */
  ResultCode reset();

  /*
    Accepts an array of octets as the next portion of the message.

    SYNOPSIS
     input()
     message_array  An array of characters representing the next portion
        of the message.
    length    The length of the message in message_array

   RETURN
     SHA_SUCCESS    ok
     != SHA_SUCCESS sha Error Code.
  */
  ResultCode input(const unsigned char* message, unsigned int length);
  /*
     Return the 160-bit message digest into the array provided by the caller

    SYNOPSIS
      result()
      Message_Digest: [out] Where the digest is returned.

    DESCRIPTION
      NOTE: The first octet of hash is stored in the 0th element,
      the last octet of hash in the 19th element.

   RETURN
     SHA_SUCCESS    ok
     != SHA_SUCCESS sha Error Code.
  */
  ResultCode get_result(unsigned char digest[SHA1_HASH_SIZE]);

private:
  /*
      Process the next 512 bits of the message stored in the _message_block array.

      SYNOPSIS
        process_message_block()

       DESCRIPTION
         Many of the variable names in this code, especially the single
         character names, were used because those were the names used in
         the publication.
  */
  void process_message_block();

  /*
    Pad message

    SYNOPSIS
      pad_message()

    DESCRIPTION
      According to the standard, the message must be padded to an even
      512 bits.  The first padding bit must be a '1'. The last 64 bits
      represent the length of the original message.  All bits in between
      should be 0.  This function will pad the message according to
      those rules by filling the _message_block array accordingly.  It
      will also call the ProcessMessageBlock function provided
      appropriately. When it returns, it can be assumed that the message
      digest has been computed.
  */
  void pad_message();

private:
  unsigned long _length;                           /* Message length in bits      */
  uint32_t _intermediate_hash[SHA1_HASH_SIZE / 4]; /* Message Digest  */
  int _computed;                                   /* Is the digest computed?     */
  ResultCode _corrupted;                           /* Is the message digest corrupted? */
  int16_t _message_block_index;                    /* Index into message block array   */
  uint8_t _message_block[64];                      /* 512-bit message blocks      */
};
}  // namespace logproxy
}  // namespace oceanbase
