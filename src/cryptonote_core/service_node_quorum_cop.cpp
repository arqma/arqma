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

#include "service_node_quorum_cop.h"
#include "service_node_voting.h"
#include "service_node_list.h"
#include "cryptonote_config.h"
#include "cryptonote_core.h"
#include "version.h"
#include "common/arqma.h"
#include "net/local_ip.h"
#include "boost/endian/conversion.hpp"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "quorum_cop"

namespace service_nodes
{
  static_assert(quorum_cop::REORG_SAFETY_BUFFER_IN_BLOCKS < STATE_CHANGE_VOTE_LIFETIME, "Safety buffer should always be less than the vote lifetime");

  quorum_cop::quorum_cop(cryptonote::core& core)
    : m_core(core), m_obligations_height(0), m_last_checkpointed_height(0)
  {
  }

  void quorum_cop::init()
  {
    m_obligations_height = 0;
    m_last_checkpointed_height = 0;
    m_uptime_proof_seen.clear();

    uint64_t top_height;
    crypto::hash top_hash;
    m_core.get_blockchain_top(top_height, top_hash);

    cryptonote::block blk;
    if(m_core.get_block_by_hash(top_hash, blk))
      process_quorums(blk);
  }

  bool quorum_cop::check_service_node(const crypto::public_key &pubkey, const service_node_info &info) const
  {
    if (!m_uptime_proof_seen.count(pubkey))
      return false;

    return true;
  }

  void quorum_cop::blockchain_detached(uint64_t height)
  {
    if (m_obligations_height >= height)
    {
      LOG_ERROR("The blockchain was detached to height: " << height << ", but quorum cop has already processed votes up to " << m_obligations_height);
      LOG_ERROR("This implies a reorg occured that was over " << REORG_SAFETY_BUFFER_IN_BLOCKS << ". This should never happen! Please report this to the devs.");
      m_obligations_height = height;
    }

    if (m_last_checkpointed_height >= height)
    {
      LOG_ERROR("The blockchain was detached to height: " << height << ", but quorum cop has already processed votes up to " << m_last_checkpointed_height);
      LOG_ERROR("This implies a reorg occured that was over " << REORG_SAFETY_BUFFER_IN_BLOCKS << ". This should never happen! Please report this to the devs.");
      m_last_checkpointed_height = height;
    }
    m_vote_pool.remove_expired_votes(height);
  }

  void quorum_cop::set_votes_relayed(std::vector<quorum_vote_t> const &relayed_votes)
  {
    m_vote_pool.set_relayed(relayed_votes);
  }

  std::vector<quorum_vote_t> quorum_cop::get_relayable_votes()
  {
    std::vector<quorum_vote_t> result = m_vote_pool.get_relayable_votes();
    return result;
  }

  static int find_index_in_quorum_group(std::vector<crypto::public_key> const &group, crypto::public_key const &my_pubkey)
  {
    int result = -1;
    auto it = std::find(group.begin(), group.end(), my_pubkey);
    if (it == group.end()) return result;
    result = std::distance(group.begin(), it);
    return result;
  }

  void quorum_cop::process_quorums(cryptonote::block const &block)
  {
    crypto::public_key my_pubkey;
    crypto::secret_key my_seckey;
    if(!m_core.get_service_node_keys(my_pubkey, my_seckey))
      return;

    if (!m_core.is_service_node(my_pubkey, /*require_active=*/ true))
      return;

    uint64_t const height = cryptonote::get_block_height(block);
    for (int i = 0; i < (int)quorum_type::count; i++)
    {
      quorum_type const type = static_cast<quorum_type>(i);
      uint64_t const vote_lifetime = service_nodes::quorum_vote_lifetime(type);

      uint64_t const latest_height = std::max(m_core.get_current_blockchain_height(), m_core.get_target_blockchain_height());
      if (latest_height < vote_lifetime)
        continue;

      uint64_t const start_voting_from_height = latest_height - vote_lifetime;
      if (height < start_voting_from_height)
        continue;

      uint8_t const hard_fork_version = block.major_version;
      switch(type)
      {
        default:
        {
          assert("Unhandled quorum type " == 0);
          LOG_ERROR("Unhandled quorum type with value: " << (int)type);
        } break;

        case quorum_type::obligations:
        {
          time_t const now = time(nullptr);
          constexpr time_t min_lifetime = UPTIME_PROOF_MAX_TIME_IN_SECONDS;

          bool alive_for_min_time = (now - m_core.get_start_time()) >= min_lifetime;
          if (!alive_for_min_time)
            continue;

          if (m_obligations_height < start_voting_from_height)
            m_obligations_height = start_voting_from_height;

          for (; m_obligations_height < (height - REORG_SAFETY_BUFFER_IN_BLOCKS); m_obligations_height++)
          {
            if (m_core.get_hard_fork_version(m_obligations_height) < 16) continue;

            const std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(quorum_type::obligations, m_obligations_height);
            if (!quorum)
            {
              LOG_ERROR("Obligations quorum for height " << m_obligations_height << " was not cached in daemon!");
              continue;
            }

            if (quorum->workers.empty()) continue;

            int index_in_group = find_index_in_quorum_group(quorum->validators, my_pubkey);
            if (index_in_group <= -1) continue;

            //
            // NOTE: I am in the quorum
            //
            auto worker_states = m_core.get_service_node_list_state(quorum->workers);
            auto worker_it = worker_states.begin();
            CRITICAL_REGION_LOCAL(m_lock);
            int good = 0, total = 0;
            for (size_t node_index = 0; node_index < quorum->workers.size(); ++worker_it, ++node_index)
            {
              // If the SN no longer exists then it will be omitted from the worker_states vector,]
              // so if the elements do not line up skip ahead.
              while (worker_it->pubkey != quorum->workers[node_index] && node_index < quorum->workers.size())
                node_index++;
              if (node_index == quorum->workers.size())
                break;
              total++;

              const auto& node_key = worker_it->pubkey;
              const auto& info = worker_it->info;

              bool checks_passed = check_service_node(node_key, info);

              new_state vote_for_state;
              if (checks_passed) {
                if (!info.is_decommissioned()) {
                  good++;
                  continue;
                }

                vote_for_state = new_state::recommission;
                LOG_PRINT_L2("Decommissioned service node " << quorum->workers[node_index] << " is now passing required checks; voting to recommission");
              }
              else
              {
                int64_t credit = calculate_decommission_credit(info, latest_height);

                if (info.is_decommissioned()) {
                  if (credit >= 0) {
                    LOG_PRINT_L2("Decommissioned service node " << quorum->workers[node_index] << " is still not passing required checks, but has remaining credit ("
                                                                << credit << " blocks); abstaining (to leave decommissioned)");
                    continue;
                  }

                  LOG_PRINT_L2("Decommissioned service node " << quorum->workers[node_index] << " has no remaining credit; voting to deregister");
                  vote_for_state = new_state::deregister; // Credit ran out!
                }
                else
                {
                  if (credit >= DECOMMISSION_MINIMUM) {
                    vote_for_state = new_state::decommission;
                    LOG_PRINT_L2("Service node " << quorum->workers[node_index] << " has stopped passing required checks, but has sufficient earned credit ("
                                                 << credit << " blocks) to avoid deregistration; voting to decommission");
                  }
                  else
                  {
                    vote_for_state = new_state::deregister;
                    LOG_PRINT_L2("Service node " << quorum->workers[node_index] << " has stopped passing required checks, but does not have sufficient earned credit ("
                                                 << credit << " blocks, " << DECOMMISSION_MINIMUM << " required) to decommission; voting to deregister");
                  }
                }
              }

              quorum_vote_t vote = service_nodes::make_state_change_vote(m_obligations_height, static_cast<uint16_t>(index_in_group), node_index, vote_for_state, my_pubkey, my_seckey);
              cryptonote::vote_verification_context vvc;
              if (!handle_vote(vote, vvc))
                LOG_ERROR("Failed to add uptime check_state vote; reason: " << print_vote_verification_context(vvc, nullptr));
            }
            if (good > 0)
              LOG_PRINT_L2(good << " of " << total << " service nodes are active and passing checks; no state change votes required");
          }
        }
        break;

        case quorum_type::checkpointing:
        {
          if (m_last_checkpointed_height < start_voting_from_height)
            m_last_checkpointed_height = start_voting_from_height;

          for (m_last_checkpointed_height += (m_last_checkpointed_height % CHECKPOINT_INTERVAL);
              m_last_checkpointed_height <= height;
              m_last_checkpointed_height += CHECKPOINT_INTERVAL)
          {
            const std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(quorum_type::checkpointing, m_last_checkpointed_height);
            if (!quorum)
            {
              LOG_ERROR("Checkpoint quorum for height: " << m_last_checkpointed_height << " was not cached in daemon!");
              continue;
            }

            int index_in_group = find_index_in_quorum_group(quorum->workers, my_pubkey);
            if (index_in_group <= -1) continue;

            //
            // NOTE: I am in the quorum, handle checkpointing
            //
            quorum_vote_t vote = {};
            vote.type = quorum_type::checkpointing;
            vote.checkpoint.block_hash = m_core.get_block_id_by_height(m_last_checkpointed_height);

            if (vote.checkpoint.block_hash == crypto::null_hash)
            {
              LOG_ERROR("Could not get block hash for block on height: " << m_last_checkpointed_height);
              continue;
            }

            vote.block_height = m_last_checkpointed_height;
            vote.group = quorum_group::worker;
            vote.index_in_group = static_cast<uint16_t>(index_in_group);
            vote.signature = make_signature_from_vote(vote, my_pubkey, my_seckey);

            cryptonote::vote_verification_context vvc = {};
            if (!handle_vote(vote, vvc))
              LOG_ERROR("Failed to add checkpoint vote reason: " << print_vote_verification_context(vvc, nullptr));
          }
        }
        break;
      }
    }
  }

  void quorum_cop::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs)
  {
    process_quorums(block);

    uint64_t const height = cryptonote::get_block_height(block) + 1;
    m_vote_pool.remove_expired_votes(height);
    m_vote_pool.remove_used_votes(txs);
  }

  bool quorum_cop::handle_vote(quorum_vote_t const &vote, cryptonote::vote_verification_context &vvc)
  {
    vvc = {};
    switch(vote.type)
    {
      default:
      {
        LOG_PRINT_L1("Unhandled vote type with value: " << (int)vote.type);
        assert("Unhandled vote type" == 0);
        return false;
      };

      case quorum_type::obligations: break;
      case quorum_type::checkpointing:
      {
        cryptonote::block block;
        if (!m_core.get_block_by_hash(vote.checkpoint.block_hash, block))
        {
          LOG_PRINT_L1("Vote does not reference valid block hash: " << vote.checkpoint.block_hash);
          return false;
        }
      }
      break;
    }

    // NOTE: Only do validation that relies on access cryptonote::core here in quorum cop, the rest goes in voting pool
    std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(vote.type, vote.block_height);
    if (!quorum)
    {
      LOG_ERROR("Quorum state for height: " << vote.block_height << " was not cached in daemon!");
      return false;
    }

    uint64_t latest_height = std::max(m_core.get_current_blockchain_height(), m_core.get_target_blockchain_height());
    std::vector<pool_vote_entry> votes = m_vote_pool.add_pool_vote_if_unique(latest_height, vote, vvc, *quorum);
    bool result = !vvc.m_verification_failed;

    if (!vvc.m_added_to_pool)
      return result;

    switch(vote.type)
    {
      default:
      {
        LOG_PRINT_L1("Unhandled vote type with value: " << (int)vote.type);
        assert("Unhandled vote type" == 0);
        return false;
      };

      case quorum_type::obligations: break;
      case quorum_type::checkpointing:
      {
        if (votes.size() >= CHECKPOINT_MIN_VOTES)
        {
          cryptonote::checkpoint_t checkpoint = {};
          checkpoint.type = cryptonote::checkpoint_type::service_node;
          checkpoint.height = vote.block_height;
          checkpoint.block_hash = vote.checkpoint.block_hash;
          checkpoint.signatures.reserve(votes.size());

          for (pool_vote_entry const &pool_vote : votes)
          {
            voter_to_signature vts = {};
            vts.voter_index = pool_vote.vote.index_in_group;
            vts.signature = pool_vote.vote.signature;
            checkpoint.signatures.push_back(vts);
          }

          m_core.get_blockchain_storage().update_checkpoint(checkpoint);
        }
        {
          LOG_PRINT_L2("Don't have enough votes yet to submit a checkpoint: have " << votes.size() << " of " << CHECKPOINT_MIN_VOTES << " required");
        }
      }
      break;
    }
    return result;
  }

  static crypto::hash make_hash(crypto::public_key const &pubkey, uint64_t timestamp, uint32_t pub_ip, uint16_t storage_port)
  {
    constexpr size_t BUFFER_SIZE = sizeof(pubkey) + sizeof(timestamp) + sizeof(pub_ip) + sizeof(storage_port);

    boost::endian::native_to_little_inplace(timestamp);
    boost::endian::native_to_little_inplace(pub_ip);
    boost::endian::native_to_little_inplace(storage_port);

    char buf[BUFFER_SIZE];
    crypto::hash result;
    memcpy(buf, reinterpret_cast<const void *>(&pubkey), sizeof(pubkey));
    memcpy(buf + sizeof(pubkey), reinterpret_cast<const void *>(&timestamp), sizeof(timestamp));
    memcpy(buf + sizeof(pubkey) + sizeof(timestamp), reinterpret_cast<const void *>(&pub_ip), sizeof(pub_ip));
    memcpy(buf + sizeof(pubkey) + sizeof(timestamp) + sizeof(pub_ip), reinterpret_cast<const void *>(&storage_port), sizeof(storage_port));

    crypto::cn_fast_hash(buf, sizeof(buf), result);

    return result;
  }

  bool quorum_cop::handle_uptime_proof(const cryptonote::NOTIFY_UPTIME_PROOF::request &proof)
  {
    uint64_t now = time(nullptr);

    uint64_t timestamp = proof.timestamp;
    const crypto::public_key& pubkey = proof.pubkey;
    const crypto::signature& sig = proof.sig;
    const uint32_t public_ip = proof.public_ip;
    const uint16_t storage_port = proof.storage_port;

    if((timestamp < now - UPTIME_PROOF_BUFFER_IN_SECONDS) || (timestamp > now + UPTIME_PROOF_BUFFER_IN_SECONDS)) {
      LOG_PRINT_L2("Rejecting uptime proof from " << pubkey << ": timestamp is too far from now");
      return false;
    }

    if(!m_core.is_service_node(pubkey, /*require_active=*/ false)) {
      LOG_PRINT_L2("Rejecting uptime proof from " << pubkey << ": no such service node is currently registered");
      return false;
    }

    uint64_t height = m_core.get_current_blockchain_height();
    uint8_t hf_ver = m_core.get_hard_fork_version(height);
    if(hf_ver >= cryptonote::network_version_16 && proof.arqma_snode_major < 7)
      return false;

    CRITICAL_REGION_LOCAL(m_lock);
    if(m_uptime_proof_seen[pubkey].timestamp >= now - (UPTIME_PROOF_FREQUENCY_IN_SECONDS / 2))
      return false; // already received one uptime proof for this node recently.

    if (epee::net_utils::is_ip_local(public_ip) || epee::net_utils::is_ip_loopback(public_ip)) return false;

    crypto::hash hash = make_hash(pubkey, timestamp, public_ip, storage_port);
    if (!crypto::check_signature(hash, pubkey, sig)) {
      LOG_PRINT_L2("Rejecting uptime proof from " << pubkey << ": signature validation failed");
      return false;
    }

    m_uptime_proof_seen[pubkey] = {now, proof.arqma_snode_major, proof.arqma_snode_minor, proof.arqma_snode_patch};
    LOG_PRINT_L2("Accepted uptime proof from " << pubkey);
    return true;
  }

  void quorum_cop::generate_uptime_proof_request(cryptonote::NOTIFY_UPTIME_PROOF::request& req) const
  {
    req.arqma_snode_major = static_cast<uint16_t>(ARQMA_VERSION_MAJOR);
    req.arqma_snode_minor = static_cast<uint16_t>(ARQMA_VERSION_MINOR);
    req.arqma_snode_patch = static_cast<uint16_t>(ARQMA_VERSION_PATCH);

    crypto::public_key pubkey;
    crypto::secret_key seckey;
    m_core.get_service_node_keys(pubkey, seckey);

    req.timestamp = time(nullptr);
    req.pubkey = pubkey;
    req.public_ip = m_core.get_service_node_public_ip();
    req.storage_port = m_core.get_storage_port();

    crypto::hash hash = make_hash(req.pubkey, req.timestamp, req.public_ip, req.storage_port);
    crypto::generate_signature(hash, pubkey, seckey, req.sig);
  }

  bool quorum_cop::prune_uptime_proof()
  {
    uint64_t now = time(nullptr);
    const uint64_t prune_from_timestamp = now - UPTIME_PROOF_MAX_TIME_IN_SECONDS;
    CRITICAL_REGION_LOCAL(m_lock);

    std::vector<crypto::public_key> to_remove;
    for(const auto &proof : m_uptime_proof_seen)
    {
      if(proof.second.timestamp < prune_from_timestamp)
        to_remove.push_back(proof.first);
    }
    for(const auto &pk : to_remove)
      m_uptime_proof_seen.erase(pk);

    return true;
  }

  proof_info quorum_cop::get_uptime_proof(const crypto::public_key &pubkey) const
  {

    CRITICAL_REGION_LOCAL(m_lock);
    const auto it = m_uptime_proof_seen.find(pubkey);
    if(it == m_uptime_proof_seen.end())
      return {};

    return it->second;
  }

  // Calculate the decommission credit for a service node.  If the SN is current decommissioned this
  // returns the number of blocks remaining in the credit; otherwise this is the number of currently
  // accumulated blocks.
  int64_t quorum_cop::calculate_decommission_credit(const service_node_info &info, uint64_t current_height)
  {
    // If currently decommissioned, we need to know how long it was up before being decommissioned;
    // otherwise we need to know how long since it last become active until now.
    int64_t blocks_up;
    if (info.is_decommissioned()) // decommissioned; the negative of active_since_height tells us when the period leading up to the current decommission started
      blocks_up = int64_t(info.last_decommission_height) - (-info.active_since_height);
    else
      blocks_up = int64_t(current_height) - int64_t(info.active_since_height);

    // Now we calculate the credit earned from being up for `blocks_up` blocks
    int64_t credit = 0;
    if (blocks_up >= 0) {
      credit = blocks_up * DECOMMISSION_CREDIT_PER_DAY / BLOCKS_EXPECTED_IN_HOURS(24);

      if (info.decommission_count <= info.is_decommissioned()) // Has never been decommissioned (or is currently in the first decommission), so add initial starting credit
        credit += DECOMMISSION_INITIAL_CREDIT;
      if (credit > DECOMMISSION_MAX_CREDIT)
        credit = DECOMMISSION_MAX_CREDIT; // Cap the available decommission credit blocks if above the max
    }

    // If currently decommissioned, remove any used credits used for the current downtime
    if (info.is_decommissioned())
      credit -= int64_t(current_height) - int64_t(info.last_decommission_height);

    return credit;
  }
}
