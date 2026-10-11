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

#include "rpc/rpc_validation.h"

namespace tools
{
namespace wallet_rpc
{
  /// Soft caps aligned with daemon RPC batch policy. Per-transaction consensus
  /// still enforces BULLETPROOF_MAX_OUTPUTS (16) inside transfer construction;
  /// the RPC destination cap is higher so transfer_split can accept multi-tx
  /// destination sets without amplifying DNS/address work unboundedly.
  constexpr uint64_t max_transfer_destinations = 100;
  constexpr uint64_t max_payment_ids_per_request = 100;
  constexpr uint64_t max_address_book_indices_per_request = 1000;
  constexpr uint64_t max_subaddr_indices_per_request = 1000;

  inline bool allow_destination_count(uint64_t count) noexcept
  {
    return count <= max_transfer_destinations;
  }

  inline bool allow_payment_id_count(uint64_t count) noexcept
  {
    return count <= max_payment_ids_per_request;
  }

  inline bool allow_address_book_index_count(uint64_t count) noexcept
  {
    return count <= max_address_book_indices_per_request;
  }

  inline bool allow_subaddr_index_count(uint64_t count) noexcept
  {
    return count <= max_subaddr_indices_per_request;
  }

  /// Accept legacy 8-byte or 32-byte payment id hex (empty rejected).
  inline bool is_valid_payment_id_hex(std::string_view value) noexcept
  {
    return cryptonote::rpc::validate_nonempty_hex(value, 8)
        || cryptonote::rpc::validate_nonempty_hex(value, 32);
  }

  /// Clamp a wallet list page size the same way daemon RPC does.
  inline uint64_t clamp_list_limit(uint64_t requested, uint64_t default_limit = 100,
                                   uint64_t max_limit = 1000) noexcept
  {
    return cryptonote::rpc::clamp_limit(requested, default_limit, max_limit);
  }
}
}
