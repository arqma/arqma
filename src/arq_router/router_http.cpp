// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "router_http.h"
#include "arq_messaging/onion_forward.hpp"
#include "arq_messaging/onion_layer.hpp"
#include "arq_messaging/onion_request.hpp"
#include "arq_storage/http_io.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace arq_router {
namespace {
std::string hex32(const arq_messaging::X25519PublicKey& pub)
{
  static const char* digits = "0123456789abcdef";
  std::string out(64, '0');
  for (std::size_t i = 0; i < 32; ++i) {
    out[i * 2] = digits[pub.data[i] >> 4];
    out[i * 2 + 1] = digits[pub.data[i] & 0xf];
  }
  return out;
}

std::string http_ok(const std::string& body, const char* type = "application/octet-stream")
{
  std::ostringstream oss;
  oss << "HTTP/1.1 200 OK\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size()
      << "\r\nConnection: close\r\n\r\n"
      << body;
  return oss.str();
}

bool peel(const arq_messaging::Identity& hop, const std::string& body, std::string& inner)
{
  if (body.empty() || hop.public_key.is_null())
    return false;
  std::vector<std::uint8_t> outer(body.begin(), body.end());
  std::vector<std::uint8_t> peeled;
  if (arq_messaging::peel_onion_layer(hop, outer, peeled))
    return false;
  inner.assign(peeled.begin(), peeled.end());
  return true;
}
} // namespace

std::string handle_http(const std::string& method, const std::string& path, const std::string& body,
                        const arq_messaging::Identity& hop, const std::string& storage_url, const std::string& token)
{
  const auto path_only = path.substr(0, path.find('?'));
  if (method == "GET" && (path_only == "/" || path_only == "/status"))
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\nConnection: close\r\n\r\narqma-router";

  if (method == "GET" && path_only == "/v1/pubkey")
    return http_ok(hex32(hop.public_key), "text/plain");

  if (!arq_storage::request_token_ok(path, token))
    return "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

  if (method == "POST" && path_only == "/v1/peel") {
    std::string inner;
    if (!peel(hop, body, inner))
      return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    return http_ok(inner);
  }

  if (method == "POST" && path_only == "/v1/store") {
    const auto ns = arq_storage::query_get(path, "ns");
    const auto key = arq_storage::query_get(path, "key");
    const auto ttl = arq_storage::query_get(path, "ttl");
    if (ns.empty() || key.empty())
      return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    std::string inner;
    if (!peel(hop, body, inner))
      return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    unsigned long fwd = 0;
    const auto fwd_raw = arq_storage::query_get(path, "fwd");
    if (!fwd_raw.empty())
      fwd = std::strtoul(fwd_raw.c_str(), nullptr, 10);
    if (fwd >= arq_messaging::OnionRequest::hop_count)
      return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    std::string next_url;
    std::vector<std::uint8_t> rest;
    if (arq_messaging::decode_onion_forward(inner, next_url, rest)) {
      if (fwd + 1 >= arq_messaging::OnionRequest::hop_count)
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      const auto next = arq_storage::parse_endpoint(next_url);
      if (!next || next.tls || !arq_storage::hop_host_allowed(next_url, storage_url))
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      auto fwd_path = "/v1/store?ns=" + arq_storage::url_encode(ns) + "&key=" + arq_storage::url_encode(key) +
                      "&fwd=" + std::to_string(fwd + 1);
      if (!ttl.empty())
        fwd_path += "&ttl=" + ttl;
      fwd_path = arq_storage::with_token(fwd_path, token);
      const auto hop_put = arq_storage::http_exchange(next, "POST", fwd_path, std::string(rest.begin(), rest.end()));
      if (!hop_put)
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      return "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }
    if (storage_url.empty())
      return "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    auto put_path = arq_storage::kv_path(ns, key);
    if (!ttl.empty())
      put_path += "&ttl=" + ttl;
    put_path = arq_storage::with_token(put_path, token);
    const auto ep = arq_storage::parse_endpoint(storage_url);
    const auto put = arq_storage::http_exchange(ep, "PUT", put_path, inner);
    if (!put)
      return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    return "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  }

  return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
}
} // namespace arq_router
