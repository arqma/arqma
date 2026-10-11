// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "blink.h"

#include "common/util.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

#include <boost/endian/conversion.hpp>

namespace arq_blink {
namespace {
std::atomic<bool> g_wire_connected{false};
void (*g_relay_new_vote)(const RelayVote&) = nullptr;
} // namespace

std::size_t min_signatures_for_quorum(const std::size_t quorum_size) noexcept
{
  if (quorum_size == 0)
    return 0;
  // ceil(2n/3) → 7 of 10.
  return (quorum_size * 2 + 2) / 3;
}

std::size_t leader_index(const std::uint64_t height, const std::size_t sn_count) noexcept
{
  if (sn_count == 0)
    return 0;
  crypto::hash hashed{};
  crypto::cn_fast_hash(&height, sizeof(height), hashed);
  std::uint64_t seed = 0;
  std::memcpy(&seed, hashed.data, sizeof(seed));
  return static_cast<std::size_t>(seed % sn_count);
}

std::vector<std::size_t> quorum_indices(const std::uint64_t height, const std::size_t sn_count,
                                        const std::size_t quorum_size)
{
  std::vector<std::size_t> out;
  if (sn_count == 0 || quorum_size == 0)
    return out;
  const std::size_t take = sn_count < quorum_size ? sn_count : quorum_size;
  out.reserve(take);
  const std::size_t lead = leader_index(height, sn_count);
  for (std::size_t i = 0; i < take; ++i)
    out.push_back((lead + i) % sn_count);
  return out;
}

crypto::hash tx_round_hash(const std::uint64_t height, const crypto::hash& txid)
{
  std::array<char, sizeof(height) + sizeof(txid)> buf{};
  std::memcpy(buf.data(), &height, sizeof(height));
  std::memcpy(buf.data() + sizeof(height), &txid, sizeof(txid));
  return crypto::cn_fast_hash(buf.data(), buf.size());
}

crypto::signature sign_tx(const crypto::hash& hashed, const crypto::public_key& pub, const crypto::secret_key& sec)
{
  crypto::signature sig{};
  crypto::generate_signature(hashed, pub, sec, sig);
  return sig;
}

bool check_tx_signature(const crypto::hash& hashed, const crypto::public_key& pub, const crypto::signature& sig)
{
  return crypto::check_signature(hashed, pub, sig);
}

bool encode_relay_vote(const RelayVote& vote, std::string& out)
{
  out.assign(k_relay_vote_bytes, '\0');
  out[0] = static_cast<char>(k_relay_vote_version);
  std::size_t off = 1;
  const auto height_le = tools::memcpy_le(vote.height);
  std::memcpy(out.data() + off, height_le.data(), height_le.size());
  off += height_le.size();
  std::memcpy(out.data() + off, vote.txid.data, crypto::HASH_SIZE);
  off += crypto::HASH_SIZE;
  const auto index_le = tools::memcpy_le(vote.validator_index);
  std::memcpy(out.data() + off, index_le.data(), index_le.size());
  off += index_le.size();
  std::memcpy(out.data() + off, vote.pub.data, sizeof(vote.pub));
  off += sizeof(vote.pub);
  std::memcpy(out.data() + off, &vote.signature, sizeof(vote.signature));
  return true;
}

bool decode_relay_vote(const std::string_view blob, RelayVote& out)
{
  if (blob.size() != k_relay_vote_bytes)
    return false;
  if (static_cast<std::uint8_t>(blob[0]) != k_relay_vote_version)
    return false;
  RelayVote vote{};
  std::size_t off = 1;
  std::memcpy(&vote.height, blob.data() + off, sizeof(vote.height));
  off += sizeof(vote.height);
  std::memcpy(vote.txid.data, blob.data() + off, crypto::HASH_SIZE);
  off += crypto::HASH_SIZE;
  std::memcpy(&vote.validator_index, blob.data() + off, sizeof(vote.validator_index));
  off += sizeof(vote.validator_index);
  std::memcpy(vote.pub.data, blob.data() + off, sizeof(vote.pub));
  off += sizeof(vote.pub);
  std::memcpy(&vote.signature, blob.data() + off, sizeof(vote.signature));
  if constexpr (boost::endian::order::native != boost::endian::order::little) {
    boost::endian::little_to_native_inplace(vote.height);
    boost::endian::little_to_native_inplace(vote.validator_index);
  }
  out = vote;
  return true;
}

void set_relay_new_vote(void (*const fn)(const RelayVote&))
{
  g_relay_new_vote = fn;
}

void set_wire_connected(const bool connected) noexcept
{
  g_wire_connected.store(connected, std::memory_order_relaxed);
}

bool wire_connected() noexcept
{
  return g_wire_connected.load(std::memory_order_relaxed);
}

const char* blocker() noexcept
{
  if (wire_connected())
    return "none";
  return "blink-wire-not-connected";
}

bool apply_relay_vote(const RelayVote& vote, const bool relay_if_new)
{
  auto& col = collector();
  if (col.height() != vote.height || col.txid() != vote.txid)
    col.reset(vote.height, vote.txid);
  const std::size_t before = col.signature_count();
  Vote local{};
  local.validator_index = vote.validator_index;
  local.pub = vote.pub;
  local.signature = vote.signature;
  if (!col.add_vote(local, tx_round_hash(vote.height, vote.txid)))
    return false;
  const bool is_new = col.signature_count() > before;
  if (is_new && relay_if_new && g_relay_new_vote)
    g_relay_new_vote(vote);
  return true;
}

void Collector::reset(const std::uint64_t height, const crypto::hash& txid)
{
  std::lock_guard<std::mutex> lock{mu_};
  height_ = height;
  txid_ = txid;
  votes_.clear();
}

bool Collector::add_vote(const Vote& vote, const crypto::hash& hashed)
{
  if (!check_tx_signature(hashed, vote.pub, vote.signature))
    return false;
  std::lock_guard<std::mutex> lock{mu_};
  for (const auto& existing : votes_) {
    if (existing.validator_index == vote.validator_index)
      return std::memcmp(&existing.signature, &vote.signature, sizeof(crypto::signature)) == 0;
  }
  votes_.push_back(vote);
  std::sort(votes_.begin(), votes_.end(),
            [](const Vote& a, const Vote& b) { return a.validator_index < b.validator_index; });
  return true;
}

std::uint64_t Collector::height() const
{
  std::lock_guard<std::mutex> lock{mu_};
  return height_;
}

crypto::hash Collector::txid() const
{
  std::lock_guard<std::mutex> lock{mu_};
  return txid_;
}

std::size_t Collector::signature_count() const
{
  std::lock_guard<std::mutex> lock{mu_};
  return votes_.size();
}

bool Collector::majority_ok(const std::size_t quorum_size) const
{
  std::lock_guard<std::mutex> lock{mu_};
  return votes_.size() >= min_signatures_for_quorum(quorum_size);
}

Collector& collector()
{
  static Collector instance;
  return instance;
}
} // namespace arq_blink
