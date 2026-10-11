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
#include "arqmq/command_registry.hpp"
#include "cryptonote_protocol/arqnet_auth.h"

TEST(arqnet_auth, unknown_curve_peer_is_denied)
{
  EXPECT_EQ(arqnet::IncomingCurveDecision::Denied, arqnet::decide_incoming_curve_peer(false));
  EXPECT_EQ(arqnet::IncomingCurveDecision::Denied, arqnet::decide_incoming_curve_peer_from_pubkey_size(false, 32));
}

TEST(arqnet_auth, registered_sn_curve_peer_is_accepted)
{
  EXPECT_EQ(arqnet::IncomingCurveDecision::ServiceNode, arqnet::decide_incoming_curve_peer(true));
  EXPECT_EQ(arqnet::IncomingCurveDecision::ServiceNode, arqnet::decide_incoming_curve_peer_from_pubkey_size(true, 32));
}

TEST(arqnet_auth, wrong_pubkey_size_is_denied_even_if_mapped)
{
  EXPECT_EQ(arqnet::IncomingCurveDecision::Denied, arqnet::decide_incoming_curve_peer_from_pubkey_size(true, 0));
  EXPECT_EQ(arqnet::IncomingCurveDecision::Denied, arqnet::decide_incoming_curve_peer_from_pubkey_size(true, 31));
  EXPECT_EQ(arqnet::IncomingCurveDecision::Denied, arqnet::decide_incoming_curve_peer_from_pubkey_size(true, 33));
}

TEST(arqnet_auth, vote_ob_requires_service_node_acl)
{
  EXPECT_FALSE(arqmq::authorize_request("vote_ob", arqmq::CategoryAcl::Denied, 16));
  EXPECT_FALSE(arqmq::authorize_request("vote_ob", arqmq::CategoryAcl::Basic, 16));
  EXPECT_TRUE(arqmq::authorize_request("vote_ob", arqmq::CategoryAcl::ServiceNode, 16));
}

TEST(arqnet_auth, native_mesh_legacy_carrier_until_live_stack)
{
  EXPECT_TRUE(arqmq::peer_mesh_is_snnetwork());
  EXPECT_TRUE(arqmq::native_mesh_implementation_ready());
  EXPECT_TRUE(arqmq::native_mesh_ready_at(arqmq::k_hf_native_arqnet_mesh));
  EXPECT_TRUE(arqmq::native_mesh_ready());
  EXPECT_FALSE(arqmq::native_mesh_live_at(arqmq::k_hf_native_arqnet_mesh));
  EXPECT_STREQ("none", arqmq::native_mesh_blocker());
}
