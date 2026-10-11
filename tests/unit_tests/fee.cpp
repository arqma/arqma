// Copyright (c) 2018 - 2026, The Arqma Network
// Copyright (c) 2014-2020, The Monero Project
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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"

#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_config.h"
#include "cryptonote_core/blockchain.h"

using namespace cryptonote;

namespace
{
  // Pre-per-byte fee path still quantizes to 8 display decimals.
  uint64_t clamp_fee(uint64_t fee)
  {
    static uint64_t mask = 0;
    if (mask == 0)
    {
      mask = 1;
      for (size_t n = PER_KB_FEE_QUANTIZATION_DECIMALS; n < config::blockchain_settings::ARQMA_DECIMALS; ++n)
        mask *= 10;
    }
    return (fee + mask - 1) / mask * mask;
  }
}

TEST(fee, pre_per_byte_fee_scales_with_median_weight)
{
  constexpr uint8_t version = network_version_12;
  const uint64_t reward = DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD;
  const size_t min_weight = get_min_block_weight(version);

  auto at_min = Blockchain::get_dynamic_base_fee(reward, min_weight, version);
  EXPECT_EQ(0u, at_min.second);
  EXPECT_EQ(clamp_fee(DYNAMIC_FEE_PER_BYTE_BASE_FEE_V13), at_min.first);

  auto doubled = Blockchain::get_dynamic_base_fee(reward, min_weight * 2, version);
  EXPECT_EQ(0u, doubled.second);
  EXPECT_EQ(clamp_fee(DYNAMIC_FEE_PER_BYTE_BASE_FEE_V13 / 2), doubled.first);
}

TEST(fee, per_byte_fee_adds_hf19_output_fee)
{
  constexpr uint8_t version = network_version_19;
  const uint64_t reward = 10000000000ull;
  const size_t median = get_min_block_weight(version);

  auto fees = Blockchain::get_dynamic_base_fee(reward, median, version);
  EXPECT_GT(fees.first, 0u);
  EXPECT_EQ(HF_19_OUTPUT_FEE, fees.second);

  auto smaller_reward = Blockchain::get_dynamic_base_fee(reward / 2, median, version);
  EXPECT_LT(smaller_reward.first, fees.first);
  EXPECT_EQ(HF_19_OUTPUT_FEE, smaller_reward.second);
}

TEST(fee, quantization_mask_matches_display_decimals)
{
  uint64_t expected = 1;
  for (size_t n = PER_KB_FEE_QUANTIZATION_DECIMALS; n < config::blockchain_settings::ARQMA_DECIMALS; ++n)
    expected *= 10;
  EXPECT_EQ(expected, Blockchain::get_fee_quantization_mask());
}
