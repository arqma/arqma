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

#include "cryptonote_config.h"
#include "cryptonote_basic/hardfork.h"

TEST(hf19, feature_gates_align)
{
  ASSERT_EQ(HF_VERSION_CLSAG, cryptonote::network_version_19);
  ASSERT_EQ(HF_VERSION_BURN, cryptonote::network_version_19);
  ASSERT_EQ(HF_VERSION_PER_OUTPUT_FEE, cryptonote::network_version_19);
}

TEST(hf19, schedules_include_v19)
{
  auto has_v19 = [](cryptonote::HardFork::ParamsIterator it) {
    for (const auto *p = it.begin(); p != it.end(); ++p)
    {
      if (p->version == cryptonote::network_version_19)
        return true;
    }
    return false;
  };

  ASSERT_TRUE(has_v19(cryptonote::HardFork::get_hardcoded_hard_forks(cryptonote::MAINNET)));
  ASSERT_TRUE(has_v19(cryptonote::HardFork::get_hardcoded_hard_forks(cryptonote::TESTNET)));
  ASSERT_TRUE(has_v19(cryptonote::HardFork::get_hardcoded_hard_forks(cryptonote::STAGENET)));

  ASSERT_EQ(1886030u, cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_19));
  ASSERT_EQ(1200u, cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::TESTNET, cryptonote::network_version_19));
  ASSERT_EQ(220u, cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::STAGENET, cryptonote::network_version_19));
}
