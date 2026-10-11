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
#include "net/levin_base.h"

TEST(p2p_limits, documented_connection_totals_match_defaults)
{
  EXPECT_EQ(P2P_DEFAULT_CONNECTIONS_COUNT_OUT + P2P_DEFAULT_CONNECTIONS_COUNT_IN,
            P2P_DEFAULT_TOTAL_CONNECTIONS);
  EXPECT_EQ(P2P_DEFAULT_CONNECTIONS_COUNT_TEST_OUT + P2P_DEFAULT_CONNECTIONS_COUNT_TEST_IN,
            P2P_DEFAULT_TOTAL_CONNECTIONS_TEST);
  EXPECT_GE(P2P_DEFAULT_TOTAL_CONNECTIONS, 16u);
  EXPECT_LE(P2P_DEFAULT_TOTAL_CONNECTIONS, 128u);
}

TEST(p2p_limits, packet_budget_stays_above_max_transaction)
{
  EXPECT_EQ(P2P_DEFAULT_PACKET_MAX_SIZE, P2P_DEFAULT_PACKET_MAX_SIZE_BYTES);
  EXPECT_EQ(50u, P2P_DEFAULT_PACKET_MAX_SIZE_MB);
  EXPECT_GT(P2P_DEFAULT_PACKET_MAX_SIZE_BYTES, static_cast<uint64_t>(CRYPTONOTE_MAX_TX_SIZE));
  EXPECT_EQ(P2P_DEFAULT_PACKET_MAX_SIZE_BYTES - CRYPTONOTE_MAX_TX_SIZE,
            P2P_DEFAULT_PACKET_MAX_SIZE_HEADROOM_BYTES);
}

TEST(p2p_limits, documented_rate_limits_stay_positive)
{
  EXPECT_EQ(P2P_DEFAULT_LIMIT_RATE_UP, P2P_DEFAULT_LIMIT_RATE_UP_KBPS);
  EXPECT_EQ(P2P_DEFAULT_LIMIT_RATE_DOWN, P2P_DEFAULT_LIMIT_RATE_DOWN_KBPS);
  EXPECT_GT(P2P_DEFAULT_LIMIT_RATE_UP_KBPS, 0u);
  EXPECT_GT(P2P_DEFAULT_LIMIT_RATE_DOWN_KBPS, P2P_DEFAULT_LIMIT_RATE_UP_KBPS);
}

TEST(p2p_limits, preauth_budget_is_below_full_packet_max)
{
  EXPECT_EQ(256u * 1024u, P2P_PREAUTH_PACKET_MAX_SIZE_BYTES);
  EXPECT_LT(P2P_PREAUTH_PACKET_MAX_SIZE_BYTES, P2P_DEFAULT_PACKET_MAX_SIZE_BYTES);
  EXPECT_EQ(250u, P2P_MAX_PEERS_IN_HANDSHAKE);
  EXPECT_EQ(static_cast<uint64_t>(LEVIN_INITIAL_MAX_PACKET_SIZE), P2P_PREAUTH_PACKET_MAX_SIZE_BYTES);
}
