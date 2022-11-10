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

#include <sys/queue.h>

#include "event2/event.h"
#include "event2/buffer.h"
#include "event2/dns.h"
#include "event2/keyvalq_struct.h"

#include "common/common.h"
#include "common/log.h"
#include "common/guard.hpp"
#include "communication/http.h"

namespace oceanbase {
namespace logproxy {

struct HttpContext {
  struct event_base* base;

  // error of libevent
  evhttp_request_error error;

  HttpResponse* response = nullptr;
};

static void http_request_done(struct evhttp_request*, void* arg)
{
  event_base_loopbreak(((HttpContext*)arg)->base);
}

static int http_header_cb(struct evhttp_request* response, void* arg)
{
  HttpContext* context = (HttpContext*)arg;
  context->response->code = evhttp_request_get_response_code(response);
  context->response->message = evhttp_request_get_response_code_line(response);
  struct evkeyval* header;
  struct evkeyvalq* headers = evhttp_request_get_input_headers(response);
  TAILQ_FOREACH(header, headers, next)
  {
    context->response->headers.emplace(header->key, header->value);
  }
  return 0;
}

static void http_payload_cb(struct evhttp_request* reponse, void* arg)
{
  HttpContext* context = (HttpContext*)arg;

  int n = 0;
  char buf[8192];
  struct evbuffer* evbuf = evhttp_request_get_input_buffer(reponse);
  while ((n = evbuffer_remove(evbuf, buf, sizeof(buf))) > 0) {
    context->response->payload.append(buf, n);
  }
}

static void http_request_error_cb(enum evhttp_request_error error, void* arg)
{
  OMS_ERROR << "HTTP request error:" << error;
  HttpContext* context = (HttpContext*)arg;
  context->error = error;
  event_base_loopexit(context->base, nullptr);
}

void http_conn_close_cb(struct evhttp_connection*, void* arg)
{
  event_base_loopexit(((HttpContext*)arg)->base, nullptr);
}

int HttpClient::get(const std::string& url, HttpResponse& response)
{
  evhttp_uri* uri = evhttp_uri_parse(url.c_str());
  if (uri == nullptr) {
    OMS_ERROR << "Failed to http GET, failed to parse url:" << url.c_str();
    return OMS_FAILED;
  }
  FreeGuard<evhttp_uri*> uri_fg(uri, evhttp_uri_free);
  const char* host = evhttp_uri_get_host(uri);
  if (host == nullptr) {
    OMS_ERROR << "Failed to http GET, failed to parse host";
    return OMS_FAILED;
  }
  int port = evhttp_uri_get_port(uri);
  if (port == -1) {
    port = 80;
  }
  const char* path = evhttp_uri_get_path(uri);
  std::string request_url = (path == nullptr || strlen(path) == 0) ? "/" : path;
  const char* query = evhttp_uri_get_query(uri);
  if (query != nullptr && strlen(query) > 0) {
    request_url.append("?").append(query);
  }

  struct event_base* base = event_base_new();
  FreeGuard<event_base*> base_fg(base, event_base_free);
  if (base == nullptr) {
    OMS_ERROR << "Failed to http GET, failed to event_base_new";
    return OMS_FAILED;
  }

  struct evdns_base* dnsbase = evdns_base_new(base, 1);
  if (dnsbase == nullptr) {
    OMS_WARN << "Failed to invoke evdns_base_new, just skip";
  }

  FreeGuard<evdns_base*> dnsbase_fg(dnsbase, [](struct evdns_base* ptr) { evdns_base_free(ptr, 0); });
  HttpContext context;
  context.base = base;
  context.response = &response;

  struct evhttp_connection* conn = evhttp_connection_base_new(base, dnsbase, host, port);
  FreeGuard<evhttp_connection*> conn_fg(conn, evhttp_connection_free);
  if (conn == nullptr) {
    OMS_ERROR << "Failed to http GET, failed to evhttp_connection_base";
    return OMS_FAILED;
  }
  evhttp_connection_set_timeout(conn, 600);
  evhttp_connection_set_closecb(conn, http_conn_close_cb, &context);

  struct evhttp_request* req = evhttp_request_new(http_request_done, &context);
  if (req == nullptr) {
    OMS_ERROR << "Failed to http GET, failed to evhttp_request";
    return OMS_FAILED;
  }
  evhttp_request_set_header_cb(req, http_header_cb);
  evhttp_request_set_chunked_cb(req, http_payload_cb);
  evhttp_request_set_error_cb(req, http_request_error_cb);
  evhttp_add_header(evhttp_request_get_output_headers(req), "Host", host);

  OMS_DEBUG << "url:" << url << " host:" << host << " port:" << port << " path:" << path
            << " request_url:" << request_url;
  // The connection gets ownership of the request
  int ret = evhttp_make_request(conn, req, EVHTTP_REQ_GET, request_url.c_str());
  if (ret == -1) {
    OMS_ERROR << "Failed to http GET of make request";
    return OMS_FAILED;
  }

  // start invoke http
  event_base_dispatch(base);

  return OMS_OK;
}

}  // namespace logproxy

}  // namespace oceanbase