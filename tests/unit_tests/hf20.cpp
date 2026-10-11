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

#include "gtest/gtest.h"

#include "arqmq/arqmq.h"
#include "cryptonote_basic/hardfork.h"
#include "cryptonote_config.h"

TEST(hf20, feature_gate_aligns_with_arqmq_constant)
{
  ASSERT_EQ(HF_VERSION_NATIVE_ARQNET_MESH, cryptonote::network_version_20);
  ASSERT_EQ(arqmq::k_hf_native_arqnet_mesh, static_cast<uint8_t>(cryptonote::network_version_20));
}

TEST(hf20, schedules_stagenet_testnet_and_mainnet)
{
  ASSERT_EQ(240u,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::STAGENET, cryptonote::network_version_20));
  ASSERT_EQ(1300u,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::TESTNET, cryptonote::network_version_20));
  ASSERT_EQ(MAINNET_HARD_FORK_20_HEIGHT,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_20));
  ASSERT_EQ(4000000u, MAINNET_HARD_FORK_20_HEIGHT);
  // Until 4_000_000 the chain stays on HF19 (compatibility with current mainnet).
  ASSERT_EQ(1886030u,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_19));
  ASSERT_LT(cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_19),
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_20));
}

TEST(hf20, mesh_cutover_requires_hf_and_implementation)
{
  EXPECT_FALSE(arqmq::hf_permits_native_mesh(static_cast<uint8_t>(cryptonote::network_version_19)));
  EXPECT_TRUE(arqmq::hf_permits_native_mesh(static_cast<uint8_t>(cryptonote::network_version_20)));
  EXPECT_TRUE(arqmq::native_mesh_implementation_ready());
  EXPECT_TRUE(arqmq::native_mesh_ready());
  EXPECT_TRUE(arqmq::native_mesh_ready_at(static_cast<uint8_t>(cryptonote::network_version_20)));
  EXPECT_FALSE(arqmq::native_mesh_ready_at(static_cast<uint8_t>(cryptonote::network_version_19)));
  // No CURVE stack in this test → live relay stays SNNetwork (legacy fallback).
  EXPECT_FALSE(arqmq::native_mesh_live_at(static_cast<uint8_t>(cryptonote::network_version_20)));
  EXPECT_FALSE(arqmq::hf_requires_native_mesh_exclusive(static_cast<uint8_t>(cryptonote::network_version_20)));
  EXPECT_TRUE(arqmq::hf_requires_native_mesh_exclusive(static_cast<uint8_t>(cryptonote::network_version_21)));
  EXPECT_STREQ("none", arqmq::native_mesh_blocker());
}
