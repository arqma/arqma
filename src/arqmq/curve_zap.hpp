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

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace arqmq {
/// ZAP domain used by Arq-Net SN listeners (must match SNNetwork).
inline constexpr const char k_zap_auth_domain_sn[] = "arqma.sn";

/// Well-known per-context ZAP endpoint required by libzmq.
inline constexpr const char k_zap_endpoint[] = "inproc://zeromq.zap.01";

enum class CurvePeerAllow
{
  Denied = 0,
  Client,
  ServiceNode,
};

using AllowConnection = std::function<CurvePeerAllow(const std::string& ip, const std::string& pubkey)>;

/// Parsed ZAP request frames (RFC 27), after REP delimiter stripping.
struct ZapRequestView
{
  std::string_view version;
  std::string_view request_id;
  std::string_view domain;
  std::string_view address;
  std::string_view identity;
  std::string_view mechanism;
  std::string_view credentials; ///< 32-byte CURVE pubkey when mechanism is CURVE
};

struct ZapReply
{
  std::string version = "1.0";
  std::string request_id;
  std::string status_code;
  std::string status_text;
  std::string user_id;
  std::string metadata;
};

/// Lower-hex encode of opaque bytes (matches SNNetwork user-id formatting).
std::string to_hex_lower(std::string_view bytes);

/// Pure ZAP CURVE evaluator shared by SocketStack and unit tests.
ZapReply evaluate_curve_zap_request(const ZapRequestView& request, const AllowConnection& allow);

/// Convenience over a contiguous frame list (version … credentials).
ZapReply evaluate_curve_zap_frames(const std::vector<std::string_view>& frames, const AllowConnection& allow);
} // namespace arqmq
