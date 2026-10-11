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

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arqmq {
/// Align with `SN_ZMQ_MAX_MSG_SIZE` in sn_network.cpp so the facade and
/// live SNNetwork transport reject oversized payloads consistently.
constexpr size_t max_message_bytes = 1024 * 1024;
constexpr size_t max_command_name_bytes = 64;
constexpr size_t max_payload_frames = 16;

inline bool allow_message_size(size_t bytes) noexcept
{
  return bytes <= max_message_bytes;
}

inline bool allow_command_name(std::string_view name) noexcept
{
  return !name.empty() && name.size() <= max_command_name_bytes;
}

inline bool allow_payload_frame_count(size_t frames) noexcept
{
  return frames <= max_payload_frames;
}

/// Combined framing gate used before ACL authorize / dispatch.
inline bool accept_request(std::string_view command, size_t payload_bytes, size_t payload_frames = 1) noexcept
{
  return allow_command_name(command) && allow_message_size(payload_bytes) && allow_payload_frame_count(payload_frames);
}
} // namespace arqmq
