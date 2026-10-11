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

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace arq_storage {
struct Endpoint
{
  bool tls = false;
  std::string host;
  std::uint16_t port = 0;
  std::string path = "/";

  explicit operator bool() const noexcept { return !host.empty() && port != 0; }
};

/// Parses `http://host:port[/path]`, `https://host:port[/path]`, and
/// RFC 3986 IPv6 (`http://[::1]:22021/status`). Default ports: http=80, https=443.
Endpoint parse_endpoint(std::string_view url) noexcept;

/// Parses CLI `host:port` or `[ipv6]:port`. Port may be 0 (ephemeral bind).
bool parse_listen_address(std::string_view listen, std::string& host, std::uint16_t& port) noexcept;

/// `127.0.0.1:22021` or `[::1]:22021` (IPv6 host is bracketed).
std::string format_http_authority(std::string_view host, std::uint16_t port);

/// HTTP `Host` header value for an endpoint (IPv6 bracketed, port included).
std::string http_host_header(const Endpoint& endpoint);

/// Minimal HTTP/1.1 GET used by cleartext reachability probes.
std::string format_http_get_request(const Endpoint& endpoint) noexcept;
} // namespace arq_storage
