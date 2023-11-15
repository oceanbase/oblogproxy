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

#include "io.h"
#include "channel.h"
#include "comm.h"
#include "peer.h"

namespace oceanbase {
namespace logproxy {
PlainChannel::PlainChannel(const Peer& peer) : Channel(peer)
{}

int PlainChannel::after_accept()
{
  return OMS_OK;
}
int PlainChannel::after_connect()
{
  return OMS_OK;
}

int PlainChannel::read(char* buf, int size)
{
  return ::read(_peer.fd, buf, size);
}

int PlainChannel::readn(char* buf, int size)
{
  return ::oceanbase::logproxy::readn(_peer.fd, buf, size);
}

int PlainChannel::write(const char* buf, int size)
{
  return ::write(_peer.fd, buf, size);
}

int PlainChannel::writen(const char* buf, int size)
{
  return ::oceanbase::logproxy::writen(_peer.fd, buf, size);
}

const char* PlainChannel::last_error()
{
  return strerror(errno);
}

}  // namespace logproxy
}  // namespace oceanbase
