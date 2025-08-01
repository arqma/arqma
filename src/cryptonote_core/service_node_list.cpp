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

#include <functional>
#include <random>
#include <algorithm>
#include <chrono>

#include <boost/endian/conversion.hpp>

extern "C" {
#include <sodium.h>
}

#include "ringct/rctSigs.h"
#include "wallet/wallet2.h"
#include "cryptonote_tx_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "int-util.h"
#include "common/scoped_message_writer.h"
#include "common/i18n.h"
#include "common/util.h"
#include "common/random.h"
#include "blockchain.h"
#include "common/lock.h"
#include "service_node_quorum_cop.h"
#include "net/local_ip.h"
#include "time_helper.h"

#include "service_node_list.h"
#include "service_node_rules.h"
#include "service_node_swarm.h"
#include "version.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "service_nodes"

namespace arqma_bc = config::blockchain_settings;

namespace service_nodes
{
  size_t constexpr STORE_LONG_TERM_STATE_INTERVAL = 10000;

  constexpr int X25519_MAP_PRUNING_INTERVAL = 3000;
  constexpr int X25519_MAP_PRUNING_LAG = 86400;
  static_assert(X25519_MAP_PRUNING_LAG > UPTIME_PROOF_MAX_TIME_IN_SECONDS, "x25519 map pruning lag is too short!");

  static uint64_t short_term_state_cull_height(uint8_t hard_fork_version, uint64_t block_height)
  {
    size_t constexpr DEFAULT_SHORT_TERM_STATE_HISTORY = 6 * STATE_CHANGE_TX_LIFETIME_IN_BLOCKS;
    uint64_t result = (block_height < DEFAULT_SHORT_TERM_STATE_HISTORY) ? 0 : block_height - DEFAULT_SHORT_TERM_STATE_HISTORY;

    return result;
  }

  static constexpr service_node_info::version_t get_minimum_sn_info_version(uint8_t hard_fork_version)
  {
    return hard_fork_version < cryptonote::network_version_17 ? service_node_info::version_t::v0 : service_node_info::version_t::v1;
  }

  service_node_list::service_node_list(cryptonote::Blockchain& blockchain)
    : m_blockchain(blockchain)
    , m_service_node_keys(nullptr)
    , m_store_quorum_history(0)
    , m_state{this}
    , m_state_added_to_archive(false)
  {
  }

  //----------------------------------------------------------------------------
  void service_node_list::rescan_starting_from_curr_state(bool store_to_disk)
  {
    if (m_blockchain.get_current_hard_fork_version() < 16)
    {
      return;
    }

    m_rescanning = true;
    ARQMA_DEFER { m_rescanning = false; };

    auto scan_start = std::chrono::high_resolution_clock::now();
    uint64_t chain_height = m_blockchain.get_current_blockchain_height();
    uint64_t current_height = chain_height - 1;
    if (m_state.height == current_height)
      return;

    MGINFO("Recalculating service nodes list, scanning blockchain from height " << m_state.height << " to: " << current_height);
    std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
    std::vector<cryptonote::transaction> txs;
    std::vector<crypto::hash> missed_txs;
    auto work_start = std::chrono::high_resolution_clock::now();
    uint64_t start_height = m_state.height;
    for(uint64_t i = 0; m_state.height < current_height; i++, start_height = m_state.height)
    {
      if(i > 0 && i % 10 == 0)
      {
        if (store_to_disk) store();
        auto work_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(work_end - work_start);
        MGINFO("... scanning height " << m_state.height << " (" << duration.count() / 1000.f << "s)");
        work_start = std::chrono::high_resolution_clock::now();
      }

      blocks.clear();
      if(!m_blockchain.get_blocks(m_state.height + 1, 1000, blocks))
      {
        MERROR("Unable to initialize service nodes list");
        return;
      }
      if (blocks.empty())
        break;

      for(const auto& block_pair : blocks)
      {
        txs.clear();
        missed_txs.clear();

        const cryptonote::block& block = block_pair.second;
        if(!m_blockchain.get_transactions(block.tx_hashes, txs, missed_txs))
        {
          MERROR("Unable to get transactions for block " << block.hash);
          return;
        }

        process_block(block, txs);
      }

      if (start_height == m_state.height)
      {
        MERROR("Unexpected state height did not change after processing blocks, height is: " << start_height);
        return;
      }
    }

    auto scan_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(scan_end - scan_start);
    MGINFO("Done recalculating service node list (" << duration.count() / 1000.f << "s)");
    if (store_to_disk) store();
  }
  //----------------------------------------------------------------------------
  void service_node_list::init()
  {
    std::lock_guard lock(m_sn_mutex);
    if (m_blockchain.get_current_hard_fork_version() < 16)
    {
      reset(true);
      return;
    }

    uint64_t current_height = m_blockchain.get_current_blockchain_height();
    bool loaded = load(current_height);
    if (loaded && m_old_quorum_states.size() < std::min(m_store_quorum_history, uint64_t{10}))
    {
      LOG_PRINT_L0("Full history storage requested, but " << m_old_quorum_states.size() << " old quorum states found");
      loaded = false;
    }

    bool store_to_disk = false;
    if (!loaded || m_state.height > current_height)
    {
      reset(true);
      store_to_disk = true;
    }

    rescan_starting_from_curr_state(store_to_disk);
  }
  //----------------------------------------------------------------------------
  template <typename UnaryPredicate>
  static std::vector<service_nodes::pubkey_and_sninfo> sort_and_filter(const service_nodes_infos_t &sns_infos, UnaryPredicate p, bool reserve = true)
  {
    std::vector<pubkey_and_sninfo> result;
    if (reserve) result.reserve(sns_infos.size());
    for (const pubkey_and_sninfo &key_info : sns_infos)
      if (p(*key_info.second))
        result.push_back(key_info);

    std::sort(result.begin(), result.end(), [](const pubkey_and_sninfo &a, const pubkey_and_sninfo &b)
    {
      return memcmp(reinterpret_cast<const void*>(&a), reinterpret_cast<const void*>(&b), sizeof(a)) < 0;
    });
    return result;
  }
  //----------------------------------------------------------------------------
  std::vector<pubkey_and_sninfo> service_node_list::state_t::active_service_nodes_infos() const
  {
    return sort_and_filter(service_nodes_infos, [](const service_node_info &info) { return info.is_active(); });
  }
  //----------------------------------------------------------------------------
  std::vector<pubkey_and_sninfo> service_node_list::state_t::decommissioned_service_nodes_infos() const
  {
    return sort_and_filter(service_nodes_infos, [](const service_node_info &info) { return info.is_decommissioned() && info.is_fully_funded(); }, /*reserve=*/ false);
  }
  //----------------------------------------------------------------------------
  std::shared_ptr<const quorum> service_node_list::get_quorum(quorum_type type, uint64_t height, bool include_old, std::vector<std::shared_ptr<const quorum>> *alt_quorums) const
  {
    height = offset_testing_quorum_height(type, height);
    std::lock_guard lock(m_sn_mutex);
    quorum_manager const *quorums = nullptr;
    if (height == m_state.height)
      quorums = &m_state.quorums;
    else
    {
      auto it = m_state_history.find(height);
      if (it != m_state_history.end())
        quorums = &it->quorums;

      if (!quorums)
      {
        auto it = m_state_archive.find(height);
        if (it != m_state_archive.end())
          quorums = &it->quorums;
      }
    }

    if (!quorums && include_old)
    {
      auto it = std::lower_bound(m_old_quorum_states.begin(), m_old_quorum_states.end(), height, [](quorums_by_height const &entry, uint64_t height)
      {
        return entry.height < height;
      });

      if (it != m_old_quorum_states.end() && it->height == height)
        quorums = &it->quorums;
    }

    if (!quorums)
      return nullptr;

    if (alt_quorums)
    {
      for (std::pair<crypto::hash, state_t> const &hash_to_state : m_alt_state)
      {
        state_t const &alt_state = hash_to_state.second;
        if (alt_state.height == height)
        {
          std::shared_ptr<const quorum> alt_result = alt_state.quorums.get(type);
          if (alt_result)
            alt_quorums->push_back(alt_result);
        }
      }
    }

    std::shared_ptr<const quorum> result = quorums->get(type);
    return result;
  }
  //----------------------------------------------------------------------------
  static bool get_pubkey_from_quorum(quorum const &quorum, quorum_group group, size_t quorum_index, crypto::public_key &key)
  {
    std::vector<crypto::public_key> const *array = nullptr;
    if (group == quorum_group::validator) array = &quorum.validators;
    else if (group == quorum_group::worker) array = &quorum.workers;
    else
    {
      MERROR("Invalid quorum group specified");
      return false;
    }

    if (quorum_index >= array->size())
    {
      MERROR("Quorum indexing out of bounds: " << quorum_index << ", quorum_size: " << array->size());
      return false;
    }

    key = (*array)[quorum_index];
    return true;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::get_quorum_pubkey(quorum_type type, quorum_group group, uint64_t height, size_t quorum_index, crypto::public_key &key) const
  {
    std::shared_ptr<const quorum> quorum = get_quorum(type, height);
    if (!quorum)
    {
      LOG_PRINT_L1("Quorum for height: " << height << ", was not stored by the daemon");
      return false;
    }

    bool result = get_pubkey_from_quorum(*quorum, group, quorum_index, key);
    return result;
  }
  //----------------------------------------------------------------------------
  size_t service_node_list::get_service_node_count() const
  {
    std::lock_guard lock(m_sn_mutex);
    return m_state.service_nodes_infos.size();
  }
  //----------------------------------------------------------------------------
  std::vector<service_node_pubkey_info> service_node_list::get_service_node_list_state(const std::vector<crypto::public_key> &service_node_pubkeys) const
  {
    std::lock_guard lock(m_sn_mutex);
    std::vector<service_node_pubkey_info> result;

    if(service_node_pubkeys.empty())
    {
      result.reserve(m_state.service_nodes_infos.size());

      for (const auto &info : m_state.service_nodes_infos)
        result.emplace_back(info);
    }
    else
    {
      result.reserve(service_node_pubkeys.size());
      for(const auto& it : service_node_pubkeys)
      {
        auto find_it = m_state.service_nodes_infos.find(it);
        if (find_it != m_state.service_nodes_infos.end())
          result.emplace_back(*find_it);
      }
    }

    return result;
  }
  //----------------------------------------------------------------------------
  void service_node_list::set_my_service_node_keys(const service_node_keys *keys)
  {
    std::lock_guard lock(m_sn_mutex);
    m_service_node_keys = keys;
  }
  //----------------------------------------------------------------------------
  void service_node_list::set_quorum_history_storage(uint64_t hist_size)
  {
    if (hist_size == 1)
      hist_size = std::numeric_limits<uint64_t>::max();
    m_store_quorum_history = hist_size;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::is_service_node(const crypto::public_key& pubkey, bool require_active) const
  {
    std::lock_guard lock(m_sn_mutex);
    auto it = m_state.service_nodes_infos.find(pubkey);
    return it != m_state.service_nodes_infos.end() && (!require_active || it->second->is_active());
  }
  //----------------------------------------------------------------------------
  bool service_node_list::is_key_image_locked(crypto::key_image const &check_image, uint64_t *unlock_height, service_node_info::contribution_t *the_locked_contribution) const
  {
    for(const auto& pubkey_info : m_state.service_nodes_infos)
    {
      const service_node_info &info = *pubkey_info.second;
      for(const service_node_info::contributor_t &contributor : info.contributors)
      {
        for(const service_node_info::contribution_t &contribution : contributor.locked_contributions)
        {
          if(check_image == contribution.key_image)
          {
            if(the_locked_contribution)
              *the_locked_contribution = contribution;
            if(unlock_height)
              *unlock_height = info.requested_unlock_height;

            return true;
          }
        }
      }
    }
    return false;
  }
  //----------------------------------------------------------------------------
  bool reg_tx_extract_fields(const cryptonote::transaction& tx, std::vector<cryptonote::account_public_address>& addresses, uint64_t& portions_for_operator, std::vector<uint64_t>& portions, uint64_t& expiration_timestamp, crypto::public_key& service_node_key, crypto::signature& signature)
  {
    cryptonote::tx_extra_service_node_register registration;
    if(!get_service_node_register_from_tx_extra(tx.extra, registration))
      return false;
    if(!cryptonote::get_service_node_pubkey_from_tx_extra(tx.extra, service_node_key))
      return false;

    addresses.clear();
    addresses.reserve(registration.m_public_spend_keys.size());
    for (size_t i = 0; i < registration.m_public_spend_keys.size(); i++)
    {
      addresses.emplace_back();
      addresses.back().m_spend_public_key = registration.m_public_spend_keys[i];
      addresses.back().m_view_public_key = registration.m_public_view_keys[i];
    }

    portions_for_operator = registration.m_portions_for_operator;
    portions = registration.m_portions;
    expiration_timestamp = registration.m_expiration_timestamp;
    signature = registration.m_service_node_signature;

    return true;
  }
  //----------------------------------------------------------------------------
  uint64_t offset_testing_quorum_height(quorum_type type, uint64_t height)
  {
    uint64_t result = height;
    if (type == quorum_type::checkpointing)
    {
      if (result < REORG_SAFETY_BUFFER_IN_BLOCKS)
        return 0;
      result -= REORG_SAFETY_BUFFER_IN_BLOCKS;
    }
    return result;
  }
  //----------------------------------------------------------------------------
  struct parsed_tx_contribution
  {
    cryptonote::account_public_address address;
    uint64_t transferred;
    crypto::secret_key tx_key;
    std::vector<service_node_info::contribution_t> locked_contributions;
  };
  //----------------------------------------------------------------------------
  static uint64_t get_reg_tx_staking_output_contribution(const cryptonote::transaction& tx, int i, crypto::key_derivation const derivation, hw::device& hwdev)
  {
    if(tx.vout[i].target.type() != typeid(cryptonote::txout_to_key))
    {
      return 0;
    }

    rct::key mask;
    uint64_t money_transferred = 0;

    crypto::secret_key scalar1;
    hwdev.derivation_to_scalar(derivation, i, scalar1);
    try
    {
      switch(tx.rct_signatures.type)
      {
        case rct::RCTTypeSimple:
        case rct::RCTTypeBulletproof:
        case rct::RCTTypeBulletproof2:
          money_transferred = rct::decodeRctSimple(tx.rct_signatures, rct::sk2rct(scalar1), i, mask, hwdev);
          break;
        case rct::RCTTypeFull:
          money_transferred = rct::decodeRct(tx.rct_signatures, rct::sk2rct(scalar1), i, mask, hwdev);
          break;
        default:
          LOG_PRINT_L0("Unsupported rct type: " << tx.rct_signatures.type);
          return 0;
      }
    }
    catch(const std::exception &e)
    {
      LOG_ERROR("Failed to decode input " << i);
      return 0;
    }

    return money_transferred;
  }
  //----------------------------------------------------------------------------
  static service_node_info &duplicate_info(std::shared_ptr<const service_node_info> &info_ptr)
  {
    auto new_ptr = std::make_shared<service_node_info>(*info_ptr);
    info_ptr = new_ptr;
    return *new_ptr;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::state_t::process_state_change_tx(state_set const &state_history, state_set const &state_archive, std::unordered_map<crypto::hash, state_t> const &alt_states, cryptonote::network_type nettype, const cryptonote::block &block, const cryptonote::transaction &tx, const service_node_keys *my_keys)
  {
    if(tx.tx_type != cryptonote::txtype::state_change)
      return false;

    uint8_t const hard_fork_version = block.major_version;
    cryptonote::tx_extra_service_node_state_change state_change;
    if(!cryptonote::get_service_node_state_change_from_tx_extra(tx.extra, state_change))
    {
      MERROR("Transaction: " << cryptonote::get_transaction_hash(tx) << " did not have valid state change data in tx extra, rejecting malformed tx");
      return false;
    }

    auto it = state_history.find(state_change.block_height);
    if (it == state_history.end())
    {
      MERROR("Transaction: " << cryptonote::get_transaction_hash(tx) << " in block " << cryptonote::get_block_height(block) << " " << cryptonote::get_block_hash(block) << ", references quorum at height " << state_change.block_height << " but that height is not stored!");
      return false;
    }

    quorum_manager const *quorums = &it->quorums;
    cryptonote::tx_verification_context tvc = {};
    if (!verify_tx_state_change(state_change, cryptonote::get_block_height(block), tvc, *quorums->obligations, hard_fork_version))
    {
      quorums = nullptr;
      for (std::pair<crypto::hash, state_t> const &entry : alt_states)
      {
        state_t const &alt_state = entry.second;
        if (alt_state.height != state_change.block_height)
          continue;

        quorums = &alt_state.quorums;
        if (!verify_tx_state_change(state_change, cryptonote::get_block_height(block), tvc, *quorums->obligations, hard_fork_version))
        {
          quorums = nullptr;
          continue;
        }
      }
    }

    if (!quorums)
    {
      MERROR("Could not get a quorum that could completely validate the votes from state change in tx: " << get_transaction_hash(tx) << ", skipping transaction");
      return false;
    }

    crypto::public_key key;
    if (!get_pubkey_from_quorum(*quorums->obligations, quorum_group::worker, state_change.service_node_index, key))
    {
      MERROR("Retrieving the public key from state change in tx: " << cryptonote::get_transaction_hash(tx) << " failed");
      return false;
    }

    auto iter = service_nodes_infos.find(key);
    if(iter == service_nodes_infos.end())
    {
      LOG_PRINT_L2("Received state change tx for non-registered service node " << key << " (perhaps a delayed tx?)");
      return false;
    }

    uint64_t block_height = cryptonote::get_block_height(block);
    auto &info = duplicate_info(iter->second);
    bool is_me = my_keys && my_keys->pub == key;

    switch (state_change.state) {
      case new_state::deregister:
        if (is_me)
          MGINFO_RED("Deregistration for service node (yours): " << key);
        else
          LOG_PRINT_L1("Deregistration for service node: " << key);

        for (const auto& contributor : info.contributors)
        {
          for (const auto& contribution : contributor.locked_contributions)
          {
            key_image_blacklist.emplace_back();
            key_image_blacklist_entry &entry = key_image_blacklist.back();
            entry.key_image = contribution.key_image;
            entry.unlock_height = block_height + staking_num_lock_blocks(nettype);
            entry.amount = contribution.amount;
          }
        }

        service_nodes_infos.erase(iter);
        return true;

      case new_state::decommission:
        if (info.is_decommissioned()) {
          LOG_PRINT_L2("Received decommission tx for already-decommissioned service node " << key << "; ignoring");
          return false;
        }

        if (is_me)
          MGINFO_RED("Temporary decommission for service node (yours): " << key);
        else
          LOG_PRINT_L1("Temporary decommission for service node: " << key);

        info.active_since_height = -info.active_since_height;
        info.last_decommission_height = block_height;
        info.decommission_count++;
        info.swarm_id = UNASSIGNED_SWARM_ID;

        if (sn_list && !sn_list->m_rescanning)
        {
          auto &proof = sn_list->m_proofs[key];
          proof.timestamp = proof.effective_timestamp = 0;
          proof.store(key, sn_list->m_blockchain);
        }
        return true;

      case new_state::recommission:
        if (!info.is_decommissioned()) {
          LOG_PRINT_L2("Received recommission tx for already-active service node " << key << "; ignoring");
          return false;
        }

        if (is_me)
          MGINFO_GREEN("Recommission for service node (yours): " << key);
        else
          LOG_PRINT_L1("Recommission for service node: " << key);

        info.active_since_height = block_height;
        info.last_reward_block_height = block_height;
        info.last_reward_transaction_index = std::numeric_limits<uint32_t>::max();

        if (sn_list)
        {
          auto &proof = sn_list->m_proofs[key];
          proof.effective_timestamp = block.timestamp;
          proof.votes.fill({});
        }
        return true;

      case new_state::ip_change_penalty:
        if (info.is_decommissioned()) {
          LOG_PRINT_L2("Received reset position tx for service node " << key << " but it is already decommissioned, ignoring");
          return false;
        }

        if (is_me)
          MGINFO_RED("Reward position reset for service node (yours): " << key);
        else
          LOG_PRINT_L1("Reward position reset for service node: " << key);

        info.last_reward_block_height = block_height;
        info.last_reward_transaction_index = std::numeric_limits<uint32_t>::max();
        info.last_ip_change_height = block_height;
        return true;

      default:
        // dev bug!
        MERROR("BUG: Service node state change tx has unknown state " << static_cast<uint16_t>(state_change.state));
        return false;
    }
  }
  //----------------------------------------------------------------------------
  bool service_node_list::state_t::process_key_image_unlock_tx(cryptonote::network_type nettype, uint64_t block_height, const cryptonote::transaction &tx)
  {
    crypto::public_key snode_key;
    if(!cryptonote::get_service_node_pubkey_from_tx_extra(tx.extra, snode_key))
      return false;

    auto it = service_nodes_infos.find(snode_key);
    if(it == service_nodes_infos.end())
      return false;

    const service_node_info &node_info = *it->second;
    if(node_info.requested_unlock_height != KEY_IMAGE_AWAITING_UNLOCK_HEIGHT)
    {
      LOG_PRINT_L1("Unlock TX: Node already requested an unlock at height: " << node_info.requested_unlock_height << " rejected on height: " << block_height
                                                                             << " for tx: " << cryptonote::get_transaction_hash(tx));
      return false;
    }

    cryptonote::tx_extra_tx_key_image_unlock unlock;
    if (!cryptonote::get_tx_key_image_unlock_from_tx_extra(tx.extra, unlock))
    {
      LOG_PRINT_L1("Unlock TX: Didn't have key image unlock in the tx_extra, rejected on height: "
                   << block_height << " for tx: " << cryptonote::get_transaction_hash(tx));
      return false;
    }

    uint64_t unlock_height = get_locked_key_image_unlock_height(nettype, node_info.registration_height, block_height);
    for(const auto &contributor : node_info.contributors)
    {
      auto cit = std::find_if(contributor.locked_contributions.begin(), contributor.locked_contributions.end(),
                              [&unlock](const service_node_info::contribution_t &contribution)
                              {
                                return unlock.key_image == contribution.key_image;
                              });
      if(cit != contributor.locked_contributions.end())
      {
        crypto::hash const hash = service_nodes::generate_request_stake_unlock_hash(unlock.nonce);
        if(crypto::check_signature(hash, cit->key_image_pub_key, unlock.signature))
        {
          duplicate_info(it->second).requested_unlock_height = unlock_height;
          return true;
        }
        else
        {
          LOG_PRINT_L1("Unlock Transaction: Could not verify key image unlock at tx extra. rejected on height: " << block_height << " for tx: " << get_transaction_hash(tx));
          return false;
        }
      }
    }

    return false;
  }
  //----------------------------------------------------------------------------
  static bool get_contribution(cryptonote::network_type nettype, const cryptonote::transaction& tx, uint64_t block_height, parsed_tx_contribution &parsed_contribution)
  {
    if(!cryptonote::get_service_node_contributor_from_tx_extra(tx.extra, parsed_contribution.address))
      return false;

    if(!cryptonote::get_tx_secret_key_from_tx_extra(tx.extra, parsed_contribution.tx_key))
    {
      MERROR("Contribution TX: There was a service node contributor but no secret key in the tx extra on height: " << block_height << " for tx: " << get_transaction_hash(tx));
      return false;
    }

    // An cryptonote transaction is constructed as follows
    //
    // P = Hs(aR)G + B
    //   where:
    //     P := Stealth Address
    //     a := Receiver's secret view key
    //     B := Receiver's public spend key
    //     R := TX Public Key
    //     G := Elliptic Curve
    //
    // At ArQmA we pack into the tx extra information to reveal information about the TX
    //   A := Public View Key (we pack contributor into tx extra, 'parsed_contribution.address')
    //   r := TX Secret Key   (we pack secret key into tx extra,  'parsed_contribution.tx_key`)

    // Calulate 'Derivation := Hs(Ar)G'
    crypto::key_derivation derivation;
    if(!crypto::generate_key_derivation(parsed_contribution.address.m_view_public_key, parsed_contribution.tx_key, derivation))
    {
      MERROR("Contribution TX: Failed to generate key derivation on height: " << block_height << " for tx: " << get_transaction_hash(tx));
      return false;
    }

    hw::device& hwdev = hw::get_device("default");
    parsed_contribution.transferred = 0;

    // While fulifing Infinite Staking, we lock the key image that would
    // be generated if you tried to send your stake and prevent it from
    // being transacted on the network whilst you are a Service Node.
    // To do this, we calculate the future key image that would be
    // generated when they user tries to spend the staked funds.
    // A key image is derived from the ephemeral, one time transaction
    // private key, 'x' in the Cryptonote Whitepaper.
    //
    // This is only possible to generate if they are the staking to themselves
    // as you need the recipients private keys to generate the key image that
    // would be generated, when they want to spend it in the future.

    cryptonote::tx_extra_tx_key_image_proofs key_image_proofs;
    if(!get_tx_key_image_proofs_from_tx_extra(tx.extra, key_image_proofs))
    {
      MERROR("Contribution TX: Didn't have key image proofs in the tx_extra, rejected on height: " << block_height << " for tx: " << get_transaction_hash(tx));
      return false;
    }

    for(size_t output_index = 0; output_index < tx.vout.size(); ++output_index)
    {
      uint64_t transferred = get_reg_tx_staking_output_contribution(tx, output_index, derivation, hwdev);
      if(transferred == 0)
        continue;

      // So prove that the destination stealth address can be decoded using the
      // staker's packed address, which means that the recipient of the
      // contribution is themselves (and hence they have the necessary secrets
      // to generate the future key image).
      //
      // i.e Verify the packed information is valid by computing the stealth
      // address P' (which should equal P if matching) using
      //
      // 'Derivation := Hs(Ar)G' (we calculated earlier) instead of 'Hs(aR)G'
      // P' = Hs(Ar)G + B
      //    = Hs(aR)G + B
      //    = Derivation + B
      //    = P

      crypto::public_key ephemeral_pub_key;
      {
        // P' := Derivation + B
        if(!hwdev.derive_public_key(derivation, output_index, parsed_contribution.address.m_spend_public_key, ephemeral_pub_key))
        {
          MERROR("Contribution TX: Could not derive TX ephemeral key on height: " << block_height << " for tx: " << get_transaction_hash(tx) << " for output: " << output_index);
          continue;
        }

        // Stealth address public key should match the public key referenced at the TX only if valid information is given);
        const auto& out_to_key = boost::get<cryptonote::txout_to_key>(tx.vout[output_index].target);
        if(out_to_key.key != ephemeral_pub_key)
        {
          MERROR("Contribution TX: Derived TX ephemeral key did not match tx stored key on height: " << block_height << " for tx: " << get_transaction_hash(tx) << " for output: " << output_index);
          continue;
        }
      }

      // To prevent the staker locking any arbitrary key image, the provided
      // key image is included and verified in a ring signature which
      // guarantees that 'the staker proves that he knows such 'x' (one time
      // ephemeral secret key) and that (the future key image) P = xG'.
      // Consequently the key image is not falsified and actually the future
      // key image.

      // The signer can try falsify the key image, but the equation used to
      // construct the key image is re-derived by the verifier, false key
      // images will not match the re-derived key image.

      crypto::public_key const *ephemeral_pub_key_ptr = &ephemeral_pub_key;
      for(auto proof = key_image_proofs.proofs.begin(); proof != key_image_proofs.proofs.end(); proof++)
      {
        if(!crypto::check_ring_signature((const crypto::hash &)(proof->key_image), proof->key_image, &ephemeral_pub_key_ptr, 1, &proof->signature))
          continue;

        parsed_contribution.locked_contributions.emplace_back(service_node_info::contribution_t::version_t::v0, ephemeral_pub_key, proof->key_image, transferred);

        parsed_contribution.transferred += transferred;
        key_image_proofs.proofs.erase(proof);
        break;
      }
    }

    return true;
  }
  //----------------------------------------------------------------------------
  bool is_registration_tx(cryptonote::network_type nettype, uint8_t hard_fork_version, const cryptonote::transaction& tx, uint64_t block_timestamp, uint64_t block_height, uint32_t index, crypto::public_key& key, service_node_info& info)
  {
    crypto::public_key service_node_key;
    std::vector<cryptonote::account_public_address> service_node_addresses;
    std::vector<uint64_t> service_node_portions;
    uint64_t portions_for_operator;
    uint64_t expiration_timestamp;
    crypto::signature signature;

    if(!reg_tx_extract_fields(tx, service_node_addresses, portions_for_operator, service_node_portions, expiration_timestamp, service_node_key, signature))
      return false;

    if(service_node_portions.size() != service_node_addresses.size() || service_node_portions.empty())
      return false;

    if(!check_service_node_portions(service_node_portions))
      return false;

    if(portions_for_operator > STAKING_SHARE_PARTS)
      return false;

    crypto::hash hash;
    if(!get_registration_hash(service_node_addresses, portions_for_operator, service_node_portions, expiration_timestamp, hash))
      return false;
    if(!crypto::check_key(service_node_key) || !crypto::check_signature(hash, service_node_key, signature))
      return false;
    if(expiration_timestamp < block_timestamp)
      return false;

    uint64_t staking_requirement = get_staking_requirement(nettype, block_height);
    cryptonote::account_public_address address;

    parsed_tx_contribution parsed_contribution = {};
    if(!get_contribution(nettype, tx, block_height, parsed_contribution))
      return false;

    const uint64_t min_transfer = get_min_node_contribution(staking_requirement, info.total_reserved, info.total_num_locked_contributions());
    if(parsed_contribution.transferred < min_transfer)
      return false;

    size_t total_num_of_addr = service_node_addresses.size();
    if(std::find(service_node_addresses.begin(), service_node_addresses.end(), parsed_contribution.address) == service_node_addresses.end())
      total_num_of_addr++;

    if(total_num_of_addr > MAX_NUMBER_OF_CONTRIBUTORS)
      return false;


    key = service_node_key;

    info.staking_requirement           = staking_requirement;
    info.operator_address              = service_node_addresses[0];
    info.portions_for_operator         = portions_for_operator;
    info.registration_height           = block_height;
    info.last_reward_block_height      = block_height;
    info.last_reward_transaction_index = index;
    info.swarm_id                      = UNASSIGNED_SWARM_ID;
    info.last_ip_change_height         = block_height;
    info.version                       = get_minimum_sn_info_version(hard_fork_version);

    for(size_t i = 0; i < service_node_addresses.size(); i++)
    {
      auto iter = std::find(service_node_addresses.begin(), service_node_addresses.begin() + i, service_node_addresses[i]);
      if(iter != service_node_addresses.begin() + i)
        return false;

      uint64_t hi, lo, resulthi, resultlo;
      lo = mul128(info.staking_requirement, service_node_portions[i], &hi);
      div128_64(hi, lo, STAKING_SHARE_PARTS, &resulthi, &resultlo);

      info.contributors.emplace_back();
      auto &contributor = info.contributors.back();
      contributor.reserved = resultlo;
      contributor.address = service_node_addresses[i];
      info.total_reserved += resultlo;
    }

    return true;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::state_t::process_registration_tx(cryptonote::network_type nettype, const cryptonote::block &block, const cryptonote::transaction& tx, uint32_t index, const service_node_keys *my_keys)
  {
    uint8_t hard_fork_version = block.major_version;
    uint64_t const block_timestamp = block.timestamp;
    uint64_t const block_height = cryptonote::get_block_height(block);

    crypto::public_key key;
    auto info_ptr = std::make_shared<service_node_info>();
    service_node_info &info = *info_ptr;
    if(!is_registration_tx(nettype, hard_fork_version, tx, block_timestamp, block_height, index, key, info))
      return false;

    const auto iter = service_nodes_infos.find(key);
    if(iter != service_nodes_infos.end())
      return false;

    if (sn_list && !sn_list->m_rescanning)
    {
      auto &proof = sn_list->m_proofs[key];
      proof = {};
      proof.store(key, sn_list->m_blockchain);
    }

    if(my_keys && my_keys->pub == key)
      MGINFO_GREEN("Service Node (yours) registered: " << key << " at height: " << block_height);
    else
      LOG_PRINT_L1("New Service Node registered: " << key << " at height: " << block_height);

    service_nodes_infos[key] = std::move(info_ptr);
    return true;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::state_t::process_contribution_tx(cryptonote::network_type nettype, const cryptonote::block &block, const cryptonote::transaction& tx, uint32_t index)
  {
    const uint64_t block_height = cryptonote::get_block_height(block);
    const uint8_t hard_fork_version = block.major_version;

    crypto::public_key pubkey;

    if(!cryptonote::get_service_node_pubkey_from_tx_extra(tx.extra, pubkey))
      return false;

    parsed_tx_contribution parsed_contribution = {};
    if(!get_contribution(nettype, tx, block_height, parsed_contribution))
    {
      MERROR("Contribution TX: Could not decode contribution for Service Node: " << pubkey << " on height: " << block_height << " for tx: " << cryptonote::get_transaction_hash(tx));
      return false;
    }

    auto iter = service_nodes_infos.find(pubkey);
    if(iter == service_nodes_infos.end())
    {
      LOG_PRINT_L1("Contribution TX: Contribution received for service node: " << pubkey <<
                   ", but could not be found in the service node list on height: " << block_height <<
                   " for tx: " << cryptonote::get_transaction_hash(tx )<< "\n"
                   "This could mean that the service node was deregistered before the contribution was processed.");
      return false;
    }

    const service_node_info& curinfo = *iter->second;
    if(curinfo.is_fully_funded())
    {
      LOG_PRINT_L1("Contribution TX: Service node: " << pubkey <<
                   " is already fully funded, but contribution received on height: "  << block_height <<
                   " for tx: " << cryptonote::get_transaction_hash(tx));
      return false;
    }

    if(!cryptonote::get_tx_secret_key_from_tx_extra(tx.extra, parsed_contribution.tx_key))
      return false;

    auto &contributors = curinfo.contributors;
    bool new_contributor = true;
    size_t contributor_position = 0;
    for (size_t i = 0; i < contributors.size(); i++)
      if (contributors[i].address == parsed_contribution.address)
      {
        contributor_position = i;
        new_contributor = false;
        break;
      }

    {
      bool too_many_contributions = curinfo.total_num_locked_contributions() + parsed_contribution.locked_contributions.size() > MAX_NUMBER_OF_CONTRIBUTORS;
      if(too_many_contributions)
      {
        LOG_PRINT_L1("Contribution TX: Already git the max number of contributions: " << MAX_NUMBER_OF_CONTRIBUTORS << " for contributor: " << cryptonote::get_account_address_as_str(nettype, false, parsed_contribution.address) << " on height: " << block_height << " for tx: " << cryptonote::get_transaction_hash(tx));
        return false;
      }
    }

    {
      const uint64_t min_contribution = get_min_node_contribution(curinfo.staking_requirement, curinfo.total_reserved, curinfo.total_num_locked_contributions());
      if(parsed_contribution.transferred < min_contribution)
      {
        LOG_PRINT_L1("Contribution TX: Amount " << parsed_contribution.transferred << " did not meet min " << min_contribution << " for service node: " << pubkey << " on height: "  << block_height << " for tx: " << cryptonote::get_transaction_hash(tx));
        return false;
      }
    }

    auto &info = duplicate_info(iter->second);
    if (new_contributor)
    {
      contributor_position = info.contributors.size();
      info.contributors.emplace_back();
      info.contributors.back().address = parsed_contribution.address;
    }
    service_node_info::contributor_t& contributor = info.contributors[contributor_position];

    uint64_t can_increase_reserved_by = info.staking_requirement - info.total_reserved;
    uint64_t max_amount = contributor.reserved + can_increase_reserved_by;
    parsed_contribution.transferred = std::min(max_amount - contributor.amount, parsed_contribution.transferred);

    contributor.amount += parsed_contribution.transferred;
    info.total_contributed += parsed_contribution.transferred;

    if(contributor.amount > contributor.reserved)
    {
      info.total_reserved += contributor.amount - contributor.reserved;
      contributor.reserved = contributor.amount;
    }

    info.last_reward_block_height = block_height;
    info.last_reward_transaction_index = index;

    for (const auto &contribution : parsed_contribution.locked_contributions)
      contributor.locked_contributions.push_back(contribution);

    LOG_PRINT_L1("Contribution of " << parsed_contribution.transferred << " received for service node " << pubkey);
    if(info.is_fully_funded())
    {
      info.active_since_height = block_height;
      return true;
    }

    return false;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, cryptonote::checkpoint_t const *checkpoint)
  {
    if (block.major_version < cryptonote::network_version_16)
      return true;

    std::lock_guard lock(m_sn_mutex);
    process_block(block, txs);

    if (block.major_version >+ cryptonote::network_version_16 && checkpoint)
    {
      std::shared_ptr<const service_nodes::quorum> quorum = get_quorum(quorum_type::checkpointing, checkpoint->height);
      if (!quorum)
      {
        LOG_PRINT_L1("Failed to get quorum checkpoint for block: " << cryptonote::get_block_hash(block));
        return false;
      }

      if (!service_nodes::verify_checkpoint(block.major_version, *checkpoint, *quorum))
      {
        LOG_PRINT_L1("Service Node checkpoint failed verification for block: " << cryptonote::get_block_hash(block));
        return false;
      }
    }

    store();
    return true;
  }

  static std::vector<size_t> generate_shuffled_service_node_index_list(size_t list_size, crypto::hash const &block_hash, quorum_type type, size_t sublist_size = 0, size_t sublist_up_to = 0)
  {
    std::vector<size_t> result(list_size);
    std::iota(result.begin(), result.end(), 0);

    uint64_t seed = 0;
    std::memcpy(&seed, block_hash.data, std::min(sizeof(seed), sizeof(block_hash.data)));
    boost::endian::little_to_native_inplace(seed);

    seed += static_cast<uint64_t>(type);

    //       Shuffle 2
    //       |=================================|
    //       |                                 |
    // Shuffle 1                               |
    // |==============|                        |
    // |     |        |                        |
    // |sublist_size  |                        |
    // |     |    sublist_up_to                |
    // 0     N        Y                        Z
    // [.......................................]

    if ((0 < sublist_size && sublist_size < list_size) && (0 < sublist_up_to && sublist_up_to < list_size))
    {
      assert(sublist_size <= sublist_up_to);
      tools::shuffle_portable(result.begin(), result.begin() + sublist_up_to, seed);
      tools::shuffle_portable(result.begin() + sublist_size, result.end(), seed);
    }
    else
    {
      tools::shuffle_portable(result.begin(), result.end(), seed);
    }
    return result;
  }

  bool service_node_list::set_storage_server_peer_reachable(crypto::public_key const &pubkey, bool value)
  {
    std::lock_guard lock(m_sn_mutex);

    if (!m_state.service_nodes_infos.count(pubkey))
    {
      LOG_PRINT_L2("No Service Node is known by this pubkey: " << pubkey);
      return false;
    }

    proof_info &info = m_proofs[pubkey];
    if (info.storage_server_reachable != value)
    {
      info.storage_server_reachable = value;
      LOG_PRINT_L2("Setting reachability status for node " << pubkey << " as: " << (value ? "true" : "false"));
    }

    info.storage_server_reachable_timestamp = time(nullptr);
    return true;
  }

  static quorum_manager generate_quorums(uint8_t hard_fork_version, service_node_list::state_t const &state)
  {
    quorum_manager result = {};
    assert(state.block_hash != crypto::null_hash);

    auto active_snode_list = state.active_service_nodes_infos();
    decltype(active_snode_list) decomm_snode_list;
    decomm_snode_list = state.decommissioned_service_nodes_infos();

    quorum_type const max_quorum_type = max_quorum_type_for_hf(hard_fork_version);
    for (int type_int = 0; type_int <= (int)max_quorum_type; type_int++)
    {
      auto type = static_cast<quorum_type>(type_int);
      size_t num_validators = 0, num_workers = 0;
      auto quorum = std::make_shared<service_nodes::quorum>();
      std::vector<size_t> pub_keys_indexes;

      if (type == quorum_type::obligations)
      {
        size_t total_nodes = active_snode_list.size() + decomm_snode_list.size();
        num_validators = std::min(active_snode_list.size(), STATE_CHANGE_QUORUM_SIZE);
        pub_keys_indexes = generate_shuffled_service_node_index_list(total_nodes, state.block_hash, type, num_validators, active_snode_list.size());
        result.obligations = quorum;
        size_t num_remaining_nodes = total_nodes - num_validators;
        num_workers = std::min(num_remaining_nodes, std::max(STATE_CHANGE_MIN_NODES_TO_TEST, num_remaining_nodes / STATE_CHANGE_NTH_OF_THE_NETWORK_TO_TEST));
      }
      else if (type == quorum_type::checkpointing)
      {
        if ((state.height + REORG_SAFETY_BUFFER_IN_BLOCKS) % CHECKPOINT_INTERVAL != 0)
          continue;

        size_t total_nodes = active_snode_list.size();

        pub_keys_indexes = generate_shuffled_service_node_index_list(total_nodes, state.block_hash, type);
        result.checkpointing = quorum;
        num_validators = std::min(pub_keys_indexes.size(), CHECKPOINT_QUORUM_SIZE);
      }
      else
      {
        MERROR("Unhandled quorum type enum with value: " << type_int);
        continue;
      }

      quorum->validators.reserve(num_validators);
      quorum->workers.reserve(num_workers);

      size_t i = 0;
      for (; i < num_validators; i++)
      {
        quorum->validators.push_back(active_snode_list[pub_keys_indexes[i]].first);
      }

      for (; i < num_validators + num_workers; i++)
      {
        size_t j = pub_keys_indexes[i];
        if (j < active_snode_list.size())
          quorum->workers.push_back(active_snode_list[j].first);
        else
          quorum->workers.push_back(decomm_snode_list[j - active_snode_list.size()].first);
      }
    }

    return result;
  }

  void service_node_list::state_t::update_from_block(cryptonote::BlockchainDB const &db, cryptonote::network_type nettype, state_set const &state_history, state_set const &state_archive, std::unordered_map<crypto::hash, state_t> const &alt_states, const cryptonote::block &block, const std::vector<cryptonote::transaction> &txs, const service_node_keys *my_keys)
  {
    ++height;
    bool need_swarm_update = false;
    uint64_t block_height = cryptonote::get_block_height(block);
    assert(height == block_height);
    quorums = {};
    block_hash = cryptonote::get_block_hash(block);

    // Remove expired blacklisted key images
    for (auto entry = key_image_blacklist.begin(); entry != key_image_blacklist.end();)
    {
      if (block_height >= entry->unlock_height)
        entry = key_image_blacklist.erase(entry);
      else
        entry++;
    }

    // Expire nodes
    for (const crypto::public_key& pubkey : get_expired_nodes(db, nettype, block_height))
    {
      auto i = service_nodes_infos.find(pubkey);
      if (i != service_nodes_infos.end())
      {
        if (my_keys && my_keys->pub == pubkey)
        {
          MGINFO_GREEN("Service Node expired (yours): " << pubkey << " at block height: " << block_height);
        }
        else
        {
          LOG_PRINT_L1("Service Node expired: " << pubkey << " at block height: " << block_height);
        }

        need_swarm_update += i->second->is_active();
        service_nodes_infos.erase(i);
      }
    }

    // Advance the list to the next candidate for a reward
    {
      crypto::public_key winner_pubkey = cryptonote::get_service_node_winner_from_tx_extra(block.miner_tx.extra);
      auto it = service_nodes_infos.find(winner_pubkey);
      if (it != service_nodes_infos.end())
      {
        auto &info = duplicate_info(it->second);
        info.last_reward_block_height = block_height;
        info.last_reward_transaction_index = UINT32_MAX;
      }
    }

    // Process TX in block
    for (uint32_t index = 0; index < txs.size(); ++index)
    {
      const cryptonote::transaction& tx = txs[index];
      if (tx.is_transfer())
      {
        process_registration_tx(nettype, block, tx, index, my_keys);
        need_swarm_update += process_contribution_tx(nettype, block, tx, index);
      }
      else if (tx.tx_type == cryptonote::txtype::state_change)
      {
        need_swarm_update += process_state_change_tx(state_history, state_archive, alt_states, nettype, block, tx, my_keys);
      }
      else if (tx.tx_type == cryptonote::txtype::key_image_unlock)
      {
        process_key_image_unlock_tx(nettype, block_height, tx);
      }
    }

    if (need_swarm_update)
    {
      crypto::hash const block_hash = cryptonote::get_block_hash(block);
      uint64_t seed = 0;
      std::memcpy(&seed, block_hash.data, sizeof(seed));

      swarm_snode_map_t existing_swarms;
      for (const auto &key_info : active_service_nodes_infos())
        existing_swarms[key_info.second->swarm_id].push_back(key_info.first);

      calc_swarm_changes(existing_swarms, seed);

      for (const auto entry : existing_swarms)
      {
        const swarm_id_t swarm_id = entry.first;
        const std::vector<crypto::public_key>& snodes = entry.second;

        for (const auto snode : snodes)
        {
          auto& sn_info_ptr = service_nodes_infos.at(snode);
          if (sn_info_ptr->swarm_id == swarm_id)
            continue;
          duplicate_info(sn_info_ptr).swarm_id = swarm_id;
        }
      }
    }
    quorums = generate_quorums(block.major_version, *this);
  }

  void service_node_list::process_block(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs)
  {
    uint64_t block_height = cryptonote::get_block_height(block);
    uint8_t hard_fork_version = m_blockchain.get_hard_fork_version(block_height);

    if(hard_fork_version < 16)
      return;

    // Cull old history
    {
      uint64_t cull_height = short_term_state_cull_height(hard_fork_version, block_height);
      auto end_it = m_state_history.upper_bound(cull_height);
      for (auto it = m_state_history.begin(); it != end_it; it++)
      {
        if (m_store_quorum_history)
          m_old_quorum_states.emplace_back(it->height, it->quorums);

        uint64_t next_long_term_state = ((it->height / STORE_LONG_TERM_STATE_INTERVAL) + 1) * STORE_LONG_TERM_STATE_INTERVAL;
        uint64_t dist_to_next_long_term_state = next_long_term_state - it->height;
        bool need_quorum_for_future_states = (dist_to_next_long_term_state <= VOTE_LIFETIME + VOTE_OR_TX_VERIFY_HEIGHT_BUFFER);

        if ((it->height % STORE_LONG_TERM_STATE_INTERVAL) == 0 || need_quorum_for_future_states)
        {
          m_state_added_to_archive = true;
          if (need_quorum_for_future_states)
          {
            state_t &state = const_cast<state_t &>(*it);
            state.service_nodes_infos = {};
            state.key_image_blacklist = {};
            state.only_loaded_quorums = true;
          }
          m_state_archive.emplace_hint(m_state_archive.end(), std::move(*it));
        }
      }
      m_state_history.erase(m_state_history.begin(), end_it);

      if (m_old_quorum_states.size() > m_store_quorum_history)
        m_old_quorum_states.erase(m_old_quorum_states.begin(), m_old_quorum_states.begin() + (m_old_quorum_states.size() - m_store_quorum_history));
    }

    // Cull alt state history
    if (hard_fork_version >= cryptonote::network_version_16)
    {
      cryptonote::checkpoint_t immutable_checkpoint;
      if (m_blockchain.get_db().get_immutable_checkpoint(&immutable_checkpoint, block_height))
      {
        for (auto it = m_alt_state.begin(); it != m_alt_state.end(); )
        {
          state_t const &alt_state = it->second;
          if (alt_state.height < immutable_checkpoint.height)
            it = m_alt_state.erase(it);
          else
            it++;
        }
      }
    }

    cryptonote::network_type nettype = m_blockchain.nettype();
    m_state_history.insert(m_state_history.end(), m_state);
    m_state.update_from_block(m_blockchain.get_db(), nettype, m_state_history, m_state_archive, {} /*m_alt_state*/, block, txs, m_service_node_keys);
  }
  //----------------------------------------------------------------------------
  void service_node_list::blockchain_detached(uint64_t height)
  {
    std::lock_guard lock(m_sn_mutex);

    uint64_t revert_to_height = height - 1;
    bool reinitialise = false;
    bool using_archive = false;
    {
      auto it = m_state_history.find(revert_to_height);
      reinitialise = (it == m_state_history.end() || it->only_loaded_quorums);
      if (!reinitialise)
        m_state_history.erase(std::next(it), m_state_history.end());
    }

    if (reinitialise)
    {
      uint64_t prev_interval = revert_to_height - (revert_to_height % STORE_LONG_TERM_STATE_INTERVAL);
      auto it = m_state_archive.find(prev_interval);
      reinitialise = (it == m_state_archive.end() || it->only_loaded_quorums);
      if (!reinitialise)
      {
        m_state_history.clear();
        m_state_archive.erase(std::next(it), m_state_archive.end());
        using_archive = true;
      }
    }

    if (reinitialise)
    {
      m_state_history.clear();
      m_state_archive.clear();
      init();
      return;
    }

    auto &history = (using_archive) ? m_state_archive : m_state_history;
    auto it = std::prev(history.end());
    m_state = std::move(*it);
    history.erase(it);

    if (m_state.height != revert_to_height)
      rescan_starting_from_curr_state(false /*store_to_disk*/);
    store();
  }
  //----------------------------------------------------------------------------
  std::vector<crypto::public_key> service_node_list::state_t::get_expired_nodes(cryptonote::BlockchainDB const &db, cryptonote::network_type nettype, uint64_t block_height) const
  {
    std::vector<crypto::public_key> expired_nodes;
    uint64_t const lock_blocks = staking_num_lock_blocks(nettype);

    for(auto it = service_nodes_infos.begin(); it != service_nodes_infos.end(); it++)
    {
      crypto::public_key const &snode_key = it->first;
      const service_node_info &info = *it->second;

      if(info.requested_unlock_height != KEY_IMAGE_AWAITING_UNLOCK_HEIGHT && block_height > info.requested_unlock_height)
        expired_nodes.push_back(snode_key);
    }

    return expired_nodes;
  }
  //----------------------------------------------------------------------------
  block_winner service_node_list::state_t::get_block_winner() const
  {
    block_winner result = {};
    service_node_info const *info = nullptr;
    {
      auto oldest_waiting = std::make_tuple(std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint32_t>::max(), crypto::null_pkey);
      for (const auto &info_it : service_nodes_infos)
      {
        const auto &sninfo = *info_it.second;
        if (sninfo.is_active())
        {
          auto waiting_since = std::make_tuple(sninfo.last_reward_block_height, sninfo.last_reward_transaction_index, info_it.first);
          if (waiting_since < oldest_waiting)
          {
            oldest_waiting = waiting_since;
            info = &sninfo;
          }
        }
      }
      result.key = std::get<2>(oldest_waiting);
    }

    if (result.key == crypto::null_pkey)
    {
      result = service_nodes::null_block_winner;
      return result;
    }

    result.payouts.reserve(info->contributors.size());
    const uint64_t remaining_portions = STAKING_SHARE_PARTS - info->portions_for_operator;
    for (const auto& contributor : info->contributors)
    {
      uint64_t hi, lo, resulthi, resultlo;
      lo = mul128(contributor.amount, remaining_portions, &hi);
      div128_64(hi, lo, info->staking_requirement, &resulthi, &resultlo);

      if(contributor.address == info->operator_address)
        resultlo += info->portions_for_operator;
      result.payouts.push_back({contributor.address, resultlo});
    }
    return result;
  }

  template<typename T>
  static constexpr bool within_one(T a, T b)
  {
    return (a > b ? a - b : b - a) <= T{1};
  }
  //----------------------------------------------------------------------------
  bool service_node_list::validate_miner_tx(const crypto::hash& prev_id, const cryptonote::transaction& miner_tx, uint64_t height, uint8_t hard_fork_version, cryptonote::block_reward_parts const &reward_parts) const
  {
    std::lock_guard lock(m_sn_mutex);
    if(hard_fork_version < 16)
      return true;

    uint64_t base_reward = reward_parts.original_base_reward;
    uint64_t total_service_node_reward = cryptonote::service_node_reward_formula(base_reward, hard_fork_version);

    block_winner winner = m_state.get_block_winner();
    crypto::public_key check_winner_pubkey = cryptonote::get_service_node_winner_from_tx_extra(miner_tx.extra);
    if(winner.key != check_winner_pubkey)
    {
      MERROR("Service Node reward winner is incorrect! Expected: " << winner.key << ", block has: " << check_winner_pubkey);
      return false;
    }

    if ((miner_tx.vout.size() - 3) < winner.payouts.size())
    {
      MERROR("Service Node reward has more winners than outputs: " << (miner_tx.vout.size() - 3) << ", winners: " << winner.payouts.size());
      return false;
    }

    for(size_t i = 0; i < winner.payouts.size(); i++)
    {
      size_t vout_index = i + 1;
      payout_entry const &payout = winner.payouts[i];
      uint64_t reward = cryptonote::get_portion_of_reward(payout.portions, total_service_node_reward);

      if(!within_one(miner_tx.vout[vout_index].amount, reward))
      {
        MERROR("Service Node reward amount incorrect. Should be: " << cryptonote::print_money(reward) << ", while is: " << cryptonote::print_money(miner_tx.vout[vout_index].amount));
        return false;
      }

      if(miner_tx.vout[vout_index].target.type() != typeid(cryptonote::txout_to_key))
      {
        MERROR("Service Node output target should be txout_to_key");
        return false;
      }

      crypto::key_derivation derivation = {};
      crypto::public_key out_eph_public_key = {};
      cryptonote::keypair gov_key = cryptonote::get_deterministic_keypair_from_height(height);

      bool r = crypto::generate_key_derivation(payout.address.m_view_public_key, gov_key.sec, derivation);
      CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to generate_key_derivation(" << payout.address.m_view_public_key << ", " << crypto::secret_key_explicit_print_ref{gov_key.sec} << ")");
      r = crypto::derive_public_key(derivation, vout_index, payout.address.m_spend_public_key, out_eph_public_key);
      CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to derive_public_key(" << derivation << ", " << vout_index << ", "<< payout.address.m_spend_public_key << ")");

      if(boost::get<cryptonote::txout_to_key>(miner_tx.vout[vout_index].target).key != out_eph_public_key)
      {
        MERROR("Invalid service node reward output");
        return false;
      }
    }

    return true;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::alt_block_added(cryptonote::block const &block, std::vector<cryptonote::transaction> const &txs, cryptonote::checkpoint_t const *checkpoint)
  {
    if (block.major_version < cryptonote::network_version_16)
      return true;

    uint64_t block_height = cryptonote::get_block_height(block);
    state_t const *starting_state = nullptr;
    crypto::hash const block_hash = get_block_hash(block);

    auto it = m_alt_state.find(block_hash);
    if (it != m_alt_state.end())
      return true;

    if (!starting_state)
    {
      auto it = m_state_history.find(block_height - 1);
      if (it != m_state_history.end())
        if (block.prev_id == it->block_hash)
          starting_state = &(*it);
    }

    if (!starting_state)
    {
      auto it = m_alt_state.find(block.prev_id);
      if (it != m_alt_state.end())
        starting_state = &it->second;
    }

    if (!starting_state)
    {
      LOG_PRINT_L1("Received alt block but couldn't find parent state in historical state");
      return false;
    }

    if (starting_state->block_hash != block.prev_id)
    {
      LOG_PRINT_L1("Unexpected state_t's hash: " << starting_state->block_hash << ", does not match the block prev hash: " << block.prev_id);
      return false;
    }

    state_t alt_state = *starting_state;
    alt_state.update_from_block(m_blockchain.get_db(), m_blockchain.nettype(), m_state_history, m_state_archive, m_alt_state, block, txs, m_service_node_keys);
    auto alt_it = m_alt_state.find(block_hash);
    if (alt_it != m_alt_state.end())
      alt_it->second = std::move(alt_state);
    else
      m_alt_state.emplace(block_hash, std::move(alt_state));

    if (checkpoint)
    {
      std::vector<std::shared_ptr<const service_nodes::quorum>> alt_quorums;
      std::shared_ptr<const service_nodes::quorum> quorum = get_quorum(quorum_type::checkpointing, checkpoint->height, false, &alt_quorums);
      if (!quorum)
        return false;

      if (!service_nodes::verify_checkpoint(block.major_version, *checkpoint, *quorum))
      {
        bool verified_on_alt_quorum = false;
        for (std::shared_ptr<const service_nodes::quorum> alt_quorum : alt_quorums)
        {
          if (service_nodes::verify_checkpoint(block.major_version, *checkpoint, *alt_quorum))
          {
            verified_on_alt_quorum = true;
            break;
          }
        }

        if (!verified_on_alt_quorum)
            return false;
      }
    }

    return true;
  }
  //----------------------------------------------------------------------------
  static service_node_list::quorum_for_serialization serialize_quorum_state(uint8_t hard_fork_version, uint64_t height, quorum_manager const &quorums)
  {
    service_node_list::quorum_for_serialization result = {};
    result.height = height;
    if (quorums.obligations)
      result.quorums[static_cast<uint8_t>(quorum_type::obligations)] = *quorums.obligations;
    if (quorums.checkpointing)
      result.quorums[static_cast<uint8_t>(quorum_type::checkpointing)] = *quorums.checkpointing;
    return result;
  }
  //----------------------------------------------------------------------------
  static service_node_list::state_serialized serialize_service_node_state_object(uint8_t hard_fork_version, service_node_list::state_t const &state, bool only_serialize_quorums = false)
  {
    service_node_list::state_serialized result = {};
    result.version = service_node_list::state_serialized::get_version(hard_fork_version);
    result.height = state.height;
    result.quorums = serialize_quorum_state(hard_fork_version, state.height, state.quorums);
    result.only_stored_quorums = state.only_loaded_quorums || only_serialize_quorums;

    if (only_serialize_quorums)
      return result;

    result.infos.reserve(state.service_nodes_infos.size());
    for (const auto &kv_pair : state.service_nodes_infos)
      result.infos.emplace_back(kv_pair);

    result.key_image_blacklist = state.key_image_blacklist;
    result.block_hash = state.block_hash;
    return result;
  }
  //----------------------------------------------------------------------------
  bool service_node_list::store()
  {
    if(!m_blockchain.has_db())
      return false;

    uint8_t hard_fork_version = m_blockchain.get_current_hard_fork_version();
    if(hard_fork_version < 16)
      return true;

    data_for_serialization *data[] = {&m_cache_long_term_data, &m_cache_short_term_data};
    auto const serialize_version = data_for_serialization::get_version(hard_fork_version);
    std::lock_guard lock(m_sn_mutex);

    for (data_for_serialization *serialize_entry : data)
    {
      if (serialize_entry->version != serialize_version)
        m_state_added_to_archive = true;
      serialize_entry->version = serialize_version;
      serialize_entry->clear();
    }

    m_cache_short_term_data.quorum_states.reserve(m_old_quorum_states.size());
    for (const quorums_by_height &entry : m_old_quorum_states)
      m_cache_short_term_data.quorum_states.push_back(serialize_quorum_state(hard_fork_version, entry.height, entry.quorums));

    if (m_state_added_to_archive)
    {
      for (auto const &it : m_state_archive)
        m_cache_long_term_data.states.push_back(serialize_service_node_state_object(hard_fork_version, it));
    }

    uint64_t const max_short_term_height = short_term_state_cull_height(hard_fork_version, (m_state.height - 1)) + VOTE_LIFETIME + VOTE_OR_TX_VERIFY_HEIGHT_BUFFER;
    for (auto it = m_state_history.begin(); it != m_state_history.end() && it->height <= max_short_term_height; it++)
    {
      m_cache_short_term_data.states.push_back(serialize_service_node_state_object(hard_fork_version, *it, it->height < max_short_term_height));
    }

    m_cache_data_blob.clear();
    if (m_state_added_to_archive)
    {
      std::stringstream ss;
      binary_archive<true> ba(ss);
      bool r = ::serialization::serialize(ba, m_cache_long_term_data);
      CHECK_AND_ASSERT_MES(r, false, "Failed to store service node info: failed to serialize long term data");
      m_cache_data_blob.append(ss.str());
      {
        auto &db = m_blockchain.get_db();
        cryptonote::db_wtxn_guard txn_guard{db};
        db.set_service_node_data(m_cache_data_blob, true);
      }
    }

    m_cache_data_blob.clear();
    {
      std::stringstream ss;
      binary_archive<true> ba(ss);
      bool r = ::serialization::serialize(ba, m_cache_short_term_data);
      CHECK_AND_ASSERT_MES(r, false, "Failed to store service node info: failed to serialize short term data");
      m_cache_data_blob.append(ss.str());
      {
        auto &db = m_blockchain.get_db();
        cryptonote::db_wtxn_guard txn_guard{db};
        db.set_service_node_data(m_cache_data_blob, false);
      }
    }

    m_state_added_to_archive = false;
    return true;
  }
  //----------------------------------------------------------------------------
  static crypto::hash hash_uptime_proof(const cryptonote::NOTIFY_UPTIME_PROOF::request &proof, uint8_t hard_fork_version)
  {
    auto buf = tools::memcpy_le(proof.pubkey.data, proof.timestamp, proof.public_ip, proof.storage_port, proof.pubkey_ed25519.data, proof.arqnet_port);
    size_t buf_size = buf.size();
    if (hard_fork_version < cryptonote::network_version_17)
      buf_size -= sizeof(proof.arqnet_port);

    crypto::hash result;
    crypto::cn_fast_hash(buf.data(), buf_size, result);
    return result;
  }
  //----------------------------------------------------------------------------
  cryptonote::NOTIFY_UPTIME_PROOF::request service_node_list::generate_uptime_proof(const service_node_keys &keys, uint32_t public_ip, uint16_t storage_port, uint16_t arqnet_port) const
  {
    cryptonote::NOTIFY_UPTIME_PROOF::request result = {};
    result.arqma_snode_version                      = ARQMA_VERSION;
    result.timestamp                                = time(nullptr);
    result.pubkey                                   = keys.pub;
    result.public_ip                                = public_ip;
    result.storage_port                             = storage_port;
    result.arqnet_port                              = arqnet_port;
    result.pubkey_ed25519                           = keys.pub_ed25519;

    crypto::hash hash = hash_uptime_proof(result, m_blockchain.get_current_hard_fork_version());
    crypto::generate_signature(hash, keys.pub, keys.key, result.sig);
    crypto_sign_detached(result.sig_ed25519.data, NULL, reinterpret_cast<unsigned char *>(hash.data), sizeof(hash.data), keys.key_ed25519.data);
    return result;
  }
  //----------------------------------------------------------------------------

#ifdef __cpp_lib_erase_if
  using std::erase_if;
#else
  template <typename Container, typename Predicate>
  static void erase_if(Container &c, Predicate pred)
  {
    for (auto it = c.begin(), last = c.end(); it != last; )
    {
      if (pred(*it))
        it = c.erase(it);
      else
        ++it;
    }
  }
#endif

  template <typename T>
  static bool update_val(T &val, const T &to)
  {
    if (val != to)
    {
      val = to;
      return true;
    }
    return false;
  }

  void proof_info::store(const crypto::public_key &pubkey, cryptonote::Blockchain &blockchain)
  {
    std::unique_lock lock{blockchain};
    auto &db = blockchain.get_db();
    db.set_service_node_proof(pubkey, *this);
  }

  bool proof_info::update(uint64_t ts, uint32_t ip, uint16_t s_port, uint16_t a_port, std::array<uint16_t, 3> ver, const crypto::ed25519_public_key &pk_ed, const crypto::x25519_public_key &pk_x2)
  {
    bool update_db = false;
    update_db |= update_val(timestamp, ts);
    update_db |= update_val(public_ip, ip);
    update_db |= update_val(storage_port, s_port);
    update_db |= update_val(arqnet_port, a_port);
    update_db |= update_val(version, ver);
    update_db |= update_val(pubkey_ed25519, pk_ed);
    effective_timestamp = timestamp;
    pubkey_x25519 = pk_x2;

    auto now = std::time(nullptr);
    if (public_ips[0].first && public_ips[0].first == public_ip)
      public_ips[0].second = now;
    else if (public_ips[1].first && public_ips[1].first == public_ip)
      public_ips[1].second = now;
    else if (public_ips[0].second > public_ips[1].second)
      public_ips[1] = {public_ip, now};
    else
      public_ips[0] = {public_ip, now};

  return update_db;
};

void proof_info::update_pubkey(const crypto::ed25519_public_key &pk)
{
  if (pk == pubkey_ed25519)
    return;
  if (pk && 0 == crypto_sign_ed25519_pk_to_curve25519(pubkey_x25519.data, pk.data))
  {
    pubkey_ed25519 = pk;
  }
  else
  {
    pubkey_x25519 = crypto::x25519_public_key::null();
    pubkey_ed25519 = crypto::ed25519_public_key::null();
  }
}


#define REJECT_PROOF(log) do { LOG_PRINT_L2("Rejecting uptime proof from " << proof.pubkey << ": " log); return false; } while (0)

  bool service_node_list::handle_uptime_proof(cryptonote::NOTIFY_UPTIME_PROOF::request const &proof, bool &my_uptime_proof_confirmation)
  {
    uint8_t const hard_fork_version = m_blockchain.get_current_hard_fork_version();
    uint64_t const now = time(nullptr);

    if ((proof.timestamp < now - UPTIME_PROOF_BUFFER_IN_SECONDS) || (proof.timestamp > now + UPTIME_PROOF_BUFFER_IN_SECONDS))
      REJECT_PROOF("timestamp is too far from now");

    for (auto const &min : MIN_UPTIME_PROOF_VERSIONS)
      if (hard_fork_version >= min.hardfork && proof.arqma_snode_version < min.version)
        REJECT_PROOF("v" << min.version[0] << "." << min.version[1] << "." << min.version[2] << "+ arqma version is required for v" << std::to_string(hard_fork_version) << "+ network proofs");

    if (!epee::net_utils::is_ip_public(proof.public_ip))
      REJECT_PROOF("public_ip is not public");

    crypto::hash hash = hash_uptime_proof(proof, hard_fork_version);

    if (!crypto::check_signature(hash, proof.pubkey, proof.sig))
      REJECT_PROOF("signature validation failed");

    crypto::x25519_public_key derived_x25519_pubkey = crypto::x25519_public_key::null();

    if (!epee::net_utils::is_ip_public(proof.public_ip)) return false;

    if (!proof.pubkey_ed25519)
      REJECT_PROOF("required ed25519 auxiliary pubkey " << epee::string_tools::pod_to_hex(proof.pubkey_ed25519) << " not included in proof");

    if (0 != crypto_sign_verify_detached(proof.sig_ed25519.data, reinterpret_cast<unsigned char *>(hash.data), sizeof(hash.data), proof.pubkey_ed25519.data))
      REJECT_PROOF("ed25519 signature validation failed");

    if (0 != crypto_sign_ed25519_pk_to_curve25519(derived_x25519_pubkey.data, proof.pubkey_ed25519.data) || !derived_x25519_pubkey)
      REJECT_PROOF("invalid ed25519 pubkey included in proof (x25519 derivation failed)");

    if (hard_fork_version >= cryptonote::network_version_17)
    {
      if (proof.arqnet_port == 0)
        REJECT_PROOF("invalid arqnet port in uptime proof");
    }

    auto locks = tools::unique_locks(m_blockchain, m_sn_mutex, m_x25519_map_mutex);
    auto it = m_state.service_nodes_infos.find(proof.pubkey);
    if (it == m_state.service_nodes_infos.end())
      REJECT_PROOF("no such service node is currently registered");

    auto &iproof = m_proofs[proof.pubkey];

    if (iproof.timestamp >= now - (UPTIME_PROOF_FREQUENCY_IN_SECONDS / 2))
      REJECT_PROOF("already received one uptime proof for this node recently");

    if (m_service_node_keys && proof.pubkey == m_service_node_keys->pub)
    {
      my_uptime_proof_confirmation = true;
      MGINFO("Received uptime-proof confirmation back from network for Service Node (yours); " << proof.pubkey);
    }
    else
    {
      my_uptime_proof_confirmation = false;
      LOG_PRINT_L2("Accepted uptime proof from " << proof.pubkey);
    }

    auto old_x25519 = iproof.pubkey_x25519;
    if (iproof.update(now, proof.public_ip, proof.storage_port, proof.arqnet_port, proof.arqma_snode_version, proof.pubkey_ed25519, derived_x25519_pubkey))
      iproof.store(proof.pubkey, m_blockchain);

    if ((uint64_t) m_x25519_map_last_pruned + X25519_MAP_PRUNING_INTERVAL <= now)
    {
      time_t cutoff = now - X25519_MAP_PRUNING_LAG;;
      erase_if(m_x25519_to_pub, [&cutoff](const decltype(m_x25519_to_pub)::value_type &x) { return x.second.second < cutoff; });
      m_x25519_map_last_pruned = now;
    }

    if (old_x25519 && old_x25519 != derived_x25519_pubkey)
      m_x25519_to_pub.erase(old_x25519);

    if (derived_x25519_pubkey)
      m_x25519_to_pub[derived_x25519_pubkey] = {proof.pubkey, now};

    return true;
  }
  //---------------------------------------------------------------------------
  void service_node_list::cleanup_proofs()
  {
    MDEBUG("Cleaning up expired SN proofs");
    auto locks = tools::unique_locks(m_sn_mutex, m_blockchain);
    uint64_t now = std::time(nullptr);
    auto &db = m_blockchain.get_db();
    cryptonote::db_wtxn_guard guard{db};
    for (auto it = m_proofs.begin(); it != m_proofs.end(); )
    {
      auto& pubkey = it->first;
      auto& proof = it->second;

      if (!m_state.service_nodes_infos.count(pubkey) && proof.timestamp + 21600 < now)
      {
        db.remove_service_node_proof(pubkey);
        it = m_proofs.erase(it);
      }
      else
        ++it;
    }
  }
  //---------------------------------------------------------------------------
  crypto::public_key service_node_list::get_pubkey_from_x25519(const crypto::x25519_public_key &x25519) const
  {
    std::shared_lock lock{m_x25519_map_mutex};
    auto it = m_x25519_to_pub.find(x25519);
    if (it != m_x25519_to_pub.end())
      return it->second.first;
    return crypto::null_pkey;
  }
  //---------------------------------------------------------------------------
  void service_node_list::initialize_x25519_map()
  {
    auto locks = tools::unique_locks(m_sn_mutex, m_x25519_map_mutex);

    auto now = std::time(nullptr);
    for (const auto &pk_info : m_state.service_nodes_infos)
    {
      auto it = m_proofs.find(pk_info.first);
      if (it == m_proofs.end())
        continue;
      if (const auto &x2_pk = it->second.pubkey_x25519)
        m_x25519_to_pub.emplace(x2_pk, std::make_pair(pk_info.first, now));
    }
  }
  //---------------------------------------------------------------------------
  void service_node_list::record_checkpoint_vote(crypto::public_key const &pubkey, uint64_t height, bool voted)
  {
    std::lock_guard lock(m_sn_mutex);
    if (!m_state.service_nodes_infos.count(pubkey))
      return;

    auto &info = m_proofs[pubkey];
    info.votes[info.vote_index].height = height;
    info.votes[info.vote_index].voted = voted;
    info.vote_index = (info.vote_index + 1) % info.votes.size();
  }
  //----------------------------------------------------------------------------
  static quorum_manager quorum_for_serialization_to_quorum_manager(service_node_list::quorum_for_serialization const &source)
  {
    quorum_manager result = {};
    result.obligations = std::make_shared<quorum>(source.quorums[static_cast<uint8_t>(quorum_type::obligations)]);

    if ((source.height + REORG_SAFETY_BUFFER_IN_BLOCKS) % CHECKPOINT_INTERVAL == 0)
      result.checkpointing = std::make_shared<quorum>(source.quorums[static_cast<uint8_t>(quorum_type::checkpointing)]);

    return result;
  }
  //----------------------------------------------------------------------------
  service_node_list::state_t::state_t(service_node_list* snl, state_serialized &&state)
  : height{state.height}
  , key_image_blacklist{std::move(state.key_image_blacklist)}
  , only_loaded_quorums{state.only_stored_quorums}
  , block_hash{state.block_hash}
  , sn_list{snl}
  {
    if (!sn_list)
      throw std::logic_error("Cannot deserialize a state_t without a service_node_list");
    if (state.version == state_serialized::version_t::version_0)
      block_hash = sn_list->m_blockchain.get_block_id_by_height(height);

    for (auto &pubkey_info : state.infos)
    {
      using version_t = service_node_info::version_t;
      auto &info = const_cast<service_node_info &>(*pubkey_info.info);

      if (info.version < version_t::v1)
      {
        info.version = version_t::v1;
      }
      assert(info.version == tools::enum_top<decltype(info.version)>);

      service_nodes_infos.emplace(std::move(pubkey_info.pubkey), std::move(pubkey_info.info));
    }
    quorums = quorum_for_serialization_to_quorum_manager(state.quorums);
  }
  //----------------------------------------------------------------------------
  bool service_node_list::load(const uint64_t current_height)
  {
    LOG_PRINT_L1("service_node_list::load()");
    reset(false);
    if(!m_blockchain.has_db())
    {
      return false;
    }

    uint64_t bytes_loaded = 0;
    auto &db = m_blockchain.get_db();
    cryptonote::db_rtxn_guard txn_guard{db};
    std::string blob;
    if(db.get_service_node_data(blob, true))
    {
      bytes_loaded += blob.size();
      std::stringstream ss;
      blob.clear();
      ss << blob;
      binary_archive<false> ba(ss);

      data_for_serialization data_in = {};
      if (::serialization::serialize(ba, data_in) && data_in.states.size())
      {
        if (data_in.states[0].version == state_serialized::version_t::version_0)
        {
          size_t const last_index = data_in.states.size() - 1;
          if ((data_in.states.back().height % STORE_LONG_TERM_STATE_INTERVAL) != 0)
          {
            LOG_PRINT_L0("Last serialized quorum height: " << data_in.states.back().height << " in archive is unexpectedly not a multiple of: "
              << STORE_LONG_TERM_STATE_INTERVAL << ", regenerating state");
            return false;
          }

          for (size_t i = data_in.states.size() - 1; i >= 1; i--)
          {
            state_serialized &serialized_entry      = data_in.states[i];
            state_serialized &prev_serialized_entry = data_in.states[i - 1];

            if ((prev_serialized_entry.height % STORE_LONG_TERM_STATE_INTERVAL) == 0)
            {
              continue;
            }

            state_t entry{this, std::move(serialized_entry)};
            entry.height--;
            entry.quorums = quorum_for_serialization_to_quorum_manager(prev_serialized_entry.quorums);

            if ((serialized_entry.height % STORE_LONG_TERM_STATE_INTERVAL) == 0)
            {
              state_t long_term_state                  = entry;
              cryptonote::block const &block           = db.get_block_from_height(long_term_state.height + 1);
              std::vector<cryptonote::transaction> txs = db.get_tx_list(block.tx_hashes);
              long_term_state.update_from_block(db, m_blockchain.nettype(), {} /*state_history*/, {}, {} /*alt_states*/, block, txs, nullptr /*my_pubkey*/);

              entry.service_nodes_infos                = {};
              entry.key_image_blacklist                = {};
              entry.only_loaded_quorums                = true;
              m_state_archive.emplace_hint(m_state_archive.begin(), std::move(long_term_state));
            }
            m_state_archive.emplace_hint(m_state_archive.begin(), std::move(entry));
          }
        }
        else
        {
          for (state_serialized &entry : data_in.states)
            m_state_archive.emplace_hint(m_state_archive.end(), this, std::move(entry));
        }
      }
    }

    if (!db.get_service_node_data(blob, false))
      return false;

    bytes_loaded += blob.size();
    std::stringstream ss;
    ss << blob;
    binary_archive<false> ba(ss);

    data_for_serialization data_in = {};
    bool deserialized = ::serialization::serialize(ba, data_in);
    CHECK_AND_ASSERT_MES(deserialized, false, "Failed to parse service node data from blob");

    if (data_in.states.empty())
      return false;

    {
      const uint64_t hist_state_from_height = current_height - m_store_quorum_history;
      uint64_t last_loaded_height = 0;
      for (auto &states : data_in.quorum_states)
      {
        if (states.height < hist_state_from_height)
          continue;

        quorums_by_height entry = {};
        entry.height            = states.height;
        entry.quorums           = quorum_for_serialization_to_quorum_manager(states);

        if (states.height <= last_loaded_height)
        {
          LOG_PRINT_L0("Serialized quorums is not stored in ascending order by height in DB, failed to load from DB");
          return false;
        }
        last_loaded_height = states.height;
        m_old_quorum_states.push_back(entry);
      }
    }

    {
      assert(data_in.states.size() > 0);
      size_t const last_index = data_in.states.size() - 1;
      if (data_in.states[last_index].only_stored_quorums)
      {
        LOG_PRINT_L0("Unexpected last serialized state only has quorums loaded");
        return false;
      }

      if (data_in.states[0].version == state_serialized::version_t::version_0)
      {
        for (size_t i = last_index; i >= 1; i--)
        {
          state_serialized &serialized_entry      = data_in.states[i];
          state_serialized &prev_serialized_entry = data_in.states[i - 1];
          state_t entry{this, std::move(serialized_entry)};
          entry.quorums = quorum_for_serialization_to_quorum_manager(prev_serialized_entry.quorums);
          entry.height--;
          if (i == last_index) m_state = std::move(entry);
          else                 m_state_archive.emplace_hint(m_state_archive.end(), std::move(entry));
        }
      }
      else
      {
        size_t const last_index  = data_in.states.size() - 1;
        for (size_t i = 0; i < last_index; i++)
        {
          state_serialized &entry = data_in.states[i];
          if (entry.block_hash == crypto::null_hash) entry.block_hash = m_blockchain.get_block_id_by_height(entry.height);
          m_state_history.emplace_hint(m_state_history.end(), this, std::move(entry));
        }

        m_state = {this, std::move(data_in.states[last_index])};
      }
    }

    m_proofs = db.get_all_service_node_proofs();
    if (m_service_node_keys)
    {
      auto &mine = m_proofs[m_service_node_keys->pub];
      mine.timestamp = mine.effective_timestamp = 0;
    }

    initialize_x25519_map();

    MGINFO("Service node data loaded successfully, height: " << m_state.height);
    MGINFO(m_state.service_nodes_infos.size() << " nodes and "
           << m_state_history.size() << " recent states loaded, " << m_state_archive.size()
           << " historical states loaded, (" << tools::get_human_readable_bytes(bytes_loaded) << ")");

    LOG_PRINT_L1("service_node_list::load() returning success");
    return true;
  }
  //----------------------------------------------------------------------------
  void service_node_list::reset(bool delete_db_entry)
  {
    m_state_history.clear();
    m_old_quorum_states.clear();
    m_state = state_t{this};

    if(m_blockchain.has_db() && delete_db_entry)
    {
      cryptonote::db_wtxn_guard txn_guard(m_blockchain.get_db());
      m_blockchain.get_db().clear_service_node_data();
    }

    uint64_t hardfork_16_from_height = 0;
    {
      uint32_t window, votes, threshold;
      uint8_t voting;
      m_blockchain.get_hard_fork_voting_info(16, window, votes, threshold, hardfork_16_from_height, voting);
    }
    m_state.height = hardfork_16_from_height - 1;
  }
  //---------------------------------------------------------------------------
  size_t service_node_info::total_num_locked_contributions() const
  {
    size_t result = 0;
    for(service_node_info::contributor_t const &contributor : this->contributors)
      result += contributor.locked_contributions.size();
    return result;
  }
  //----------------------------------------------------------------------------
  converted_registration_args convert_registration_args(cryptonote::network_type nettype, const std::vector<std::string>& args, uint64_t staking_requirement)
  {
    converted_registration_args result = {};
    if(args.size() % 2 == 0 || args.size() < 3)
    {
      result.err_msg = tr("Usage: <operator cut> <address> <contribution %> [<address> <contribution %> [...]]");
      return result;
    }
    if((args.size() - 1) / 2 > MAX_NUMBER_OF_CONTRIBUTORS)
    {
      result.err_msg = tr("Exceeds the maximum number of contributors, which is: ") + std::to_string(MAX_NUMBER_OF_CONTRIBUTORS);
      return result;
    }

    try
    {
      result.portions_for_operator = boost::lexical_cast<uint64_t>(args[0]);
      if(result.portions_for_operator > STAKING_SHARE_PARTS)
      {
        result.err_msg = tr("Invalid contribution % amount: ") + args[0] + tr(". Must be between 0 (0%) and ") + std::to_string(STAKING_SHARE_PARTS);
        return result;
      }
    }
    catch(const std::exception &e)
    {
      result.err_msg = tr("Invalid contribution % amount: ") + args[0] + tr(". Must be between 0 (0%) and ") + std::to_string(STAKING_SHARE_PARTS);
      return result;
    }

    struct addr_to_portion_t
    {
      cryptonote::address_parse_info info;
      uint64_t portions;
    };

    std::vector<addr_to_portion_t> addr_to_portions;
    size_t const OPERATOR_ARG_INDEX = 1;
    for(size_t i = OPERATOR_ARG_INDEX, num_contributions = 0; i < args.size(); i += 2, ++num_contributions)
    {
      cryptonote::address_parse_info info;
      if(!cryptonote::get_account_address_from_str(info, nettype, args[i]))
      {
        result.err_msg = tr("Failed to parse address: ") + args[i];
        return result;
      }

      if(info.has_payment_id)
      {
        result.err_msg = tr("Can't use a payment id for staking tx");
        return result;
      }

      if(info.is_subaddress)
      {
        result.err_msg = tr("Can't use subaddress for staking tx");
        return result;
      }

      try
      {
        uint64_t num_portions = boost::lexical_cast<uint64_t>(args[i+1]);
        addr_to_portions.push_back({info, num_portions});
      }
      catch (const std::exception &e)
      {
        result.err_msg = tr("Invalid amount for contributor: ") + args[i] + tr(", with contribution amount that could not be converted to a number: ") + args[i+1];
        return result;
      }
    }

    std::array<uint64_t, MAX_NUMBER_OF_CONTRIBUTORS> excess_portions;
    std::array<uint64_t, MAX_NUMBER_OF_CONTRIBUTORS> min_contributions;
    {
      uint64_t arqma_reserved = 0;
      for(size_t index = 0; index < addr_to_portions.size(); ++index)
      {
        addr_to_portion_t const &addr_to_portion = addr_to_portions[index];
        uint64_t min_contribution_portions = service_nodes::get_min_node_contribution_in_portions(staking_requirement, arqma_reserved, index);
        uint64_t arqma_amount = service_nodes::portions_to_amount(staking_requirement, addr_to_portion.portions);
        arqma_reserved += arqma_amount;

        uint64_t excess = 0;
        if(addr_to_portion.portions > min_contribution_portions)
          excess = addr_to_portion.portions - min_contribution_portions;

        min_contributions[index] = min_contribution_portions;
        excess_portions[index] = excess;
      }
    }

    uint64_t portions_left = STAKING_SHARE_PARTS;
    uint64_t total_reserved = 0;
    for(size_t i = 0; i < addr_to_portions.size(); ++i)
    {
      addr_to_portion_t &addr_to_portion = addr_to_portions[i];
      uint64_t min_portions = get_min_node_contribution_in_portions(staking_requirement, total_reserved, i);

      uint64_t portions_to_steal = 0;
      if(addr_to_portion.portions < min_portions)
      {
        uint64_t needed = min_portions - addr_to_portion.portions;
        const uint64_t FUDGE_FACTOR = 10;
        const uint64_t DUST_UNIT = (STAKING_SHARE_PARTS / staking_requirement);
        const uint64_t DUST = DUST_UNIT * FUDGE_FACTOR;
        if(needed > DUST)
          continue;

        for(size_t sub_index = 0; sub_index < addr_to_portions.size(); sub_index++)
        {
          if(i == sub_index)
            continue;
          uint64_t &contributor_excess = excess_portions[sub_index];
          if(contributor_excess > 0)
          {
            portions_to_steal = std::min(needed, contributor_excess);
            addr_to_portion.portions += portions_to_steal;
            contributor_excess -= portions_to_steal;
            needed -= portions_to_steal;
            result.portions[sub_index] -= portions_to_steal;

            if(needed == 0)
              break;
          }
        }

        if(needed > 0 && addr_to_portions.size() < MAX_NUMBER_OF_CONTRIBUTORS)
          addr_to_portion.portions += needed;
      }

      if(addr_to_portion.portions < min_portions || addr_to_portion.portions > portions_left)
      {
        result.err_msg = tr("Invalid contributor amount: ") + args[i] + tr(", with portion amount: ") + args[i+1] + tr(". \n") +
                         tr("Each contributor must have at least 25% required stake to last contributor be allowed less contributions\n") +
                         tr("than required 25% minimum.");
        return result;
      }

      if(min_portions == UINT64_MAX)
      {
        result.err_msg = tr("Too many contributors specified. You can only split between ") + std::to_string(MAX_NUMBER_OF_CONTRIBUTORS) + tr(" contributors.");
        return result;
      }

      portions_left -= addr_to_portion.portions;
      portions_left += portions_to_steal;
      result.addresses.push_back(addr_to_portion.info.address);
      result.portions.push_back(addr_to_portion.portions);
      uint64_t arqma_amount = service_nodes::portions_to_amount(addr_to_portion.portions, staking_requirement);
      total_reserved += arqma_amount;
    }

    result.success = true;
    return result;
  }
  //----------------------------------------------------------------------------
  bool make_registration_cmd(cryptonote::network_type nettype, uint64_t staking_requirement, const std::vector<std::string> &args, const service_node_keys &keys, std::string &cmd, bool make_friendly, boost::optional<std::string&> err_msg)
  {
    converted_registration_args converted_args = convert_registration_args(nettype, args, staking_requirement);
    if(!converted_args.success)
    {
      MERROR(tr("Could not convert registration args, reason: ") << converted_args.err_msg);
      return false;
    }

    uint64_t exp_timestamp = time(nullptr) + STAKING_AUTHORIZATION_EXPIRATION_WINDOW;

    crypto::hash hash;
    bool hashed = cryptonote::get_registration_hash(converted_args.addresses, converted_args.portions_for_operator, converted_args.portions, exp_timestamp, hash);
    if(!hashed)
    {
      MERROR(tr("Could not make registration hash from addresses and contribution %"));
      return false;
    }

    crypto::signature signature;
    crypto::generate_signature(hash, keys.pub, keys.key, signature);

    std::stringstream stream;
    if(make_friendly)
    {
      stream << tr("Run this command in the wallet that will fund this registration:\n\n");
    }

    stream << "register_service_node";
    for(size_t i = 0; i < args.size(); ++i)
    {
      stream << " " << args[i];
    }

    stream << " " << exp_timestamp << " ";
    stream << epee::string_tools::pod_to_hex(keys.pub) << " ";
    stream << epee::string_tools::pod_to_hex(signature);

    if(make_friendly)
    {
      stream << "\n\n";
      time_t tt = exp_timestamp;

      struct tm tm;
      epee::misc_utils::get_gmt_time(tt, tm);

      char buffer[128];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M:%S %p UTC", &tm);
      stream << tr("This registration expires at ") << buffer << tr(".\n");
      stream << tr("This should be in about 2 weeks.\n");
      stream << tr("If it isn't, check this computer's clock.\n") << std::endl;
      stream << tr("Please submit your registration into the blockchain before this time or it will be invalid.");
    }

    cmd = stream.str();
    return true;
  }

  bool service_node_info::can_be_voted_on(uint64_t height) const
  {
    if (!this->is_fully_funded() || this->registration_height >= height) return false;
    if (!this->is_decommissioned() && this->last_decommission_height >= height) return false;

    if (this->is_active())
    {
      assert(this->active_since_height >= 0);
      if (static_cast<uint64_t>(this->active_since_height) >= height) return false;
    }

    return true;
  }

  bool service_node_info::can_transition_to_state(uint8_t hard_fork_version, uint64_t height, new_state proposed_state) const
  {
    if (hard_fork_version >= cryptonote::network_version_16)
    {
      if (!can_be_voted_on(height))
      return false;

      if (proposed_state == new_state::deregister)
      {
        if (height <= this->registration_height)
          return false;
      }
      else if (proposed_state == new_state::ip_change_penalty)
      {
        if (height <= this->last_ip_change_height)
          return false;
      }

      if (this->is_decommissioned())
      {
        return proposed_state != new_state::decommission && proposed_state != new_state::ip_change_penalty;
      }

      return (proposed_state != new_state::recommission);
    }
    else
    {
      if (proposed_state == new_state::deregister)
      {
        if (height < this->registration_height) return false;
      }

      if (this->is_decommissioned())
      {
        return proposed_state != new_state::decommission && proposed_state != new_state::ip_change_penalty;
      }
      else
      {
        return (proposed_state != new_state::recommission);
      }
    }
  }
}
