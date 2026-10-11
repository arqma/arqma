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

#include "onion_layer.hpp"
#include "onion_forward.hpp"

namespace arq_messaging {
std::error_code wrap_onion_layer(const X25519PublicKey& hop_pubkey, const std::vector<std::uint8_t>& inner,
                                 std::vector<std::uint8_t>& outer) noexcept
{
  if (inner.size() > max_onion_ciphertext_bytes)
    return std::make_error_code(std::errc::message_size);
  return seal_payload(hop_pubkey, inner, outer);
}

std::error_code peel_onion_layer(const Identity& hop, const std::vector<std::uint8_t>& outer,
                                 std::vector<std::uint8_t>& inner) noexcept
{
  if (outer.size() > max_onion_ciphertext_bytes)
    return std::make_error_code(std::errc::message_size);
  return open_payload(hop, outer, inner);
}

std::error_code build_onion(const std::vector<X25519PublicKey>& hop_pubkeys, const std::vector<std::uint8_t>& payload,
                            std::vector<std::uint8_t>& onion) noexcept
{
  if (hop_pubkeys.empty() || hop_pubkeys.size() > OnionRequest::hop_count)
    return std::make_error_code(std::errc::invalid_argument);
  if (payload.size() > max_onion_payload_bytes)
    return std::make_error_code(std::errc::message_size);

  onion = payload;
  // Innermost sealed to last hop; outermost to first hop.
  for (std::size_t i = hop_pubkeys.size(); i > 0; --i) {
    std::vector<std::uint8_t> next;
    if (auto ec = wrap_onion_layer(hop_pubkeys[i - 1], onion, next)) {
      onion.clear();
      return ec;
    }
    if (next.size() > max_onion_ciphertext_bytes) {
      onion.clear();
      return std::make_error_code(std::errc::message_size);
    }
    onion = std::move(next);
  }
  return {};
}

std::error_code peel_onion(const std::vector<Identity>& hop_identities, const std::vector<std::uint8_t>& onion_in,
                           std::vector<std::uint8_t>& payload) noexcept
{
  if (hop_identities.empty() || hop_identities.size() > OnionRequest::hop_count)
    return std::make_error_code(std::errc::invalid_argument);
  if (onion_in.size() > max_onion_ciphertext_bytes)
    return std::make_error_code(std::errc::message_size);

  std::vector<std::uint8_t> current = onion_in;
  for (const auto& hop : hop_identities) {
    std::vector<std::uint8_t> next;
    if (auto ec = peel_onion_layer(hop, current, next)) {
      payload.clear();
      return ec;
    }
    current = std::move(next);
  }
  if (current.size() > max_onion_payload_bytes) {
    payload.clear();
    return std::make_error_code(std::errc::message_size);
  }
  payload = std::move(current);
  return {};
}

std::error_code compose_onion_route(const std::vector<X25519PublicKey>& hop_pubkeys,
                                    const std::vector<std::string>& hop_urls, const std::vector<std::uint8_t>& payload,
                                    std::vector<std::uint8_t>& onion)
{
  if (hop_pubkeys.empty() || hop_pubkeys.size() != hop_urls.size() || hop_pubkeys.size() > OnionRequest::hop_count)
    return std::make_error_code(std::errc::invalid_argument);
  if (payload.size() > max_onion_payload_bytes)
    return std::make_error_code(std::errc::message_size);

  std::vector<std::uint8_t> current = payload;
  for (std::size_t i = hop_pubkeys.size(); i-- > 0;) {
    std::vector<std::uint8_t> wrapped;
    if (auto ec = wrap_onion_layer(hop_pubkeys[i], current, wrapped)) {
      onion.clear();
      return ec;
    }
    current = std::move(wrapped);
    if (i == 0)
      break;
    std::vector<std::uint8_t> framed;
    if (!encode_onion_forward(hop_urls[i], current, framed)) {
      onion.clear();
      return std::make_error_code(std::errc::invalid_argument);
    }
    current = std::move(framed);
  }
  if (current.size() > max_onion_ciphertext_bytes) {
    onion.clear();
    return std::make_error_code(std::errc::message_size);
  }
  onion = std::move(current);
  return {};
}
} // namespace arq_messaging
