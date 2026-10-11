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

#include "rpc_validation.h"

#include "common/hex.h"

#include <algorithm>

namespace cryptonote {
namespace rpc {
uint64_t clamp_limit(uint64_t requested, uint64_t default_limit, uint64_t max_limit) noexcept
{
  const uint64_t value = requested == 0 ? default_limit : requested;
  return max_limit == 0 ? value : std::min(value, max_limit);
}

bool validate_nonempty_hex(std::string_view value, size_t expected_bytes) noexcept
{
  return !value.empty() && value.size() == expected_bytes * 2 && tools::is_hex(value);
}

pagination pagination::make(uint64_t requested_offset, uint64_t requested_limit, uint64_t fallback_limit,
                            uint64_t hard_max_limit) noexcept
{
  return {requested_offset, clamp_limit(requested_limit, fallback_limit, hard_max_limit)};
}
} // namespace rpc
} // namespace cryptonote
