// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "storage_endpoint.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace arq_storage {
namespace {
bool parse_port_digits(const std::string_view raw, std::uint16_t& port) noexcept
{
  if (raw.empty() || raw.size() > 5)
    return false;
  unsigned long value = 0;
  for (char c : raw) {
    if (!std::isdigit(static_cast<unsigned char>(c)))
      return false;
    value = value * 10u + static_cast<unsigned long>(c - '0');
  }
  if (value > 65535u)
    return false;
  port = static_cast<std::uint16_t>(value);
  return true;
}

bool parse_host_port(const std::string_view hostport, std::string& host, std::uint16_t& port,
                     const std::uint16_t default_port, const bool require_port) noexcept
{
  if (hostport.empty() || hostport.find(' ') != std::string_view::npos)
    return false;
  if (hostport.front() == '[') {
    const auto close = hostport.find(']');
    if (close == std::string_view::npos || close < 2)
      return false;
    host.assign(hostport.substr(1, close - 1));
    if (host.empty())
      return false;
    if (close + 1 == hostport.size()) {
      if (require_port)
        return false;
      port = default_port;
      return true;
    }
    if (hostport[close + 1] != ':')
      return false;
    return parse_port_digits(hostport.substr(close + 2), port);
  }
  const auto colon = hostport.rfind(':');
  if (colon == std::string_view::npos) {
    if (require_port)
      return false;
    host.assign(hostport);
    port = default_port;
    return !host.empty();
  }
  if (colon == 0)
    return false;
  host.assign(hostport.substr(0, colon));
  return !host.empty() && parse_port_digits(hostport.substr(colon + 1), port);
}
} // namespace

bool parse_listen_address(const std::string_view listen, std::string& host, std::uint16_t& port) noexcept
{
  host.clear();
  port = 0;
  return parse_host_port(listen, host, port, 0, true);
}

std::string format_http_authority(const std::string_view host, const std::uint16_t port)
{
  if (host.find(':') != std::string_view::npos)
    return "[" + std::string{host} + "]:" + std::to_string(port);
  return std::string{host} + ":" + std::to_string(port);
}

std::string http_host_header(const Endpoint& endpoint)
{
  return format_http_authority(endpoint.host, endpoint.port);
}

Endpoint parse_endpoint(const std::string_view url) noexcept
{
  Endpoint out{};
  if (url.empty())
    return out;

  std::string_view rest = url;
  if (rest.size() >= 8 && rest.substr(0, 8) == "https://") {
    out.tls = true;
    rest.remove_prefix(8);
  } else if (rest.size() >= 7 && rest.substr(0, 7) == "http://") {
    out.tls = false;
    rest.remove_prefix(7);
  } else
    return {};

  const auto slash = rest.find('/');
  const std::string_view hostport = slash == std::string_view::npos ? rest : rest.substr(0, slash);
  if (slash != std::string_view::npos)
    out.path = std::string{rest.substr(slash)};
  if (out.path.empty())
    out.path = "/";

  if (!parse_host_port(hostport, out.host, out.port, out.tls ? 443 : 80, false))
    return {};
  if (out.host.empty() || out.host == "." || out.host.find('/') != std::string::npos)
    return {};
  return out;
}

std::string format_http_get_request(const Endpoint& endpoint) noexcept
{
  if (!endpoint)
    return {};
  const std::string& path = endpoint.path.empty() ? "/" : endpoint.path;
  return "GET " + path + " HTTP/1.1\r\nHost: " + http_host_header(endpoint) + "\r\nConnection: close\r\n\r\n";
}
} // namespace arq_storage
