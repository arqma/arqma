// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "pulse.h"

#include "common/util.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>

namespace service_nodes
{
namespace pulse
{
namespace
{
// 0 = leader/quorum helpers only
// 1 = local sign/verify + miner extra
// 2 = network quorum collection + wait-windows (current)
// 3 = reserved: would drop RandomARQ at HF21 — not scheduled; keep hybrid PoW
constexpr int k_pulse_pow_stage = 2;
constexpr int k_pulse_pow_replacement_stage = 3;

void (*g_relay_new_vote)(const RelayVote&) = nullptr;

void insert_sorted_vote(std::vector<cryptonote::tx_extra_pulse_round::vote>& votes,
                        const cryptonote::tx_extra_pulse_round::vote& vote)
{
  const auto it = std::lower_bound(votes.begin(), votes.end(), vote.validator_index,
                                   [](const cryptonote::tx_extra_pulse_round::vote& existing, const uint32_t index) {
                                     return existing.validator_index < index;
                                   });
  votes.insert(it, vote);
}
} // namespace

const char* to_string(const SnMode mode) noexcept
{
  switch (mode)
  {
  case SnMode::Legacy:
    return "legacy";
  case SnMode::Hybrid:
    return "hybrid";
  case SnMode::Exclusive:
    return "exclusive";
  }
  return "legacy";
}

SnMode sn_operating_mode(const uint8_t hard_fork_version) noexcept
{
  if (hard_fork_version >= k_hf_exclusive)
    return SnMode::Exclusive;
  if (hard_fork_version >= k_hf_hybrid)
    return SnMode::Hybrid;
  return SnMode::Legacy;
}

bool hybrid_sn_permitted(const uint8_t hard_fork_version) noexcept
{
  return hard_fork_version >= k_hf_hybrid;
}

bool exclusive_sn_required(const uint8_t hard_fork_version) noexcept
{
  return hard_fork_version >= k_hf_exclusive;
}

size_t leader_index(const uint64_t height, const size_t sn_count, const uint8_t round) noexcept
{
  if (sn_count == 0)
    return 0;
  crypto::hash hashed{};
  crypto::cn_fast_hash(&height, sizeof(height), hashed);
  uint64_t seed = 0;
  std::memcpy(&seed, hashed.data, sizeof(seed));
  return static_cast<size_t>((seed % sn_count + static_cast<size_t>(round) % sn_count) % sn_count);
}

uint8_t round_from_timestamps(const uint64_t parent_timestamp, const uint64_t now) noexcept
{
  if (parent_timestamp == 0 || now <= parent_timestamp)
    return 0;
  const uint64_t elapsed = now - parent_timestamp;
  const uint64_t r = elapsed / k_round_window_seconds;
  return r > k_max_round ? k_max_round : static_cast<uint8_t>(r);
}

bool extra_round_matches_timestamps(const uint64_t parent_timestamp, const uint64_t block_timestamp,
                                    const uint8_t extra_round) noexcept
{
  return extra_round == round_from_timestamps(parent_timestamp, block_timestamp);
}

bool majority_reached(const size_t signatures, const size_t quorum_size) noexcept
{
  return signatures >= min_signatures_for_quorum(quorum_size);
}

std::vector<size_t> quorum_indices(const uint64_t height, const size_t sn_count, const uint8_t round,
                                   const size_t quorum_size)
{
  std::vector<size_t> out;
  if (sn_count == 0 || quorum_size == 0)
    return out;
  const size_t take = sn_count < quorum_size ? sn_count : quorum_size;
  out.reserve(take);
  const size_t lead = leader_index(height, sn_count, round);
  for (size_t i = 0; i < take; ++i)
    out.push_back((lead + i) % sn_count);
  return out;
}

size_t min_signatures_for_quorum(const size_t quorum_size) noexcept
{
  if (quorum_size == 0)
    return 0;
  const size_t two_thirds = (quorum_size * 2) / 3;
  return two_thirds < 1 ? 1 : two_thirds;
}

size_t validator_slot(const uint64_t height, const std::vector<crypto::public_key>& active_pubs,
                      const crypto::public_key& pub, const uint8_t round)
{
  const auto q = quorum_indices(height, active_pubs.size(), round);
  for (size_t i = 0; i < q.size(); ++i)
  {
    if (q[i] < active_pubs.size() && active_pubs[q[i]] == pub)
      return i;
  }
  return k_not_in_quorum;
}

std::vector<crypto::public_key> quorum_pubkeys(const uint64_t height,
                                               const std::vector<crypto::public_key>& active_pubs,
                                               const uint8_t round)
{
  std::vector<crypto::public_key> out;
  const auto q = quorum_indices(height, active_pubs.size(), round);
  out.reserve(q.size());
  for (const size_t idx : q)
  {
    if (idx < active_pubs.size())
      out.push_back(active_pubs[idx]);
  }
  return out;
}

crypto::hash round_hash(const uint64_t height, const crypto::hash& prev_id, const uint8_t round,
                        const crypto::hash& payload_hash)
{
  const auto prefix = tools::memcpy_le(height, round);
  std::array<char, sizeof(prefix) + 2 * crypto::HASH_SIZE> buf{};
  std::memcpy(buf.data(), prefix.data(), prefix.size());
  std::memcpy(buf.data() + prefix.size(), prev_id.data, crypto::HASH_SIZE);
  std::memcpy(buf.data() + prefix.size() + crypto::HASH_SIZE, payload_hash.data, crypto::HASH_SIZE);
  crypto::hash out{};
  crypto::cn_fast_hash(buf.data(), buf.size(), out);
  return out;
}

crypto::hash miner_payload_hash(const uint64_t timestamp, const std::vector<crypto::hash>& tx_hashes,
                                const crypto::public_key& miner_out_key)
{
  const auto hdr = tools::memcpy_le(timestamp, static_cast<uint64_t>(tx_hashes.size()));
  std::string buf;
  buf.append(hdr.data(), hdr.size());
  for (const auto& hashed : tx_hashes)
    buf.append(reinterpret_cast<const char*>(hashed.data), crypto::HASH_SIZE);
  buf.append(reinterpret_cast<const char*>(&miner_out_key), sizeof(miner_out_key));
  crypto::hash out{};
  crypto::cn_fast_hash(buf.data(), buf.size(), out);
  return out;
}

crypto::signature sign_round(const crypto::hash& hashed, const crypto::public_key& pub,
                             const crypto::secret_key& sec)
{
  crypto::signature sig{};
  crypto::generate_signature(hashed, pub, sec, sig);
  return sig;
}

bool check_round_signature(const crypto::hash& hashed, const crypto::public_key& pub,
                           const crypto::signature& sig)
{
  return crypto::check_signature(hashed, pub, sig);
}

bool try_local_round(const uint64_t height, const crypto::hash& prev_id, const uint8_t round,
                     const crypto::public_key& pub, const crypto::secret_key& sec,
                     const std::vector<crypto::public_key>& active_pubs,
                     cryptonote::tx_extra_pulse_round& out, const crypto::hash& payload_hash)
{
  const size_t slot = validator_slot(height, active_pubs, pub, round);
  if (slot == k_not_in_quorum)
    return false;

  out = {};
  out.height = height;
  out.round = round;
  out.leader_index = static_cast<uint32_t>(leader_index(height, active_pubs.size(), round));
  out.payload_hash = payload_hash;
  const crypto::hash hashed = round_hash(height, prev_id, round, payload_hash);
  out.votes.push_back({sign_round(hashed, pub, sec), static_cast<uint32_t>(slot)});
  return true;
}

bool participate_round(const uint64_t height, const crypto::hash& prev_id, const uint8_t round,
                       const crypto::public_key& pub, const crypto::secret_key& sec,
                       const std::vector<crypto::public_key>& active_pubs, const bool relay,
                       const crypto::hash& payload_hash)
{
  if (active_pubs.empty())
    return false;
  const uint32_t lead = static_cast<uint32_t>(leader_index(height, active_pubs.size(), round));
  collector().prepare(height, prev_id, round, lead);
  cryptonote::tx_extra_pulse_round local{};
  if (!try_local_round(height, prev_id, round, pub, sec, active_pubs, local, payload_hash) || local.votes.empty())
    return false;
  RelayVote vote{};
  vote.height = local.height;
  vote.round = local.round;
  vote.leader_index = local.leader_index;
  vote.validator_index = local.votes.front().validator_index;
  vote.prev_id = prev_id;
  vote.payload_hash = payload_hash;
  vote.signature = local.votes.front().signature;
  const bool added = collector().add_vote(vote, quorum_pubkeys(height, active_pubs, round), active_pubs.size(),
                                          false);
  if (!added && !collector().has_vote(vote.validator_index))
    return false;
  if (relay && g_relay_new_vote && collector().payload_hash() == payload_hash)
    g_relay_new_vote(vote);
  return added;
}

bool verify_round(const cryptonote::tx_extra_pulse_round& extra, const uint64_t height,
                  const crypto::hash& prev_id, const size_t active_sn_count,
                  const std::vector<crypto::public_key>& quorum_keys, const size_t min_signatures)
{
  if (extra.height != height)
    return false;
  if (extra.round > k_max_round)
    return false;
  if (active_sn_count == 0)
    return false;
  if (extra.leader_index != leader_index(height, active_sn_count, extra.round))
    return false;
  if (quorum_keys.empty())
    return false;
  if (extra.votes.size() < min_signatures)
    return false;

  const crypto::hash hashed = round_hash(height, prev_id, extra.round, extra.payload_hash);
  uint32_t prev_index = 0;
  bool first = true;
  for (const auto& vote : extra.votes)
  {
    if (vote.validator_index >= quorum_keys.size())
      return false;
    if (!first && vote.validator_index <= prev_index)
      return false;
    first = false;
    prev_index = vote.validator_index;
    if (!check_round_signature(hashed, quorum_keys[vote.validator_index], vote.signature))
      return false;
  }
  return true;
}

bool verify_majority_certificate(const cryptonote::tx_extra_pulse_round& extra, const uint64_t height,
                                 const crypto::hash& prev_id, const size_t active_sn_count,
                                 const std::vector<crypto::public_key>& quorum_keys)
{
  const size_t min_sigs = min_signatures_for_quorum(quorum_keys.size());
  if (min_sigs == 0 || extra.votes.size() != min_sigs)
    return false;
  if (extra.payload_hash == crypto::null_hash)
    return false;
  return verify_round(extra, height, prev_id, active_sn_count, quorum_keys, min_sigs);
}

size_t majority_certificate_blob_size()
{
  cryptonote::tx_extra_pulse_round extra{};
  extra.height = std::numeric_limits<uint64_t>::max();
  extra.round = k_max_round;
  extra.leader_index = std::numeric_limits<uint32_t>::max();
  extra.payload_hash = crypto::null_hash;
  extra.votes.resize(min_signatures_for_quorum(k_quorum_size));
  for (size_t i = 0; i < extra.votes.size(); ++i)
    extra.votes[i].validator_index = static_cast<uint32_t>(i);
  std::vector<uint8_t> blob;
  if (!cryptonote::add_pulse_round_to_tx_extra(blob, extra))
    return 0;
  return blob.size();
}

bool splice_majority_certificate(std::vector<uint8_t>& extra, const size_t extra_before_padding,
                                 const cryptonote::tx_extra_pulse_round& cert)
{
  std::vector<uint8_t> blob;
  if (!cryptonote::add_pulse_round_to_tx_extra(blob, cert))
    return false;
  if (blob.empty() || extra.size() < extra_before_padding)
    return false;
  const size_t padding = extra.size() - extra_before_padding;
  if (padding < blob.size())
    return false;
  const size_t leftover = padding - blob.size();
  extra.resize(extra_before_padding);
  extra.insert(extra.end(), blob.begin(), blob.end());
  extra.insert(extra.end(), leftover, 0);
  return true;
}

bool summarize_miner_pulse_extra(const std::vector<uint8_t>& extra, MinerExtraSummary& out)
{
  out = {};
  cryptonote::tx_extra_pulse_round pulse{};
  if (!cryptonote::get_pulse_round_from_tx_extra(extra, pulse))
    return false;
  out.present = true;
  out.round = pulse.round;
  out.leader_index = pulse.leader_index;
  out.signature_count = pulse.votes.size();
  out.payload_hash = pulse.payload_hash;
  return true;
}

bool encode_relay_vote(const RelayVote& vote, std::string& out)
{
  const auto hdr = tools::memcpy_le(vote.height, vote.round, vote.leader_index, vote.validator_index);
  out.assign(k_relay_vote_bytes, '\0');
  out[0] = static_cast<char>(k_relay_vote_version);
  std::memcpy(out.data() + 1, hdr.data(), hdr.size());
  std::memcpy(out.data() + 1 + hdr.size(), vote.prev_id.data, crypto::HASH_SIZE);
  std::memcpy(out.data() + 1 + hdr.size() + crypto::HASH_SIZE, vote.payload_hash.data, crypto::HASH_SIZE);
  std::memcpy(out.data() + 1 + hdr.size() + 2 * crypto::HASH_SIZE, &vote.signature, sizeof(vote.signature));
  return true;
}

bool decode_relay_vote(const std::string_view blob, RelayVote& out)
{
  if (blob.size() != k_relay_vote_bytes)
    return false;
  if (static_cast<uint8_t>(blob[0]) != k_relay_vote_version)
    return false;
  RelayVote vote{};
  size_t off = 1;
  std::memcpy(&vote.height, blob.data() + off, sizeof(vote.height));
  off += sizeof(vote.height);
  std::memcpy(&vote.round, blob.data() + off, sizeof(vote.round));
  off += sizeof(vote.round);
  std::memcpy(&vote.leader_index, blob.data() + off, sizeof(vote.leader_index));
  off += sizeof(vote.leader_index);
  std::memcpy(&vote.validator_index, blob.data() + off, sizeof(vote.validator_index));
  off += sizeof(vote.validator_index);
  std::memcpy(vote.prev_id.data, blob.data() + off, crypto::HASH_SIZE);
  off += crypto::HASH_SIZE;
  std::memcpy(vote.payload_hash.data, blob.data() + off, crypto::HASH_SIZE);
  off += crypto::HASH_SIZE;
  std::memcpy(&vote.signature, blob.data() + off, sizeof(vote.signature));
  if constexpr (boost::endian::order::native != boost::endian::order::little)
  {
    boost::endian::little_to_native_inplace(vote.height);
    boost::endian::little_to_native_inplace(vote.leader_index);
    boost::endian::little_to_native_inplace(vote.validator_index);
  }
  out = vote;
  return true;
}

void set_relay_new_vote(void (*const fn)(const RelayVote&))
{
  g_relay_new_vote = fn;
}

RoundCollector& collector()
{
  static RoundCollector instance;
  return instance;
}

void RoundCollector::prepare(const uint64_t height, const crypto::hash& prev_id, const uint8_t round,
                             const uint32_t leader_index)
{
  std::lock_guard lock{m_mu};
  if (m_extra.height == height && m_extra.round == round && m_prev_id == prev_id &&
      m_extra.leader_index == leader_index)
    return;
  m_extra = {};
  m_extra.height = height;
  m_extra.round = round;
  m_extra.leader_index = leader_index;
  m_prev_id = prev_id;
  m_payload_hash = {};
  m_quorum_size = 0;
}

bool RoundCollector::add_vote(const RelayVote& vote, const std::vector<crypto::public_key>& quorum_keys,
                              const size_t active_sn_count, const bool relay_if_new)
{
  if (quorum_keys.empty() || vote.validator_index >= quorum_keys.size())
    return false;

  {
    std::lock_guard lock{m_mu};
    const bool same_block = m_extra.height == vote.height && m_prev_id == vote.prev_id;
    if (same_block && vote.round < m_extra.round)
      return false;
    const bool same_round = same_block && m_extra.round == vote.round;
    if (!same_round || m_payload_hash != vote.payload_hash)
    {
      // Idle (null) votes must not wipe a payload certificate. A different
      // non-null payload is frozen out for this round so competing miner
      // templates cannot reset a majority already in flight.
      if (same_round && vote.payload_hash == crypto::null_hash && m_payload_hash != crypto::null_hash)
        return false;
      if (same_round && m_payload_hash != crypto::null_hash && vote.payload_hash != m_payload_hash)
        return false;
      m_extra = {};
      m_extra.height = vote.height;
      m_extra.round = vote.round;
      m_extra.leader_index = vote.leader_index;
      m_extra.payload_hash = vote.payload_hash;
      m_prev_id = vote.prev_id;
      m_payload_hash = vote.payload_hash;
    }
    m_quorum_size = quorum_keys.size();
    m_extra.payload_hash = vote.payload_hash;
    if (m_extra.leader_index != vote.leader_index)
      return false;
    for (const auto& existing : m_extra.votes)
    {
      if (existing.validator_index == vote.validator_index)
        return false;
    }

    cryptonote::tx_extra_pulse_round probe = m_extra;
    insert_sorted_vote(probe.votes, {vote.signature, vote.validator_index});
    if (!verify_round(probe, vote.height, vote.prev_id, active_sn_count, quorum_keys, probe.votes.size()))
      return false;
    m_extra.votes = std::move(probe.votes);
  }

  if (relay_if_new && g_relay_new_vote)
    g_relay_new_vote(vote);
  return true;
}

bool RoundCollector::snapshot(cryptonote::tx_extra_pulse_round& out) const
{
  std::lock_guard lock{m_mu};
  const size_t q = m_quorum_size != 0 ? m_quorum_size : k_quorum_size;
  if (m_extra.payload_hash == crypto::null_hash || !majority_reached(m_extra.votes.size(), q))
    return false;
  out = m_extra;
  const size_t keep = min_signatures_for_quorum(q);
  if (out.votes.size() > keep)
    out.votes.resize(keep);
  return true;
}

bool RoundCollector::has_vote(const uint32_t validator_index) const
{
  std::lock_guard lock{m_mu};
  for (const auto& existing : m_extra.votes)
  {
    if (existing.validator_index == validator_index)
      return true;
  }
  return false;
}

crypto::hash RoundCollector::payload_hash() const
{
  std::lock_guard lock{m_mu};
  return m_payload_hash;
}

uint64_t RoundCollector::height() const
{
  std::lock_guard lock{m_mu};
  return m_extra.height;
}

uint8_t RoundCollector::round() const
{
  std::lock_guard lock{m_mu};
  return m_extra.round;
}

size_t RoundCollector::signature_count() const
{
  std::lock_guard lock{m_mu};
  return m_extra.votes.size();
}

size_t RoundCollector::quorum_size() const
{
  std::lock_guard lock{m_mu};
  return m_quorum_size;
}

bool RoundCollector::majority_ok() const
{
  std::lock_guard lock{m_mu};
  const size_t q = m_quorum_size != 0 ? m_quorum_size : k_quorum_size;
  return majority_reached(m_extra.votes.size(), q);
}

void RoundCollector::discard_below(const uint64_t height)
{
  std::lock_guard lock{m_mu};
  if (m_extra.height != 0 && m_extra.height < height)
  {
    m_extra = {};
    m_prev_id = {};
    m_payload_hash = {};
    m_quorum_size = 0;
  }
}

void RoundCollector::clear()
{
  std::lock_guard lock{m_mu};
  m_extra = {};
  m_prev_id = {};
  m_payload_hash = {};
  m_quorum_size = 0;
}

bool pow_replacement_ready() noexcept
{
  // Hybrid policy: Pulse majority does not replace RandomARQ, even if
  // k_pulse_pow_stage is bumped. Revisit only with an explicit product decision.
  return false;
}

bool pow_required_for_block(const uint8_t /*hard_fork_version*/) noexcept
{
  return true;
}

const char* blocker() noexcept
{
  if (k_pulse_pow_stage < 1)
    return "pulse-block-producer-unwired";
  if (k_pulse_pow_stage < 2)
    return "pulse-quorum-relay-unwired";
  if (k_pulse_pow_stage < k_pulse_pow_replacement_stage)
    return "pulse-pow-replacement-not-ready";
  return "none";
}
} // namespace pulse
} // namespace service_nodes
