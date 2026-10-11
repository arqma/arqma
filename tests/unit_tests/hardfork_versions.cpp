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

using namespace cryptonote;

TEST(hardfork_versions, network_version_ordering)
{
  EXPECT_LT(network_version_12, network_version_13);
  EXPECT_LT(network_version_16, network_version_19);
  EXPECT_LT(network_version_19, network_version_20);
  EXPECT_LT(network_version_20, network_version_21);
  EXPECT_EQ(19u, static_cast<unsigned>(network_version_19));
  EXPECT_EQ(20u, static_cast<unsigned>(network_version_20));
  EXPECT_EQ(21u, static_cast<unsigned>(network_version_21));
}

TEST(hardfork_versions, per_byte_and_burn_gates)
{
  EXPECT_EQ(network_version_13, HF_VERSION_PER_BYTE_FEE);
  EXPECT_EQ(network_version_19, HF_VERSION_BURN);
  EXPECT_EQ(network_version_19, HF_VERSION_CLSAG);
  EXPECT_GT(HF_19_OUTPUT_FEE, 0u);
}
