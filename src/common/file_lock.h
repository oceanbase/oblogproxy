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
#include <fcntl.h>

#ifndef UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif
#endif

namespace oceanbase {
namespace logproxy {
UNUSED static int _flock(int fd, int type, int whence, int offset, int len)
{
  struct flock fl;
  fl.l_type = type;
  fl.l_whence = whence;
  fl.l_start = offset;
  fl.l_len = len;
  return fcntl(fd, F_SETLKW, &fl);
}

UNUSED static int _non_block_flock(int fd, int type, int whence, int offset, int len)
{
  struct flock fl;
  fl.l_type = type;
  fl.l_whence = whence;
  fl.l_start = offset;
  fl.l_len = len;

  return fcntl(fd, F_SETLK, &fl);
}

#define OMS_PROC_WRLOCK(fd) _flock(fd, F_WRLCK, SEEK_SET, 0, 0)
#define OMS_PROC_RDLOCK(fd) _flock(fd, F_RDLCK, SEEK_SET, 0, 0)
#define OMS_PROC_NON_BLOCK_WRLOCK(fd) _non_block_flock(fd, F_WRLCK, SEEK_SET, 0, 0)
#define OMS_PROC_NON_BLOCK_RDLOCK(fd) _non_block_flock(fd, F_RDLCK, SEEK_SET, 0, 0)
#define OMS_PROC_UNLOCK(fd) _flock(fd, F_UNLCK, SEEK_SET, 0, 0)

#define OMS_PROC_WRLOCK_EX(fd, offset, n) _flock(fd, F_WRLCK, SEEK_SET, offset, n)
#define OMS_PROC_RDLOCK_EX(fd, offset, n) _flock(fd, F_RDLCK, SEEK_SET, offset, n)
#define OMS_PROC_NON_BLOCK_WRLOCK_EX(fd, offset, n) _non_block_flock(fd, F_WRLCK, SEEK_SET, offset, n)
#define OMS_PROC_NON_BLOCK_RDLOCK_EX(fd, offset, n) _non_block_flock(fd, F_RDLCK, SEEK_SET, offset, n)
#define OMS_PROC_UNLOCK_EX(fd, offset, n) _flock(fd, F_UNLCK, SEEK_SET, offset, n)

}  // namespace logproxy
}  // namespace oceanbase
