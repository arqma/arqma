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

#include "wallet/wallet_rpc_auth.h"
#include "wallet/wallet_rpc_server_error_codes.h"

namespace
{
  struct dummy_error
  {
    int64_t code = 0;
    std::string message;
  };
}

TEST(wallet_rpc_auth, restricted_denies_spend_and_key_methods)
{
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("transfer"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("query_key"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("relay_tx"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("get_tx_key"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("start_mining"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("stake"));

  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("get_transfers_csv"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("create_wallet"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("open_wallet"));
  EXPECT_TRUE(tools::wallet_rpc::method_requires_full_access("get_tx_proof"));
  EXPECT_FALSE(tools::wallet_rpc::allow_wallet_rpc_method("restore_deterministic_wallet", true));
  EXPECT_TRUE(tools::wallet_rpc::allow_wallet_rpc_method("get_balance", true));
}

TEST(wallet_rpc_auth, deny_if_restricted_fills_error)
{
  dummy_error er;
  EXPECT_FALSE(tools::wallet_rpc::deny_if_restricted(false, er));
  EXPECT_EQ(0, er.code);

  EXPECT_TRUE(tools::wallet_rpc::deny_if_restricted(true, er));
  EXPECT_EQ(WALLET_RPC_ERROR_CODE_DENIED, er.code);
  EXPECT_EQ("Command unavailable in restricted mode.", er.message);

  dummy_error stake_er;
  EXPECT_TRUE(tools::wallet_rpc::deny_if_restricted(true, stake_er, "Staking command not available in restricted mode."));
  EXPECT_EQ("Staking command not available in restricted mode.", stake_er.message);
}
