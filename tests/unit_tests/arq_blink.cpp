// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "gtest/gtest.h"

#include <cstring>
#include <string>

#include "arq_blink/blink.h"

TEST(arq_blink, majority_is_seven_of_ten)
{
  EXPECT_EQ(7u, arq_blink::min_signatures_for_quorum(arq_blink::k_quorum_size));
  EXPECT_EQ(0u, arq_blink::min_signatures_for_quorum(0));
}

TEST(arq_blink, quorum_rotates_from_leader)
{
  const auto q = arq_blink::quorum_indices(100, 20);
  ASSERT_EQ(arq_blink::k_quorum_size, q.size());
  EXPECT_EQ(arq_blink::leader_index(100, 20), q.front());
  EXPECT_FALSE(arq_blink::wire_connected());
  EXPECT_EQ(std::string("blink-wire-not-connected"), arq_blink::blocker());
  arq_blink::set_wire_connected(true);
  EXPECT_TRUE(arq_blink::wire_connected());
  EXPECT_EQ(std::string("none"), arq_blink::blocker());
  arq_blink::set_wire_connected(false);
  EXPECT_FALSE(arq_blink::wire_connected());
  EXPECT_EQ(std::string("blink-wire-not-connected"), arq_blink::blocker());
}

TEST(arq_blink, relay_vote_encode_decode_roundtrip)
{
  EXPECT_EQ(141u, arq_blink::k_relay_vote_bytes);
  EXPECT_EQ(1u, arq_blink::k_relay_vote_version);

  crypto::public_key pub{};
  crypto::secret_key sec{};
  crypto::generate_keys(pub, sec);
  crypto::hash txid{};
  txid.data[0] = 9;
  const auto hashed = arq_blink::tx_round_hash(77, txid);
  const auto sig = arq_blink::sign_tx(hashed, pub, sec);

  arq_blink::RelayVote vote{};
  vote.height = 77;
  vote.txid = txid;
  vote.validator_index = 3;
  vote.pub = pub;
  vote.signature = sig;

  std::string blob;
  ASSERT_TRUE(arq_blink::encode_relay_vote(vote, blob));
  EXPECT_EQ(arq_blink::k_relay_vote_bytes, blob.size());
  EXPECT_EQ(arq_blink::k_relay_vote_version, static_cast<std::uint8_t>(blob[0]));

  arq_blink::RelayVote decoded{};
  ASSERT_TRUE(arq_blink::decode_relay_vote(blob, decoded));
  EXPECT_EQ(vote.height, decoded.height);
  EXPECT_EQ(vote.txid, decoded.txid);
  EXPECT_EQ(vote.validator_index, decoded.validator_index);
  EXPECT_EQ(0, std::memcmp(&vote.pub, &decoded.pub, sizeof(vote.pub)));
  EXPECT_EQ(0, std::memcmp(&vote.signature, &decoded.signature, sizeof(vote.signature)));

  EXPECT_FALSE(arq_blink::decode_relay_vote(std::string_view{blob}.substr(1), decoded));
  blob[0] = 2;
  EXPECT_FALSE(arq_blink::decode_relay_vote(blob, decoded));
}

TEST(arq_blink, sign_verify_and_collector_majority)
{
  crypto::public_key pub{};
  crypto::secret_key sec{};
  crypto::generate_keys(pub, sec);
  crypto::hash txid{};
  txid.data[0] = 7;
  const auto hashed = arq_blink::tx_round_hash(42, txid);
  const auto sig = arq_blink::sign_tx(hashed, pub, sec);
  EXPECT_TRUE(arq_blink::check_tx_signature(hashed, pub, sig));

  arq_blink::Collector col;
  col.reset(42, txid);
  for (std::uint32_t i = 0; i < 7; ++i) {
    arq_blink::Vote vote{};
    vote.validator_index = i;
    vote.pub = pub;
    vote.signature = sig;
    EXPECT_TRUE(col.add_vote(vote, hashed));
  }
  EXPECT_EQ(7u, col.signature_count());
  EXPECT_TRUE(col.majority_ok());
  EXPECT_FALSE(arq_blink::collector().majority_ok());
}
