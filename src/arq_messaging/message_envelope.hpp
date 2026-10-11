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

#include "identity.hpp"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace arq_messaging {
struct MessageEnvelope
{
  static constexpr std::uint8_t current_version = 1;

  std::uint8_t version = current_version;
  X25519PublicKey recipient_x25519{};
  std::vector<std::uint8_t> payload;
  std::uint32_t ttl_seconds = 0;
};

namespace detail {
inline void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value)
{
  out.push_back(static_cast<std::uint8_t>(value & 0xff));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
}

inline std::uint32_t read_u32_le(const std::uint8_t* ptr) noexcept
{
  return static_cast<std::uint32_t>(ptr[0]) | (static_cast<std::uint32_t>(ptr[1]) << 8) |
         (static_cast<std::uint32_t>(ptr[2]) << 16) | (static_cast<std::uint32_t>(ptr[3]) << 24);
}
} // namespace detail

inline std::vector<std::uint8_t> encode_message_envelope(const MessageEnvelope& envelope)
{
  std::vector<std::uint8_t> encoded;
  encoded.reserve(1 + envelope.recipient_x25519.data.size() + 8 + envelope.payload.size());

  encoded.push_back(envelope.version);
  encoded.insert(encoded.end(), envelope.recipient_x25519.data.begin(), envelope.recipient_x25519.data.end());
  detail::append_u32_le(encoded, envelope.ttl_seconds);
  detail::append_u32_le(encoded, static_cast<std::uint32_t>(envelope.payload.size()));
  encoded.insert(encoded.end(), envelope.payload.begin(), envelope.payload.end());

  return encoded;
}

inline bool decode_message_envelope(std::string_view encoded, MessageEnvelope& envelope) noexcept
{
  constexpr std::size_t fixed_size = 1 + X25519PublicKey::bytes + 4 + 4;
  if (encoded.size() < fixed_size)
    return false;

  const auto* bytes = reinterpret_cast<const std::uint8_t*>(encoded.data());
  const auto payload_size = static_cast<std::size_t>(detail::read_u32_le(bytes + 1 + X25519PublicKey::bytes + 4));
  if (encoded.size() != fixed_size + payload_size)
    return false;

  envelope.version = bytes[0];
  std::memcpy(envelope.recipient_x25519.data.data(), bytes + 1, X25519PublicKey::bytes);
  envelope.ttl_seconds = detail::read_u32_le(bytes + 1 + X25519PublicKey::bytes);
  envelope.payload.assign(bytes + fixed_size, bytes + fixed_size + payload_size);
  return true;
}
} // namespace arq_messaging
