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

#include "arqmq.h"
#include "message_limits.hpp"

#include <string_view>

namespace arqmq {
/// Static command → ACL map for the dedicated ArqMQ socket stack and
/// SNNetwork handlers. LegacyArqNet still dispatches mesh I/O through
/// SNNetwork; `--arqnet-backend=arqmq` also runs `arqmq::SocketStack`.
struct CommandAcl
{
  std::string_view name;
  CategoryAcl required;
};

inline constexpr CommandAcl k_builtin_commands[] = {
    {"ping", CategoryAcl::Basic},           {"pong", CategoryAcl::Basic},
    {"vote_ob", CategoryAcl::ServiceNode},  {"pulse_rnd", CategoryAcl::ServiceNode},
    {"blink_tx", CategoryAcl::ServiceNode}, {"arqnet_status", CategoryAcl::Basic},
    {"admin_shutdown", CategoryAcl::Admin},
};

inline CategoryAcl required_acl_for(std::string_view command) noexcept
{
  for (const auto& entry : k_builtin_commands) {
    if (entry.name == command)
      return entry.required;
  }
  return CategoryAcl::Denied;
}

inline bool authorize(std::string_view command, CategoryAcl granted) noexcept
{
  const CategoryAcl required = required_acl_for(command);
  // Unknown / explicitly denied commands never authorize, even for Admin.
  if (required == CategoryAcl::Denied)
    return false;
  return allows(required, granted);
}

/// Framing + ACL gate for inbound ArqMQ/Arq-Net commands.
inline bool authorize_request(std::string_view command, CategoryAcl granted, size_t payload_bytes,
                              size_t payload_frames = 1) noexcept
{
  if (!accept_request(command, payload_bytes, payload_frames))
    return false;
  return authorize(command, granted);
}
} // namespace arqmq
