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

#include "rpc/rpc_validation.h"

TEST(rpc_validation, clamp_limit_uses_default_and_maximum)
{
  EXPECT_EQ(25u, cryptonote::rpc::clamp_limit(0, 25, 100));
  EXPECT_EQ(100u, cryptonote::rpc::clamp_limit(250, 25, 100));
  EXPECT_EQ(17u, cryptonote::rpc::clamp_limit(17, 25, 100));
}

TEST(rpc_validation, validate_nonempty_hex_checks_length)
{
  EXPECT_TRUE(cryptonote::rpc::validate_nonempty_hex("aabbccdd", 4));
  EXPECT_FALSE(cryptonote::rpc::validate_nonempty_hex("", 4));
  EXPECT_FALSE(cryptonote::rpc::validate_nonempty_hex("aabbcc", 4));
  EXPECT_FALSE(cryptonote::rpc::validate_nonempty_hex("zzbbccdd", 4));
}

TEST(rpc_validation, pagination_builder_clamps_requested_limit)
{
  const auto pagination = cryptonote::rpc::pagination::make(7, 400, 25, 100);
  EXPECT_EQ(7u, pagination.offset);
  EXPECT_EQ(100u, pagination.limit);

  const auto defaults = cryptonote::rpc::pagination::make(3, 0, 25, 100);
  EXPECT_EQ(3u, defaults.offset);
  EXPECT_EQ(25u, defaults.limit);
}

TEST(rpc_validation, batch_request_caps_are_defined)
{
  EXPECT_EQ(100u, cryptonote::rpc::max_tx_hashes_per_request);
  EXPECT_EQ(1000u, cryptonote::rpc::max_key_images_per_request);
  EXPECT_EQ(100u, cryptonote::rpc::max_block_heights_per_request);
  EXPECT_EQ(100u, cryptonote::rpc::max_block_hashes_per_request);
  EXPECT_EQ(1000u, cryptonote::rpc::max_block_headers_range);
  EXPECT_EQ(1000u, cryptonote::rpc::max_service_node_pubkeys_per_request);
}
