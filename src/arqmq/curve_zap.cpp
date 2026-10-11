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

#include "curve_zap.hpp"

namespace arqmq {
namespace {
constexpr char k_hex[] = "0123456789abcdef";
}

std::string to_hex_lower(const std::string_view bytes)
{
  std::string hex;
  hex.reserve(bytes.size() * 2);
  for (const unsigned char c : bytes) {
    hex.push_back(k_hex[(c & 0xf0) >> 4]);
    hex.push_back(k_hex[c & 0x0f]);
  }
  return hex;
}

ZapReply evaluate_curve_zap_request(const ZapRequestView& request, const AllowConnection& allow)
{
  ZapReply reply;
  reply.request_id = std::string{request.request_id};

  if (request.version != "1.0") {
    reply.status_code = "500";
    reply.status_text = "Internal error: invalid auth request";
    return reply;
  }
  if (request.mechanism != "CURVE") {
    reply.status_code = "500";
    reply.status_text = "Invalid CURVE authentication request\n";
    return reply;
  }
  if (request.credentials.size() != 32) {
    reply.status_code = "500";
    reply.status_text = "Invalid public key size for CURVE authentication";
    return reply;
  }
  if (request.domain != k_zap_auth_domain_sn) {
    reply.status_code = "400";
    reply.status_text = "Unknown authentication domain: " + std::string{request.domain};
    return reply;
  }
  if (!allow) {
    reply.status_code = "400";
    reply.status_text = "Access denied";
    return reply;
  }

  const std::string ip{request.address};
  const std::string pubkey{request.credentials};
  const auto decision = allow(ip, pubkey);
  if (decision == CurvePeerAllow::ServiceNode || decision == CurvePeerAllow::Client) {
    reply.status_code = "200";
    reply.status_text.clear();
    reply.user_id = (decision == CurvePeerAllow::ServiceNode ? "S:" : "C:") + to_hex_lower(pubkey);
    return reply;
  }

  reply.status_code = "400";
  reply.status_text = "Access denied";
  return reply;
}

ZapReply evaluate_curve_zap_frames(const std::vector<std::string_view>& frames, const AllowConnection& allow)
{
  ZapRequestView view;
  if (frames.size() >= 1)
    view.version = frames[0];
  if (frames.size() >= 2)
    view.request_id = frames[1];
  if (frames.size() >= 3)
    view.domain = frames[2];
  if (frames.size() >= 4)
    view.address = frames[3];
  if (frames.size() >= 5)
    view.identity = frames[4];
  if (frames.size() >= 6)
    view.mechanism = frames[5];
  if (frames.size() >= 7)
    view.credentials = frames[6];

  if (frames.size() < 6) {
    ZapReply reply;
    if (frames.size() >= 2)
      reply.request_id = std::string{frames[1]};
    reply.status_code = "500";
    reply.status_text = "Internal error: invalid auth request";
    return reply;
  }
  if (frames.size() != 7) {
    ZapReply reply;
    reply.request_id = std::string{view.request_id};
    reply.status_code = "500";
    reply.status_text = "Invalid CURVE authentication request\n";
    return reply;
  }
  return evaluate_curve_zap_request(view, allow);
}
} // namespace arqmq
