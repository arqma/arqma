// Copyright (c)      2018-2022, The Arqma Project
// Copyright (c)      2018, The Loki Project
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

#pragma once

#include <boost/variant.hpp>
#include <mutex>
#include <shared_mutex>
#include "serialization/serialization.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_core/service_node_rules.h"
#include "cryptonote_core/service_node_voting.h"
#include "cryptonote_core/service_node_quorum_cop.h"
#include "common/util.h"

namespace cryptonote
{
  class Blockchain;
  class BlockchainDB;
  struct checkpoint_t;
};

namespace service_nodes
{
  constexpr uint64_t INVALID_HEIGHT = static_cast<uint64_t>(-1);
  struct checkpoint_vote_record
  {
    uint64_t height = INVALID_HEIGHT;
    bool voted = true;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(height)
      KV_SERIALIZE(voted)
    END_KV_SERIALIZE_MAP()
  };

  struct proof_info
  {
    uint64_t timestamp = 0;
    uint64_t effective_timestamp = 0;
    std::array<checkpoint_vote_record, CHECKPOINT_NUM_QUORUMS_TO_PARTICIPATE_IN> votes;
    uint8_t vote_index = 0;
    std::array<std::pair<uint32_t, uint64_t>, 2> public_ips = {};

    bool storage_server_reachable = true;
    uint64_t storage_server_reachable_timestamp = 0;
    proof_info() { votes.fill({}); }

    uint32_t public_ip = 0;
    uint16_t storage_port = 0;
    uint16_t arqnet_port = 0;
    std::array<uint16_t, 3> version{{0,0,0}};
    crypto::ed25519_public_key pubkey_ed25519 = crypto::ed25519_public_key::null();
    crypto::x25519_public_key pubkey_x25519 = crypto::x25519_public_key::null();
    void update_pubkey(const crypto::ed25519_public_key &pk);
    bool update(uint64_t ts, uint32_t ip, uint16_t s_port, uint16_t a_port, std::array<uint16_t, 3> ver, const crypto::ed25519_public_key &pk_ed, const crypto::x25519_public_key &pk_x2);
    void store(const crypto::public_key &pubkey, cryptonote::Blockchain &blockchain);
  };

  struct service_node_info // registration information
  {
    enum class version_t : uint8_t
    {
      v0,
      v1,

      _count
    };

    struct contribution_t
    {
      enum class version_t : uint8_t {
        v0,

        _count
      };

      version_t version{version_t::v0};
      crypto::public_key key_image_pub_key{};
      crypto::key_image key_image{};
      uint64_t amount = 0;

      contribution_t() = default;
      contribution_t(version_t version, const crypto::public_key &pubkey, const crypto::key_image &key_image, uint64_t amount)
        : version{version}, key_image_pub_key{pubkey}, key_image{key_image}, amount{amount} {}

      BEGIN_SERIALIZE_OBJECT()
        ENUM_FIELD(version, version < version_t::_count)
        FIELD(key_image_pub_key)
        FIELD(key_image)
        VARINT_FIELD(amount)
      END_SERIALIZE()
    };

    struct contributor_t
    {
      uint8_t version = 0;
      uint64_t amount = 0;
      uint64_t reserved = 0;
      cryptonote::account_public_address address{};
      std::vector<contribution_t> locked_contributions;

      contributor_t() = default;
      contributor_t(uint64_t reserved_, const cryptonote::account_public_address& address_) : reserved(reserved_), address(address_)
      {
        *this = {};
        reserved = reserved_;
        address = address_;
      }

      BEGIN_SERIALIZE_OBJECT()
        VARINT_FIELD(version)
        VARINT_FIELD(amount)
        VARINT_FIELD(reserved)
        FIELD(address)
        FIELD(locked_contributions)
      END_SERIALIZE()
    };

    uint64_t registration_height = 0;
    uint64_t requested_unlock_height = 0;
    uint64_t last_reward_block_height = 0;
    uint32_t last_reward_transaction_index = 0;
    uint32_t decommission_count = 0;
    uint64_t active_since_height = 0;
    uint64_t last_decommission_height = 0;
    std::vector<contributor_t> contributors;
    uint64_t total_contributed = 0;
    uint64_t total_reserved = 0;
    uint64_t staking_requirement = 0;
    uint64_t portions_for_operator = 0;
    swarm_id_t swarm_id = 0;
    cryptonote::account_public_address operator_address{};
    uint64_t last_ip_change_height = 0;
    version_t version = tools::enum_top<version_t>;

    std::shared_ptr<proof_info> proof = std::make_shared<proof_info>();

    service_node_info() = default;
    bool is_fully_funded() const { return total_contributed >= staking_requirement; }
    bool is_decommissioned() const { return active_since_height < 0; }
    bool is_active() const { return is_fully_funded() && !is_decommissioned(); }

    bool can_transition_to_state(uint8_t hard_fork_version, uint64_t height, new_state proposed_state) const;
    bool can_be_voted_on(uint64_t block_height) const;
    size_t total_num_locked_contributions() const;

    BEGIN_SERIALIZE_OBJECT()
      ENUM_FIELD(version, version < version_t::_count)
      VARINT_FIELD(registration_height)
      VARINT_FIELD(requested_unlock_height)
      VARINT_FIELD(last_reward_block_height)
      VARINT_FIELD(last_reward_transaction_index)
      VARINT_FIELD(decommission_count)
      VARINT_FIELD(active_since_height)
      VARINT_FIELD(last_decommission_height)
      FIELD(contributors)
      VARINT_FIELD(total_contributed)
      VARINT_FIELD(total_reserved)
      VARINT_FIELD(staking_requirement)
      VARINT_FIELD(portions_for_operator)
      FIELD(operator_address)
      VARINT_FIELD(swarm_id)
      if (version < version_t::v1)
      {
        uint32_t fake_ip = 0;
        uint16_t fake_port = 0;
        VARINT_FIELD_N("public_ip", fake_ip)
        VARINT_FIELD_N("storage_port", fake_port)
      }
      VARINT_FIELD(last_ip_change_height)
      if (version < version_t::v1)
      {
        crypto::ed25519_public_key fake_pk = crypto::ed25519_public_key::null();
        FIELD_N("pubkey_ed25519", fake_pk)
      }
      if (version >= version_t::v1)
      {
        uint16_t fake_port = 0;
        VARINT_FIELD_N("arqnet_port", fake_port)
      }
    END_SERIALIZE()
  };

  using pubkey_and_sninfo = std::pair<crypto::public_key, std::shared_ptr<const service_node_info>>;
  using service_nodes_infos_t = std::unordered_map<crypto::public_key, std::shared_ptr<const service_node_info>>;

  struct service_node_pubkey_info
  {
    crypto::public_key pubkey;
    std::shared_ptr<const service_node_info> info;

    service_node_pubkey_info() = default;
    service_node_pubkey_info(const pubkey_and_sninfo &pair) : pubkey{pair.first}, info{pair.second} {}

    BEGIN_SERIALIZE_OBJECT()
      FIELD(pubkey)
      if (!W)
        info = std::make_shared<service_node_info>();
      FIELD_N("info", const_cast<service_node_info &>(*info));
    END_SERIALIZE()
  };

  struct key_image_blacklist_entry
  {
    enum struct version_t : uint8_t { version_0, version_1, _count };
    version_t version{version_t::version_1};
    crypto::key_image key_image;
    uint64_t unlock_height = 0;
    uint64_t amount = 0;

    key_image_blacklist_entry() = default;
    key_image_blacklist_entry(version_t version, const crypto::key_image &key_image, uint64_t unlock_height, uint64_t amount)
      : version{version}, key_image{key_image}, unlock_height{unlock_height}, amount(amount) {}

    bool operator==(const key_image_blacklist_entry &other) const { return key_image == other.key_image; }
    bool operator==(const crypto::key_image &image) const { return key_image == image; }

    BEGIN_SERIALIZE_OBJECT()
      ENUM_FIELD(version, version < version_t::_count)
      FIELD(key_image)
      VARINT_FIELD(unlock_height)
      VARINT_FIELD(amount)
    END_SERIALIZE()
  };

  struct payout_entry
  {
    cryptonote::account_public_address address;
    uint64_t portions;
  };

  struct block_winner
  {
    crypto::public_key key;
    std::vector<payout_entry> payouts;
  };

  struct service_node_keys {
    crypto::secret_key key;
    crypto::public_key pub;

    crypto::ed25519_secret_key key_ed25519;
    crypto::ed25519_public_key pub_ed25519;

    crypto::x25519_secret_key key_x25519;
    crypto::x25519_public_key pub_x25519;
  };

  class service_node_list
    : public cryptonote::BlockAddedHook,
      public cryptonote::BlockchainDetachedHook,
      public cryptonote::InitHook,
      public cryptonote::ValidateMinerTxHook,
      public cryptonote::AltBlockAddedHook
  {
  public:
    explicit service_node_list(cryptonote::Blockchain& blockchain);

    service_node_list(const service_node_list &) = delete;
    service_node_list &operator=(const service_node_list &) = delete;

    bool block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, cryptonote::checkpoint_t const *checkpoint) override;
    void blockchain_detached(uint64_t height) override;
    void init() override;
    bool validate_miner_tx(const crypto::hash& prev_id, const cryptonote::transaction& miner_tx, uint64_t height, uint8_t hard_fork_version, cryptonote::block_reward_parts const &base_reward) const override;
    bool alt_block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, cryptonote::checkpoint_t const *checkpoint) override;
    block_winner get_block_winner() const { std::lock_guard lock{m_sn_mutex}; return m_state.get_block_winner(); }

    bool is_service_node(const crypto::public_key& pubkey, bool require_cative = true) const;
    bool is_key_image_locked(crypto::key_image const &check_image, uint64_t *unlock_height = nullptr, service_node_info::contribution_t *the_locked_contribution = nullptr) const;

    std::shared_ptr<const quorum> get_quorum(quorum_type type, uint64_t height, bool include_old = false, std::vector<std::shared_ptr<const quorum>> *alt_states = nullptr) const;
    bool get_quorum_pubkey(quorum_type type, quorum_group group, uint64_t height, size_t quorum_size, crypto::public_key &key) const;

    size_t get_service_node_count() const;
    std::vector<service_node_pubkey_info> get_service_node_list_state(const std::vector<crypto::public_key> &service_node_pubkeys = {}) const;
    const std::vector<key_image_blacklist_entry> &get_blacklisted_key_images() const { return m_state.key_image_blacklist; }

    template <typename Func>
    void access_proof(const crypto::public_key &pubkey, Func f) const
    {
      std::unique_lock lock{m_sn_mutex};
      auto it = m_proofs.find(pubkey);
      if (it != m_proofs.end())
        f(it->second);
    }

    crypto::public_key get_pubkey_from_x25519(const crypto::x25519_public_key &x25519) const;

    void initialize_x25519_map();

    template <typename It, typename Func>
    void for_each_service_node_info_and_proof(It begin, It end, Func f) const
    {
      static const proof_info empty_proof{};
      std::lock_guard lock{m_sn_mutex};
      for (auto sni_end = m_state.service_nodes_infos.end(); begin != end; ++begin)
      {
        auto it = m_state.service_nodes_infos.find(*begin);
        if (it != sni_end)
        {
          auto pit = m_proofs.find(it->first);
          f(it->first, *it->second, (pit != m_proofs.end() ? pit->second : empty_proof));
        }
      }
    }

    void set_my_service_node_keys(const service_node_keys *keys);
    void set_quorum_history_storage(uint64_t hist_size); // 0 = none (default), 1 = unlimited, N = # of blocks
    bool store();

    cryptonote::NOTIFY_UPTIME_PROOF::request generate_uptime_proof(const service_node_keys &keys, uint32_t public_ip, uint16_t storage_port, uint16_t arqnet_port) const;
    bool handle_uptime_proof(cryptonote::NOTIFY_UPTIME_PROOF::request const &proof, bool &my_uptime_proof_confirmation);

    void record_checkpoint_vote(crypto::public_key const &pubkey, uint64_t height, bool voted);
    void cleanup_proofs();
    bool set_storage_server_peer_reachable(crypto::public_key const &pubkey, bool value);

    struct quorum_for_serialization
    {
      uint8_t version;
      uint64_t height;
      quorum quorums[tools::enum_count<quorum_type>];

      BEGIN_SERIALIZE_OBJECT()
        FIELD(version)
        FIELD(height)
        FIELD_N("obligations_quorum", quorums[static_cast<uint8_t>(quorum_type::obligations)])
        FIELD_N("checkpointing_quorum", quorums[static_cast<uint8_t>(quorum_type::checkpointing)])
      END_SERIALIZE()
    };

    struct state_serialized
    {
      enum struct version_t : uint8_t
      {
        version_0,
        _count
      };
      static version_t get_version(uint8_t) { return version_t::version_0; }

      version_t version;
      uint64_t height;
      std::vector<service_node_pubkey_info> infos;
      std::vector<key_image_blacklist_entry> key_image_blacklist;
      quorum_for_serialization quorums;
      bool only_stored_quorums;
      crypto::hash block_hash;

      BEGIN_SERIALIZE()
        ENUM_FIELD(version, version < version_t::_count)
        VARINT_FIELD(height)
        FIELD(infos)
        FIELD(key_image_blacklist)
        FIELD(quorums)
        FIELD(only_stored_quorums)
        FIELD(block_hash)
      END_SERIALIZE()
    };

    struct data_for_serialization
    {
      enum struct version_t : uint8_t
      {
        version_0,
        _count
      };
      static version_t get_version(uint8_t) { return version_t::version_0; }

      version_t version;
      std::vector<quorum_for_serialization> quorum_states;
      std::vector<state_serialized> states;
      void clear() { quorum_states.clear(); states.clear(); version = {}; }
      BEGIN_SERIALIZE()
        ENUM_FIELD(version, version < version_t::_count)
        FIELD(quorum_states)
        FIELD(states)
      END_SERIALIZE()
    };

    struct state_t;
    using state_set = std::set<state_t, std::less<>>;
    using block_height = uint64_t;
    struct state_t
    {
      crypto::hash block_hash{crypto::null_hash};
      bool only_loaded_quorums{false};
      service_nodes_infos_t service_nodes_infos;
      std::vector<key_image_blacklist_entry> key_image_blacklist;
      block_height height{0};
      mutable quorum_manager quorums;
      service_node_list* sn_list;

      state_t(service_node_list* snl) : sn_list{snl} {}
      state_t(service_node_list* snl, state_serialized &&state);

      friend bool operator<(const state_t &a, const state_t &b) { return a.height < b.height; }
      friend bool operator<(const state_t &s, block_height h) { return s.height < h; }
      friend bool operator<(block_height h, const state_t &s) { return h < s.height; }

      std::vector<pubkey_and_sninfo> active_service_nodes_infos() const;
      std::vector<pubkey_and_sninfo> decommissioned_service_nodes_infos() const;
      std::vector<crypto::public_key> get_expired_nodes(cryptonote::BlockchainDB const &db, cryptonote::network_type nettype, uint64_t block_height) const;
      void update_from_block(cryptonote::BlockchainDB const &db, cryptonote::network_type nettype, state_set const &state_history, state_set const &state_archive, std::unordered_map<crypto::hash, state_t> const &alt_states, const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, const service_node_keys *my_keys);

      bool process_registration_tx(cryptonote::network_type nettype, cryptonote::block const &block, const cryptonote::transaction& tx, uint32_t index, const service_node_keys *my_keys);
      bool process_contribution_tx(cryptonote::network_type nettype, cryptonote::block const &block, const cryptonote::transaction& tx, uint32_t index);
      bool process_state_change_tx(state_set const &state_history, state_set const &state_archive, std::unordered_map<crypto::hash, state_t> const &alt_states, cryptonote::network_type nettype, const cryptonote::block &block, const cryptonote::transaction& tx, const service_node_keys *my_keys);
      bool process_key_image_unlock_tx(cryptonote::network_type nettype, uint64_t block_height, const cryptonote::transaction &tx);
      block_winner get_block_winner() const;
    };

  private:

    bool m_rescanning = false;
    void rescan_starting_from_curr_state(bool store_to_disk);
    void process_block(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs);

    void reset(bool delete_db_entry = false);
    bool load(uint64_t current_height);

    mutable std::recursive_mutex m_sn_mutex;
    cryptonote::Blockchain& m_blockchain;
    const service_node_keys *m_service_node_keys;
    uint64_t m_store_quorum_history = 0;

    std::unordered_map<crypto::x25519_public_key, std::pair<crypto::public_key, time_t>> m_x25519_to_pub;
    time_t m_x25519_map_last_pruned = 0;
    mutable std::shared_mutex m_x25519_map_mutex;

    struct quorums_by_height
    {
      quorums_by_height() = default;
      quorums_by_height(uint64_t height, quorum_manager quorums) : height(height), quorums(std::move(quorums)) {}
      uint64_t height;
      quorum_manager quorums;
    };

    std::deque<quorums_by_height> m_old_quorum_states;
    state_set m_state_history;
    state_set m_state_archive;
    state_t m_state;
    std::unordered_map<crypto::hash, state_t> m_alt_state;
    std::unordered_map<crypto::public_key, proof_info> m_proofs;
    bool m_state_added_to_archive;
    data_for_serialization m_cache_long_term_data;
    data_for_serialization m_cache_short_term_data;
    std::string m_cache_data_blob;
  };

  bool is_registration_tx(cryptonote::network_type nettype, uint8_t hard_fork_version, const cryptonote::transaction& tx, uint64_t block_timestamp, uint64_t block_height, uint32_t index, crypto::public_key& key, service_node_info& info);
  bool reg_tx_extract_fields(const cryptonote::transaction& tx, std::vector<cryptonote::account_public_address>& addresses, uint64_t& portions_for_operator, std::vector<uint64_t>& portions, uint64_t& expiration_timestamp, crypto::public_key& service_node_key, crypto::signature& signature, crypto::public_key& tx_pub_key);
  uint64_t offset_testing_quorum_height(quorum_type type, uint64_t height);

  struct converted_registration_args
  {
    bool success;
    std::vector<cryptonote::account_public_address> addresses;
    std::vector<uint64_t> portions;
    uint64_t portions_for_operator;
    std::string err_msg;
  };
  converted_registration_args convert_registration_args(cryptonote::network_type nettype, const std::vector<std::string>& args, uint64_t staking_requirement);

  bool make_registration_cmd(cryptonote::network_type nettype, uint64_t staking_requirement, const std::vector<std::string>& args, const service_node_keys &keys, std::string &cmd, bool make_friendly, boost::optional<std::string&> err_msg);

  const static cryptonote::account_public_address null_address{crypto::null_pkey, crypto::null_pkey};
  const static std::vector<payout_entry> null_winner = {{null_address, STAKING_SHARE_PARTS}};
  const static block_winner null_block_winner = {crypto::null_pkey, {null_winner}};
}