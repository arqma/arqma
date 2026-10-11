// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "gtest/gtest.h"

#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/hardfork.h"
#include "cryptonote_config.h"
#include "cryptonote_core/pulse.h"

TEST(hf21, feature_gate_aligns)
{
  ASSERT_EQ(HF_VERSION_PULSE_EXCLUSIVE, cryptonote::network_version_21);
  ASSERT_EQ(21u, static_cast<unsigned>(cryptonote::network_version_21));
  ASSERT_EQ(service_nodes::pulse::k_hf_exclusive, static_cast<uint8_t>(cryptonote::network_version_21));
}

TEST(hf21, schedules_after_hf20)
{
  ASSERT_EQ(260u,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::STAGENET, cryptonote::network_version_21));
  ASSERT_EQ(1400u,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::TESTNET, cryptonote::network_version_21));
  ASSERT_EQ(MAINNET_HARD_FORK_21_HEIGHT,
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_21));
  ASSERT_EQ(5000000u, MAINNET_HARD_FORK_21_HEIGHT);
  ASSERT_LT(cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_20),
            cryptonote::HardFork::get_hardcoded_hard_fork_height(cryptonote::MAINNET, cryptonote::network_version_21));
}

TEST(pulse, modes_follow_hard_fork)
{
  using service_nodes::pulse::SnMode;
  EXPECT_EQ(SnMode::Legacy, service_nodes::pulse::sn_operating_mode(19));
  EXPECT_EQ(SnMode::Hybrid, service_nodes::pulse::sn_operating_mode(20));
  EXPECT_EQ(SnMode::Exclusive, service_nodes::pulse::sn_operating_mode(21));
  EXPECT_STREQ("hybrid", service_nodes::pulse::to_string(SnMode::Hybrid));
  EXPECT_STREQ("exclusive", service_nodes::pulse::to_string(SnMode::Exclusive));
  EXPECT_FALSE(service_nodes::pulse::hybrid_sn_permitted(19));
  EXPECT_TRUE(service_nodes::pulse::hybrid_sn_permitted(20));
  EXPECT_FALSE(service_nodes::pulse::exclusive_sn_required(20));
  EXPECT_TRUE(service_nodes::pulse::exclusive_sn_required(21));
}

TEST(pulse, pow_stays_required_in_hybrid)
{
  EXPECT_FALSE(service_nodes::pulse::pow_replacement_ready());
  EXPECT_TRUE(service_nodes::pulse::pow_required_for_block(19));
  EXPECT_TRUE(service_nodes::pulse::pow_required_for_block(20));
  EXPECT_TRUE(service_nodes::pulse::pow_required_for_block(21));
  EXPECT_STREQ("pulse-pow-replacement-not-ready", service_nodes::pulse::blocker());
}

TEST(pulse, leader_and_quorum_are_deterministic)
{
  EXPECT_EQ(0u, service_nodes::pulse::leader_index(100, 0));
  EXPECT_TRUE(service_nodes::pulse::quorum_indices(100, 0).empty());

  const size_t a = service_nodes::pulse::leader_index(42, 17);
  const size_t b = service_nodes::pulse::leader_index(42, 17);
  const size_t c = service_nodes::pulse::leader_index(43, 17);
  EXPECT_EQ(a, b);
  EXPECT_LT(a, 17u);
  EXPECT_NE(a, c);

  const auto q = service_nodes::pulse::quorum_indices(42, 17);
  ASSERT_EQ(service_nodes::pulse::k_quorum_size, q.size());
  EXPECT_EQ(a, q.front());
  for (size_t i = 1; i < q.size(); ++i)
    EXPECT_EQ((q[i - 1] + 1) % 17, q[i]);

  EXPECT_EQ((a + 1) % 17, service_nodes::pulse::leader_index(42, 17, 1));
  EXPECT_EQ((a + 3) % 17, service_nodes::pulse::leader_index(42, 17, 3));
  const auto q1 = service_nodes::pulse::quorum_indices(42, 17, 1);
  ASSERT_EQ(service_nodes::pulse::k_quorum_size, q1.size());
  EXPECT_EQ((a + 1) % 17, q1.front());
}

TEST(pulse, quorum_shrinks_when_few_service_nodes)
{
  const auto q = service_nodes::pulse::quorum_indices(7, 3);
  ASSERT_EQ(3u, q.size());
}

TEST(pulse, majority_threshold_is_two_thirds)
{
  EXPECT_EQ(0u, service_nodes::pulse::min_signatures_for_quorum(0));
  EXPECT_EQ(1u, service_nodes::pulse::min_signatures_for_quorum(1));
  EXPECT_EQ(2u, service_nodes::pulse::min_signatures_for_quorum(3));
  EXPECT_EQ(7u, service_nodes::pulse::min_signatures_for_quorum(11));
  EXPECT_FALSE(service_nodes::pulse::majority_reached(6, 11));
  EXPECT_TRUE(service_nodes::pulse::majority_reached(7, 11));
}

TEST(pulse, wait_window_round_from_timestamps)
{
  EXPECT_EQ(0, service_nodes::pulse::round_from_timestamps(100, 100));
  EXPECT_EQ(0, service_nodes::pulse::round_from_timestamps(100, 99));
  EXPECT_EQ(0, service_nodes::pulse::round_from_timestamps(0, 1'000'000));
  EXPECT_EQ(0, service_nodes::pulse::round_from_timestamps(100, 114));
  EXPECT_EQ(1, service_nodes::pulse::round_from_timestamps(100, 115));
  EXPECT_EQ(7, service_nodes::pulse::round_from_timestamps(100, 100 + 7 * service_nodes::pulse::k_round_window_seconds));
  EXPECT_EQ(service_nodes::pulse::k_max_round,
            service_nodes::pulse::round_from_timestamps(100, 100 + 1000));
  EXPECT_TRUE(service_nodes::pulse::extra_round_matches_timestamps(100, 114, 0));
  EXPECT_TRUE(service_nodes::pulse::extra_round_matches_timestamps(100, 115, 1));
  EXPECT_FALSE(service_nodes::pulse::extra_round_matches_timestamps(100, 115, 0));
}

TEST(pulse, round_hash_is_deterministic)
{
  crypto::hash prev{};
  prev.data[0] = 7;
  const crypto::hash a = service_nodes::pulse::round_hash(100, prev, 0);
  const crypto::hash b = service_nodes::pulse::round_hash(100, prev, 0);
  const crypto::hash c = service_nodes::pulse::round_hash(101, prev, 0);
  const crypto::hash d = service_nodes::pulse::round_hash(100, prev, 1);
  crypto::hash payload{};
  payload.data[0] = 3;
  const crypto::hash e = service_nodes::pulse::round_hash(100, prev, 0, payload);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
  EXPECT_FALSE(a == d);
  EXPECT_FALSE(a == e);
}

TEST(pulse, miner_payload_hash_is_deterministic)
{
  crypto::public_key miner{};
  crypto::secret_key sec{};
  crypto::generate_keys(miner, sec);
  std::vector<crypto::hash> txs(2);
  txs[0].data[0] = 1;
  txs[1].data[1] = 2;
  const crypto::hash a = service_nodes::pulse::miner_payload_hash(100, txs, miner);
  const crypto::hash b = service_nodes::pulse::miner_payload_hash(100, txs, miner);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == service_nodes::pulse::miner_payload_hash(101, txs, miner));
  EXPECT_FALSE(a == service_nodes::pulse::miner_payload_hash(100, {}, miner));
  crypto::public_key other{};
  crypto::secret_key other_sec{};
  crypto::generate_keys(other, other_sec);
  EXPECT_FALSE(a == service_nodes::pulse::miner_payload_hash(100, txs, other));
  EXPECT_FALSE(a == crypto::null_hash);
}

namespace
{
struct pulse_keys
{
  crypto::public_key pub{};
  crypto::secret_key sec{};
};

std::vector<pulse_keys> make_pulse_keys(size_t n)
{
  std::vector<pulse_keys> out(n);
  for (auto &k : out)
    crypto::generate_keys(k.pub, k.sec);
  return out;
}

std::vector<crypto::public_key> pubs_of(const std::vector<pulse_keys> &keys)
{
  std::vector<crypto::public_key> out;
  out.reserve(keys.size());
  for (const auto &k : keys)
    out.push_back(k.pub);
  return out;
}

crypto::hash sample_payload()
{
  crypto::hash hashed{};
  hashed.data[0] = static_cast<char>(0xA5);
  return hashed;
}

service_nodes::pulse::RelayVote make_relay_vote(const uint64_t height, const crypto::hash &prev,
                                                const uint8_t round, const uint32_t lead,
                                                const uint32_t validator_index, const crypto::hash &payload,
                                                const pulse_keys &keys)
{
  service_nodes::pulse::RelayVote vote{};
  vote.height = height;
  vote.round = round;
  vote.leader_index = lead;
  vote.validator_index = validator_index;
  vote.prev_id = prev;
  vote.payload_hash = payload;
  vote.signature = service_nodes::pulse::sign_round(
      service_nodes::pulse::round_hash(height, prev, round, payload), keys.pub, keys.sec);
  return vote;
}

int g_pulse_relay_count = 0;
void count_pulse_relay(const service_nodes::pulse::RelayVote &)
{
  ++g_pulse_relay_count;
}
} // namespace

TEST(pulse, local_round_and_majority_verify)
{
  constexpr uint64_t height = 4242;
  constexpr uint8_t round = 0;
  crypto::hash prev{};
  prev.data[3] = 9;
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  ASSERT_EQ(11u, q.size());
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  ASSERT_EQ(11u, qpubs.size());

  cryptonote::tx_extra_pulse_round extra{};
  extra.height = height;
  extra.round = round;
  extra.leader_index = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size()));
  extra.payload_hash = sample_payload();
  const crypto::hash hashed = service_nodes::pulse::round_hash(height, prev, round, extra.payload_hash);
  for (size_t i = 0; i < 7; ++i)
  {
    const size_t sn = q[i];
    extra.votes.push_back({service_nodes::pulse::sign_round(hashed, keys[sn].pub, keys[sn].sec),
                           static_cast<uint32_t>(i)});
  }

  EXPECT_TRUE(service_nodes::pulse::verify_round(extra, height, prev, pubs.size(), qpubs, 7));
  EXPECT_TRUE(service_nodes::pulse::verify_majority_certificate(extra, height, prev, pubs.size(), qpubs));
  auto too_many = extra;
  too_many.votes.push_back({service_nodes::pulse::sign_round(hashed, keys[q[7]].pub, keys[q[7]].sec), 7});
  EXPECT_TRUE(service_nodes::pulse::verify_round(too_many, height, prev, pubs.size(), qpubs, 7));
  EXPECT_FALSE(service_nodes::pulse::verify_majority_certificate(too_many, height, prev, pubs.size(), qpubs));

  auto too_few = extra;
  too_few.votes.pop_back();
  EXPECT_FALSE(service_nodes::pulse::verify_round(too_few, height, prev, pubs.size(), qpubs, 7));
  EXPECT_FALSE(service_nodes::pulse::verify_majority_certificate(too_few, height, prev, pubs.size(), qpubs));
  EXPECT_TRUE(service_nodes::pulse::verify_round(too_few, height, prev, pubs.size(), qpubs, 1));

  auto unbound = extra;
  unbound.payload_hash = crypto::null_hash;
  EXPECT_FALSE(service_nodes::pulse::verify_majority_certificate(unbound, height, prev, pubs.size(), qpubs));
  auto wrong_payload = extra;
  wrong_payload.payload_hash.data[1] = 0x11;
  EXPECT_FALSE(service_nodes::pulse::verify_majority_certificate(wrong_payload, height, prev, pubs.size(), qpubs));

  auto bad_height = extra;
  EXPECT_FALSE(service_nodes::pulse::verify_round(bad_height, height + 1, prev, pubs.size(), qpubs, 1));

  auto bad_leader = extra;
  bad_leader.leader_index = extra.leader_index + 1;
  EXPECT_FALSE(service_nodes::pulse::verify_round(bad_leader, height, prev, pubs.size(), qpubs, 1));
}

TEST(pulse, wrong_signer_is_rejected)
{
  constexpr uint64_t height = 99;
  crypto::hash prev{};
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  ASSERT_GE(q.size(), 2u);
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  const crypto::hash hashed = service_nodes::pulse::round_hash(height, prev, 0);
  cryptonote::tx_extra_pulse_round extra{};
  extra.height = height;
  extra.round = 0;
  extra.leader_index = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size()));
  // Second quorum member signs but claims the leader slot.
  extra.votes.push_back({service_nodes::pulse::sign_round(hashed, keys[q[1]].pub, keys[q[1]].sec), 0});
  EXPECT_FALSE(service_nodes::pulse::verify_round(extra, height, prev, pubs.size(), qpubs, 1));
}

TEST(pulse, try_local_round_only_for_quorum_members)
{
  constexpr uint64_t height = 500;
  crypto::hash prev{};
  const auto keys = make_pulse_keys(17);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  ASSERT_EQ(11u, q.size());

  size_t outsider = service_nodes::pulse::k_not_in_quorum;
  for (size_t i = 0; i < pubs.size(); ++i)
  {
    if (service_nodes::pulse::validator_slot(height, pubs, pubs[i]) == service_nodes::pulse::k_not_in_quorum)
    {
      outsider = i;
      break;
    }
  }
  ASSERT_NE(service_nodes::pulse::k_not_in_quorum, outsider);

  cryptonote::tx_extra_pulse_round extra{};
  EXPECT_FALSE(service_nodes::pulse::try_local_round(height, prev, 0, keys[outsider].pub, keys[outsider].sec, pubs, extra));

  const size_t lead = q.front();
  ASSERT_TRUE(service_nodes::pulse::try_local_round(height, prev, 0, keys[lead].pub, keys[lead].sec, pubs, extra));
  EXPECT_EQ(0u, extra.votes.front().validator_index);
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  EXPECT_TRUE(service_nodes::pulse::verify_round(extra, height, prev, pubs.size(), qpubs, 1));
}

TEST(pulse, extra_roundtrip_survives_sort)
{
  cryptonote::tx_extra_pulse_round extra{};
  extra.height = 12;
  extra.round = 3;
  extra.leader_index = 4;
  extra.payload_hash = sample_payload();
  extra.votes.push_back({crypto::signature{}, 2});
  std::vector<uint8_t> blob;
  ASSERT_TRUE(cryptonote::add_pulse_round_to_tx_extra(blob, extra));
  std::vector<uint8_t> sorted;
  ASSERT_TRUE(cryptonote::sort_tx_extra(blob, sorted));
  cryptonote::tx_extra_pulse_round parsed{};
  ASSERT_TRUE(cryptonote::get_pulse_round_from_tx_extra(sorted, parsed));
  EXPECT_EQ(extra.height, parsed.height);
  EXPECT_EQ(extra.round, parsed.round);
  EXPECT_EQ(extra.leader_index, parsed.leader_index);
  EXPECT_TRUE(extra.payload_hash == parsed.payload_hash);
  ASSERT_EQ(1u, parsed.votes.size());
  EXPECT_EQ(2u, parsed.votes[0].validator_index);
}

TEST(pulse, majority_certificate_splice_is_weight_neutral)
{
  cryptonote::tx_extra_pulse_round extra{};
  extra.height = 12;
  extra.round = 3;
  extra.leader_index = 4;
  extra.payload_hash = sample_payload();
  extra.votes.resize(service_nodes::pulse::min_signatures_for_quorum(service_nodes::pulse::k_quorum_size));
  for (uint32_t i = 0; i < extra.votes.size(); ++i)
    extra.votes[i].validator_index = i;
  std::vector<uint8_t> blob;
  ASSERT_TRUE(cryptonote::add_pulse_round_to_tx_extra(blob, extra));
  const size_t reserve = service_nodes::pulse::majority_certificate_blob_size();
  ASSERT_GT(reserve, 0u);
  EXPECT_GE(reserve, blob.size());

  std::vector<uint8_t> padded(blob.size() + 8, 0);
  const size_t before = 0;
  const size_t padded_size = padded.size();
  ASSERT_TRUE(service_nodes::pulse::splice_majority_certificate(padded, before, extra));
  EXPECT_EQ(padded_size, padded.size());
  cryptonote::tx_extra_pulse_round parsed{};
  ASSERT_TRUE(cryptonote::get_pulse_round_from_tx_extra(padded, parsed));
  EXPECT_EQ(extra.height, parsed.height);
  EXPECT_TRUE(extra.payload_hash == parsed.payload_hash);
  ASSERT_EQ(extra.votes.size(), parsed.votes.size());
  std::vector<uint8_t> too_small(blob.size() / 2, 0);
  EXPECT_FALSE(service_nodes::pulse::splice_majority_certificate(too_small, 0, extra));
  service_nodes::pulse::MinerExtraSummary sum{};
  ASSERT_TRUE(service_nodes::pulse::summarize_miner_pulse_extra(padded, sum));
  EXPECT_TRUE(sum.present);
  EXPECT_EQ(3u, sum.round);
  EXPECT_EQ(4u, sum.leader_index);
  EXPECT_EQ(extra.votes.size(), sum.signature_count);
  EXPECT_TRUE(sum.payload_hash == extra.payload_hash);
  EXPECT_FALSE(service_nodes::pulse::summarize_miner_pulse_extra({}, sum));
  EXPECT_FALSE(sum.present);
}

TEST(pulse, relay_vote_roundtrip_and_collector)
{
  service_nodes::pulse::collector().clear();
  constexpr uint64_t height = 777;
  crypto::hash prev{};
  prev.data[1] = 4;
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  const auto payload = sample_payload();
  const uint32_t lead = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size()));

  const auto first = make_relay_vote(height, prev, 0, lead, 0, payload, keys[q[0]]);

  std::string blob;
  ASSERT_TRUE(service_nodes::pulse::encode_relay_vote(first, blob));
  EXPECT_EQ(service_nodes::pulse::k_relay_vote_bytes, blob.size());
  EXPECT_EQ(146u, blob.size());
  service_nodes::pulse::RelayVote decoded{};
  ASSERT_TRUE(service_nodes::pulse::decode_relay_vote(blob, decoded));
  EXPECT_EQ(first.height, decoded.height);
  EXPECT_EQ(first.validator_index, decoded.validator_index);
  EXPECT_TRUE(first.payload_hash == decoded.payload_hash);

  auto &collector = service_nodes::pulse::collector();
  collector.prepare(height, prev, 0, lead);
  EXPECT_TRUE(collector.add_vote(first, qpubs, pubs.size(), false));
  EXPECT_FALSE(collector.add_vote(first, qpubs, pubs.size(), false));
  EXPECT_EQ(1u, collector.signature_count());
  cryptonote::tx_extra_pulse_round early{};
  EXPECT_FALSE(collector.snapshot(early));
  EXPECT_FALSE(collector.majority_ok());

  for (size_t i = 1; i < 7; ++i)
  {
    const auto v = make_relay_vote(height, prev, 0, lead, static_cast<uint32_t>(i), payload, keys[q[i]]);
    EXPECT_TRUE(collector.add_vote(v, qpubs, pubs.size(), false));
  }
  EXPECT_EQ(7u, collector.signature_count());
  EXPECT_TRUE(collector.majority_ok());
  cryptonote::tx_extra_pulse_round extra{};
  ASSERT_TRUE(collector.snapshot(extra));
  EXPECT_TRUE(extra.payload_hash == payload);
  EXPECT_TRUE(service_nodes::pulse::verify_round(extra, height, prev, pubs.size(), qpubs, 7));
  EXPECT_TRUE(service_nodes::pulse::verify_majority_certificate(extra, height, prev, pubs.size(), qpubs));

  auto unsorted = extra;
  ASSERT_GE(unsorted.votes.size(), 2u);
  std::swap(unsorted.votes.front(), unsorted.votes.back());
  EXPECT_FALSE(service_nodes::pulse::verify_round(unsorted, height, prev, pubs.size(), qpubs, 7));

  collector.clear();
  EXPECT_EQ(0u, collector.signature_count());
}

TEST(pulse, collector_canonicalizes_vote_order)
{
  service_nodes::pulse::collector().clear();
  constexpr uint64_t height = 778;
  crypto::hash prev{};
  prev.data[6] = 2;
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  const auto payload = sample_payload();
  const uint32_t lead = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size()));
  auto &collector = service_nodes::pulse::collector();
  collector.prepare(height, prev, 0, lead);

  const uint32_t order[] = {6, 0, 4, 2, 1, 5, 3};
  for (const uint32_t i : order)
  {
    const auto v = make_relay_vote(height, prev, 0, lead, i, payload, keys[q[i]]);
    EXPECT_TRUE(collector.add_vote(v, qpubs, pubs.size(), false));
  }
  cryptonote::tx_extra_pulse_round extra{};
  ASSERT_TRUE(collector.snapshot(extra));
  ASSERT_EQ(7u, extra.votes.size());
  for (uint32_t i = 0; i < extra.votes.size(); ++i)
    EXPECT_EQ(i, extra.votes[i].validator_index);
  EXPECT_TRUE(extra.payload_hash == payload);
  EXPECT_TRUE(service_nodes::pulse::verify_majority_certificate(extra, height, prev, pubs.size(), qpubs));

  for (const uint32_t i : {7u, 8u})
  {
    const auto v = make_relay_vote(height, prev, 0, lead, i, payload, keys[q[i]]);
    EXPECT_TRUE(collector.add_vote(v, qpubs, pubs.size(), false));
  }
  EXPECT_EQ(9u, collector.signature_count());
  cryptonote::tx_extra_pulse_round trimmed{};
  ASSERT_TRUE(collector.snapshot(trimmed));
  ASSERT_EQ(7u, trimmed.votes.size());
  for (uint32_t i = 0; i < trimmed.votes.size(); ++i)
    EXPECT_EQ(i, trimmed.votes[i].validator_index);
  EXPECT_TRUE(service_nodes::pulse::verify_majority_certificate(trimmed, height, prev, pubs.size(), qpubs));
  collector.clear();
}

TEST(pulse, idle_majority_does_not_snapshot_and_cannot_wipe_payload)
{
  service_nodes::pulse::collector().clear();
  constexpr uint64_t height = 779;
  crypto::hash prev{};
  prev.data[7] = 3;
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  const uint32_t lead = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size()));
  auto &collector = service_nodes::pulse::collector();
  collector.prepare(height, prev, 0, lead);

  for (uint32_t i = 0; i < 7; ++i)
  {
    const auto v = make_relay_vote(height, prev, 0, lead, i, crypto::null_hash, keys[q[i]]);
    EXPECT_TRUE(collector.add_vote(v, qpubs, pubs.size(), false));
  }
  EXPECT_TRUE(collector.majority_ok());
  cryptonote::tx_extra_pulse_round idle{};
  EXPECT_FALSE(collector.snapshot(idle));

  const auto payload = sample_payload();
  const auto first_payload = make_relay_vote(height, prev, 0, lead, 0, payload, keys[q[0]]);
  EXPECT_TRUE(collector.add_vote(first_payload, qpubs, pubs.size(), false));
  EXPECT_TRUE(collector.payload_hash() == payload);
  EXPECT_EQ(1u, collector.signature_count());

  const auto idle_again = make_relay_vote(height, prev, 0, lead, 1, crypto::null_hash, keys[q[1]]);
  EXPECT_FALSE(collector.add_vote(idle_again, qpubs, pubs.size(), false));

  crypto::hash other = payload;
  other.data[2] = 0x22;
  const auto rival = make_relay_vote(height, prev, 0, lead, 1, other, keys[q[1]]);
  EXPECT_FALSE(collector.add_vote(rival, qpubs, pubs.size(), false));

  for (uint32_t i = 1; i < 7; ++i)
  {
    const auto v = make_relay_vote(height, prev, 0, lead, i, payload, keys[q[i]]);
    EXPECT_TRUE(collector.add_vote(v, qpubs, pubs.size(), false));
  }
  cryptonote::tx_extra_pulse_round extra{};
  ASSERT_TRUE(collector.snapshot(extra));
  EXPECT_TRUE(extra.payload_hash == payload);
  EXPECT_TRUE(service_nodes::pulse::verify_majority_certificate(extra, height, prev, pubs.size(), qpubs));
  collector.clear();
}

TEST(pulse, fallback_round_rotates_leader_and_rejects_stale_votes)
{
  service_nodes::pulse::collector().clear();
  constexpr uint64_t height = 888;
  constexpr uint8_t round = 3;
  crypto::hash prev{};
  prev.data[2] = 5;
  const auto keys = make_pulse_keys(17);
  const auto pubs = pubs_of(keys);
  const auto q0 = service_nodes::pulse::quorum_indices(height, pubs.size(), 0);
  const auto q3 = service_nodes::pulse::quorum_indices(height, pubs.size(), round);
  ASSERT_NE(q0.front(), q3.front());
  const auto qpubs0 = service_nodes::pulse::quorum_pubkeys(height, pubs, 0);
  const auto qpubs3 = service_nodes::pulse::quorum_pubkeys(height, pubs, round);
  const uint32_t lead0 = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size(), 0));
  const uint32_t lead3 = static_cast<uint32_t>(service_nodes::pulse::leader_index(height, pubs.size(), round));
  EXPECT_EQ(lead3, (lead0 + round) % pubs.size());

  cryptonote::tx_extra_pulse_round extra{};
  ASSERT_TRUE(service_nodes::pulse::try_local_round(height, prev, round, keys[q3.front()].pub,
                                                    keys[q3.front()].sec, pubs, extra));
  EXPECT_EQ(round, extra.round);
  EXPECT_EQ(lead3, extra.leader_index);
  EXPECT_TRUE(service_nodes::pulse::verify_round(extra, height, prev, pubs.size(), qpubs3, 1));
  extra.round = 0;
  extra.leader_index = lead0;
  EXPECT_FALSE(service_nodes::pulse::verify_round(extra, height, prev, pubs.size(), qpubs0, 1));

  auto &collector = service_nodes::pulse::collector();
  collector.prepare(height, prev, round, lead3);
  service_nodes::pulse::RelayVote stale{};
  stale.height = height;
  stale.round = 0;
  stale.leader_index = lead0;
  stale.validator_index = 0;
  stale.prev_id = prev;
  stale.signature = service_nodes::pulse::sign_round(service_nodes::pulse::round_hash(height, prev, 0),
                                                     keys[q0.front()].pub, keys[q0.front()].sec);
  EXPECT_FALSE(collector.add_vote(stale, qpubs0, pubs.size(), false));

  service_nodes::pulse::RelayVote current{};
  current.height = height;
  current.round = round;
  current.leader_index = lead3;
  current.validator_index = 0;
  current.prev_id = prev;
  current.signature = service_nodes::pulse::sign_round(service_nodes::pulse::round_hash(height, prev, round),
                                                       keys[q3.front()].pub, keys[q3.front()].sec);
  EXPECT_TRUE(collector.add_vote(current, qpubs3, pubs.size(), false));
  EXPECT_EQ(round, collector.round());
  EXPECT_EQ(1u, collector.signature_count());
  collector.clear();
}

TEST(pulse, participate_round_signs_and_is_idempotent)
{
  service_nodes::pulse::collector().clear();
  constexpr uint64_t height = 901;
  crypto::hash prev{};
  prev.data[4] = 8;
  const auto keys = make_pulse_keys(17);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  ASSERT_EQ(11u, q.size());

  size_t outsider = service_nodes::pulse::k_not_in_quorum;
  for (size_t i = 0; i < pubs.size(); ++i)
  {
    if (service_nodes::pulse::validator_slot(height, pubs, pubs[i]) == service_nodes::pulse::k_not_in_quorum)
    {
      outsider = i;
      break;
    }
  }
  ASSERT_NE(service_nodes::pulse::k_not_in_quorum, outsider);

  EXPECT_FALSE(service_nodes::pulse::participate_round(height, prev, 0, keys[outsider].pub,
                                                       keys[outsider].sec, pubs, false));
  EXPECT_EQ(0u, service_nodes::pulse::collector().signature_count());

  const size_t lead = q.front();
  EXPECT_TRUE(service_nodes::pulse::participate_round(height, prev, 0, keys[lead].pub, keys[lead].sec,
                                                      pubs, false));
  EXPECT_EQ(1u, service_nodes::pulse::collector().signature_count());
  EXPECT_FALSE(service_nodes::pulse::participate_round(height, prev, 0, keys[lead].pub, keys[lead].sec,
                                                       pubs, false));
  EXPECT_EQ(1u, service_nodes::pulse::collector().signature_count());

  cryptonote::tx_extra_pulse_round extra{};
  EXPECT_FALSE(service_nodes::pulse::collector().snapshot(extra));
  EXPECT_FALSE(service_nodes::pulse::collector().majority_ok());
  cryptonote::tx_extra_pulse_round local{};
  ASSERT_TRUE(service_nodes::pulse::try_local_round(height, prev, 0, keys[lead].pub, keys[lead].sec, pubs, local));
  const auto qpubs = service_nodes::pulse::quorum_pubkeys(height, pubs);
  EXPECT_TRUE(service_nodes::pulse::verify_round(local, height, prev, pubs.size(), qpubs, 1));
  EXPECT_FALSE(service_nodes::pulse::verify_round(local, height, prev, pubs.size(), qpubs,
                                                  service_nodes::pulse::min_signatures_for_quorum(qpubs.size())));
  service_nodes::pulse::collector().clear();
}

TEST(pulse, participate_round_retransmits_duplicate_vote)
{
  service_nodes::pulse::collector().clear();
  service_nodes::pulse::set_relay_new_vote(nullptr);
  constexpr uint64_t height = 902;
  crypto::hash prev{};
  prev.data[5] = 1;
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  const size_t lead = q.front();

  g_pulse_relay_count = 0;
  service_nodes::pulse::set_relay_new_vote(&count_pulse_relay);
  EXPECT_TRUE(service_nodes::pulse::participate_round(height, prev, 0, keys[lead].pub, keys[lead].sec, pubs, true));
  EXPECT_EQ(1, g_pulse_relay_count);
  EXPECT_FALSE(service_nodes::pulse::participate_round(height, prev, 0, keys[lead].pub, keys[lead].sec, pubs, true));
  EXPECT_EQ(2, g_pulse_relay_count);
  EXPECT_EQ(1u, service_nodes::pulse::collector().signature_count());
  service_nodes::pulse::set_relay_new_vote(nullptr);
  service_nodes::pulse::collector().clear();
}

TEST(pulse, collector_discards_votes_below_next_height)
{
  service_nodes::pulse::collector().clear();
  constexpr uint64_t height = 903;
  crypto::hash prev{};
  const auto keys = make_pulse_keys(11);
  const auto pubs = pubs_of(keys);
  const auto q = service_nodes::pulse::quorum_indices(height, pubs.size());
  ASSERT_TRUE(service_nodes::pulse::participate_round(height, prev, 0, keys[q.front()].pub,
                                                      keys[q.front()].sec, pubs, false));
  EXPECT_EQ(1u, service_nodes::pulse::collector().signature_count());
  service_nodes::pulse::collector().discard_below(height);
  EXPECT_EQ(1u, service_nodes::pulse::collector().signature_count());
  service_nodes::pulse::collector().discard_below(height + 1);
  EXPECT_EQ(0u, service_nodes::pulse::collector().signature_count());
  service_nodes::pulse::collector().clear();
}


