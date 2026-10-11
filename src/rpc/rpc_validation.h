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

namespace cryptonote {
namespace rpc {
uint64_t clamp_limit(uint64_t requested, uint64_t default_limit, uint64_t max_limit) noexcept;

bool validate_nonempty_hex(std::string_view value, size_t expected_bytes) noexcept;

struct pagination
{
  static constexpr uint64_t default_limit = 100;
  static constexpr uint64_t max_limit = 1000;

  uint64_t offset = 0;
  uint64_t limit = default_limit;

  static pagination make(uint64_t requested_offset = 0, uint64_t requested_limit = 0,
                         uint64_t fallback_limit = default_limit, uint64_t hard_max_limit = max_limit) noexcept;
};

/// Soft caps for batch RPC requests to reduce easy DoS amplification.
constexpr uint64_t max_tx_hashes_per_request = 100;
constexpr uint64_t max_key_images_per_request = 1000;
constexpr uint64_t max_block_heights_per_request = 100;
constexpr uint64_t max_block_hashes_per_request = 100;
constexpr uint64_t max_block_headers_range = 1000;
constexpr uint64_t max_service_node_pubkeys_per_request = 1000;
} // namespace rpc
} // namespace cryptonote
