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

#include "wallet/wallet_rpc_validation.h"

TEST(wallet_rpc_validation, destination_and_batch_caps)
{
  EXPECT_TRUE(tools::wallet_rpc::allow_destination_count(1));
  EXPECT_TRUE(tools::wallet_rpc::allow_destination_count(tools::wallet_rpc::max_transfer_destinations));
  EXPECT_FALSE(tools::wallet_rpc::allow_destination_count(tools::wallet_rpc::max_transfer_destinations + 1));

  EXPECT_TRUE(tools::wallet_rpc::allow_payment_id_count(tools::wallet_rpc::max_payment_ids_per_request));
  EXPECT_FALSE(tools::wallet_rpc::allow_payment_id_count(tools::wallet_rpc::max_payment_ids_per_request + 1));

  EXPECT_TRUE(tools::wallet_rpc::allow_address_book_index_count(tools::wallet_rpc::max_address_book_indices_per_request));
  EXPECT_FALSE(tools::wallet_rpc::allow_address_book_index_count(tools::wallet_rpc::max_address_book_indices_per_request + 1));

  EXPECT_TRUE(tools::wallet_rpc::allow_subaddr_index_count(tools::wallet_rpc::max_subaddr_indices_per_request));
  EXPECT_FALSE(tools::wallet_rpc::allow_subaddr_index_count(tools::wallet_rpc::max_subaddr_indices_per_request + 1));
}

TEST(wallet_rpc_validation, payment_id_hex)
{
  EXPECT_TRUE(tools::wallet_rpc::is_valid_payment_id_hex("0123456789abcdef"));
  EXPECT_TRUE(tools::wallet_rpc::is_valid_payment_id_hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
  EXPECT_FALSE(tools::wallet_rpc::is_valid_payment_id_hex(""));
  EXPECT_FALSE(tools::wallet_rpc::is_valid_payment_id_hex("abcd"));
  EXPECT_FALSE(tools::wallet_rpc::is_valid_payment_id_hex("zz23456789abcdef"));
}

TEST(wallet_rpc_validation, clamp_list_limit)
{
  EXPECT_EQ(100u, tools::wallet_rpc::clamp_list_limit(0));
  EXPECT_EQ(1000u, tools::wallet_rpc::clamp_list_limit(5000));
  EXPECT_EQ(42u, tools::wallet_rpc::clamp_list_limit(42));
}
