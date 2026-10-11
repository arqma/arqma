// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"

namespace arq_blink {
/// Local collector for Blink-style quorum signatures (7 of 10), plus Arq-Net
/// `blink_tx` packed-vote gossip. Blink does not replace Pulse or RandomARQ.
inline constexpr std::size_t k_quorum_size = 10;

std::size_t min_signatures_for_quorum(std::size_t quorum_size) noexcept;
std::size_t leader_index(std::uint64_t height, std::size_t sn_count) noexcept;
std::vector<std::size_t> quorum_indices(std::uint64_t height, std::size_t sn_count,
                                        std::size_t quorum_size = k_quorum_size);

crypto::hash tx_round_hash(std::uint64_t height, const crypto::hash& txid);
crypto::signature sign_tx(const crypto::hash& hashed, const crypto::public_key& pub, const crypto::secret_key& sec);
bool check_tx_signature(const crypto::hash& hashed, const crypto::public_key& pub, const crypto::signature& sig);

/// Packed vote used on Arq-Net `blink_tx` (v1: version || height || txid ||
/// validator_index || pub || signature).
struct RelayVote
{
  std::uint64_t height = 0;
  crypto::hash txid{};
  std::uint32_t validator_index = 0;
  crypto::public_key pub{};
  crypto::signature signature{};
};

inline constexpr std::uint8_t k_relay_vote_version = 1;
inline constexpr std::size_t k_relay_vote_bytes = 1 + sizeof(std::uint64_t) + crypto::HASH_SIZE +
                                                  sizeof(std::uint32_t) + sizeof(crypto::public_key) +
                                                  sizeof(crypto::signature);

bool encode_relay_vote(const RelayVote& vote, std::string& out);
bool decode_relay_vote(std::string_view blob, RelayVote& out);

/// Apply `vote` into the process-wide collector (reset on height/txid change).
/// Optionally gossips via `set_relay_new_vote` when a new signature is accepted.
bool apply_relay_vote(const RelayVote& vote, bool relay_if_new = true);

/// Daemon Arq-Net sets this so a newly accepted vote is gossiped to the Blink quorum.
void set_relay_new_vote(void (*fn)(const RelayVote&));

/// When true, `blocker()` reports wire connectivity as ready (`"none"`).
void set_wire_connected(bool connected) noexcept;
bool wire_connected() noexcept;

const char* blocker() noexcept;

struct Vote
{
  std::uint32_t validator_index = 0;
  crypto::public_key pub{};
  crypto::signature signature{};
};

class Collector
{
public:
  void reset(std::uint64_t height, const crypto::hash& txid);
  bool add_vote(const Vote& vote, const crypto::hash& hashed);
  std::uint64_t height() const;
  crypto::hash txid() const;
  std::size_t signature_count() const;
  bool majority_ok(std::size_t quorum_size = k_quorum_size) const;

private:
  mutable std::mutex mu_;
  std::uint64_t height_ = 0;
  crypto::hash txid_{};
  std::vector<Vote> votes_;
};

Collector& collector();
} // namespace arq_blink
