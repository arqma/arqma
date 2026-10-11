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

#include "rpc/rpc_auth.h"

TEST(rpc_auth, access_allows_respects_ordering)
{
  EXPECT_TRUE(cryptonote::rpc::access_allows(cryptonote::rpc::AccessLevel::Public, cryptonote::rpc::AccessLevel::Public));
  EXPECT_TRUE(cryptonote::rpc::access_allows(cryptonote::rpc::AccessLevel::Public, cryptonote::rpc::AccessLevel::Operator));
  EXPECT_FALSE(cryptonote::rpc::access_allows(cryptonote::rpc::AccessLevel::Operator, cryptonote::rpc::AccessLevel::Public));
  EXPECT_TRUE(cryptonote::rpc::access_allows(cryptonote::rpc::AccessLevel::Operator, cryptonote::rpc::AccessLevel::Admin));
}

TEST(rpc_auth, daemon_access_level_from_mode)
{
  EXPECT_EQ(cryptonote::rpc::AccessLevel::Admin, cryptonote::rpc::daemon_access_level(false, false));
  EXPECT_EQ(cryptonote::rpc::AccessLevel::Operator, cryptonote::rpc::daemon_access_level(false, true));
  EXPECT_EQ(cryptonote::rpc::AccessLevel::Public, cryptonote::rpc::daemon_access_level(true, true));
}

TEST(rpc_auth, operator_methods_denied_for_public)
{
  EXPECT_TRUE(cryptonote::rpc::method_requires_operator("start_mining"));
  EXPECT_TRUE(cryptonote::rpc::method_requires_operator("stop_daemon"));
  EXPECT_TRUE(cryptonote::rpc::method_requires_operator("prune_blockchain"));
  EXPECT_TRUE(cryptonote::rpc::method_requires_operator("get_service_node_key"));
  EXPECT_TRUE(cryptonote::rpc::method_requires_operator("get_service_node_privkey"));
  EXPECT_FALSE(cryptonote::rpc::method_requires_operator("get_info"));

  EXPECT_FALSE(cryptonote::rpc::allow_rpc_method("start_mining", cryptonote::rpc::AccessLevel::Public));
  EXPECT_TRUE(cryptonote::rpc::allow_rpc_method("start_mining", cryptonote::rpc::AccessLevel::Operator));
  EXPECT_TRUE(cryptonote::rpc::allow_rpc_method("get_info", cryptonote::rpc::AccessLevel::Public));
}
