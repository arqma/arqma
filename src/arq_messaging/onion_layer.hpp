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
#include "onion_request.hpp"
#include "sealed_sender.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace arq_messaging {
/// Soft caps for onion ciphertext growth (3 hops × seal overhead + payload).
constexpr std::size_t max_onion_payload_bytes = 64 * 1024;
constexpr std::size_t max_onion_ciphertext_bytes = 96 * 1024;

/// Wrap plaintext for a single hop (libsodium sealed-box to hop pubkey).
std::error_code wrap_onion_layer(const X25519PublicKey& hop_pubkey, const std::vector<std::uint8_t>& inner,
                                 std::vector<std::uint8_t>& outer) noexcept;

/// Peel one hop with the local identity.
std::error_code peel_onion_layer(const Identity& hop, const std::vector<std::uint8_t>& outer,
                                 std::vector<std::uint8_t>& inner) noexcept;

/// Build a multi-hop onion sealed successively to hops[0]..hops[n-1]
/// (outermost sealed to hops[0]). `hop_pubkeys` must be non-empty and ≤ hop_count.
std::error_code build_onion(const std::vector<X25519PublicKey>& hop_pubkeys, const std::vector<std::uint8_t>& payload,
                            std::vector<std::uint8_t>& onion) noexcept;

/// Peel successive layers with matching hop identities (outermost first).
std::error_code peel_onion(const std::vector<Identity>& hop_identities, const std::vector<std::uint8_t>& onion,
                           std::vector<std::uint8_t>& payload) noexcept;

/// Wrap `payload` for successive router hops (outermost = hops[0]). Between hops the
/// inner ciphertext is framed with the next hop URL (`encode_onion_forward`) so each
/// `arqma-router` peels one layer and POSTs the rest. `hop_pubkeys.size()` must equal
/// `hop_urls.size()`, be non-empty, and stay ≤ hop_count.
std::error_code compose_onion_route(const std::vector<X25519PublicKey>& hop_pubkeys,
                                    const std::vector<std::string>& hop_urls, const std::vector<std::uint8_t>& payload,
                                    std::vector<std::uint8_t>& onion);
} // namespace arq_messaging
