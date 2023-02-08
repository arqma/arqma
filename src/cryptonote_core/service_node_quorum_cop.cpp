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
#include "common/util.h"
#include "net/local_ip.h"
#include "boost/endian/conversion.hpp"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "quorum_cop"

namespace service_nodes
{
  quorum_cop::quorum_cop(cryptonote::core& core)
    : m_core(core), m_obligations_height(0), m_last_checkpointed_height(0)
  {
  }

  void quorum_cop::init()
  {
    m_obligations_height = 0;
    m_last_checkpointed_height = 0;
  }

  service_node_test_results quorum_cop::check_service_node(const crypto::public_key &pubkey, const service_node_info &info) const
  {
    service_node_test_results result;
    uint64_t now = time(nullptr);
    proof_info const &proof = *info.proof;
    uint64_t time_since_last_uptime_proof = now - std::max(proof.timestamp, proof.effective_timestamp);

    bool check_uptime_obligation = true;
    bool check_checkpoint_obligation = true;

    if (check_uptime_obligation && time_since_last_uptime_proof > UPTIME_PROOF_MAX_TIME_IN_SECONDS)
    {
      LOG_PRINT_L1("Service Node: " << pubkey << ", failed uptime proof obligation check: the last uptime proof was older than: "
                                    << UPTIME_PROOF_MAX_TIME_IN_SECONDS << "s. Time since last uptime proof was: "
                                    << tools::get_human_readable_timespan(std::chrono::seconds(time_since_last_uptime_proof)));
      result.uptime_proved = false;
    }

    const auto &ips = proof.public_ips;
    if (ips[0].first && ips[1].first) {
      std::vector<cryptonote::block> blocks;
      if (m_core.get_blocks(info.last_ip_change_height, 1, blocks)) {
        uint64_t find_ips_used_since = std::max(uint64_t(std::time(nullptr)) - IP_CHANGE_WINDOW_IN_SECONDS, uint64_t(blocks[0].timestamp) + IP_CHANGE_BUFFER_IN_SECONDS);
        if (ips[0].second > find_ips_used_since && ips[1].second > find_ips_used_since)
          result.single_ip = false;
      }
    }

    if (check_checkpoint_obligation && !info.is_decommissioned())
    {
      int num_votes = 0;
      for (bool voted : proof.votes)
        num_votes += voted;

      if (num_votes <= CHECKPOINT_MAX_MISSABLE_VOTES)
      {
        LOG_PRINT_L1("Service Node: " << pubkey << ", failed checkpoint obligation check: missed the last: "
                                      << CHECKPOINT_MAX_MISSABLE_VOTES << " checkpoint votes from: "
                                      << CHECKPOINT_MIN_QUORUMS_NODE_MUST_VOTE_IN_BEFORE_DEREGISTER_CHECK
                                      << " quorums that they were required to participate in.");
        result.voted_in_checkpoints = false;
      }
    }

    return result;
  }

  void quorum_cop::blockchain_detached(uint64_t height)
  {
    if (m_obligations_height >= height)
    {
      LOG_ERROR("The blockchain was detached to height: " << height << ", but quorum cop has already processed votes up to " << m_obligations_height);
      LOG_ERROR("This implies a reorg occured that was over " << REORG_SAFETY_BUFFER_IN_BLOCKS << ". This should never happen! Please report this to the devs.");
      m_obligations_height = height;
    }

    if (m_last_checkpointed_height >= height + REORG_SAFETY_BUFFER_IN_BLOCKS)
    {
      LOG_ERROR("The blockchain was detached to height: " << height << ", but quorum cop has already processed votes for checkpointing up to " << m_last_checkpointed_height);
      LOG_ERROR("This implies a reorg occured that was over " << REORG_SAFETY_BUFFER_IN_BLOCKS << ". This should rarely happen! Please report this to the devs.");
      m_last_checkpointed_height = height;
    }

    m_vote_pool.remove_expired_votes(height);
  }

  void quorum_cop::set_votes_relayed(std::vector<quorum_vote_t> const &relayed_votes)
  {
    m_vote_pool.set_relayed(relayed_votes);
  }

  std::vector<quorum_vote_t> quorum_cop::get_relayable_votes(uint64_t current_height)
  {
    return m_vote_pool.get_relayable_votes(current_height);
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
    uint8_t const hard_fork_version = block.major_version;
    if (hard_fork_version < cryptonote::network_version_16)
      return;

    auto my_keys = m_core.get_service_node_keys();

    if (!m_core.is_service_node(my_keys->pub, /*require_active=*/ true))
      return;

    uint64_t const height = cryptonote::get_block_height(block);
    uint64_t const latest_height = std::max(m_core.get_current_blockchain_height(), m_core.get_target_blockchain_height());
    if (latest_height < VOTE_LIFETIME)
      return;

    uint64_t const start_voting_from_height = latest_height - VOTE_LIFETIME;
    if (height < start_voting_from_height)
      return;

    for (int i = 0; i <= static_cast<int>(quorum_type::count); i++)
    {
      quorum_type const type = static_cast<quorum_type>(i);

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
          bool alive_for_min_time = (now - m_core.get_start_time()) >= MIN_TIME_IN_S_BEFORE_VOTING;
          if (!alive_for_min_time)
            break;

          m_obligations_height = std::max(m_obligations_height, start_voting_from_height);
          for (; m_obligations_height < (height - REORG_SAFETY_BUFFER_IN_BLOCKS); m_obligations_height++)
          {
            if (m_core.get_hard_fork_version(m_obligations_height) < cryptonote::network_version_16) continue;

            if (m_core.get_hard_fork_version(m_obligations_height) >= cryptonote::network_version_16)
            {
              if (std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(quorum_type::checkpointing, m_obligations_height))
              {
                for (size_t index_in_quorum = 0; index_in_quorum < quorum->workers.size(); index_in_quorum++)
                {
                  crypto::public_key const &key = quorum->workers[index_in_quorum];
                  m_core.record_checkpoint_vote(key, m_vote_pool.received_checkpoint_vote(m_obligations_height, index_in_quorum));
                }
              }
            }

            const std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(quorum_type::obligations, m_obligations_height);
            if (!quorum)
            {
              LOG_ERROR("Obligations quorum for height " << m_obligations_height << " was not cached in daemon!");
              continue;
            }

            if (quorum->workers.empty()) continue;

            int index_in_group = find_index_in_quorum_group(quorum->validators, my_keys->pub);
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

              const auto &node_key = worker_it->pubkey;
              const auto &info = *worker_it->info;

              if (!info.can_be_voted_on(m_obligations_height))
                continue;

              auto test_results = check_service_node(node_key, info);
              bool passed = test_results.passed();

              new_state vote_for_state;
              if (passed) {
                if (info.is_decommissioned()) {
                  vote_for_state = new_state::recommission;
                  LOG_PRINT_L2("Decommissioned service node " << quorum->workers[node_index] << " is now passing required checks, voting to recommission");
                } else if (!test_results.single_ip) {
                  vote_for_state = new_state::ip_change_penalty;
                  LOG_PRINT_L2("Service node: " << quorum->workers[node_index] << " was observed with multiple IPs recently, voting to reset reward position");
                } else {
                  good++;
                  continue;
                }

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

              quorum_vote_t vote = service_nodes::make_state_change_vote(m_obligations_height, static_cast<uint16_t>(index_in_group), node_index, vote_for_state, *my_keys);
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
          uint64_t start_checkpointing_height = start_voting_from_height;
          if ((start_checkpointing_height % CHECKPOINT_INTERVAL) > 0)
            start_checkpointing_height += (CHECKPOINT_INTERVAL - (start_checkpointing_height % CHECKPOINT_INTERVAL));

          m_last_checkpointed_height = std::max(start_checkpointing_height, m_last_checkpointed_height);
          for (; m_last_checkpointed_height <= height; m_last_checkpointed_height += CHECKPOINT_INTERVAL)
          {
            if (m_core.get_hard_fork_version(m_last_checkpointed_height) <= cryptonote::network_version_16)
              continue;

            if (m_last_checkpointed_height < REORG_SAFETY_BUFFER_IN_BLOCKS)
              continue;

            const std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(quorum_type::checkpointing, m_last_checkpointed_height);
            if (!quorum)
            {
              LOG_ERROR("Checkpoint quorum for height: " << m_last_checkpointed_height << " was not cached in daemon!");
              continue;
            }

            int index_in_group = find_index_in_quorum_group(quorum->workers, my_keys->pub);
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
            vote.signature = make_signature_from_vote(vote, *my_keys);

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

    uint64_t const height = cryptonote::get_block_height(block) + 1; // chain height = new top block height + 1
    m_vote_pool.remove_expired_votes(height);
    m_vote_pool.remove_used_votes(txs);
  }

  bool quorum_cop::handle_vote(quorum_vote_t const &vote, cryptonote::vote_verification_context &vvc)
  {
    vvc = {};
    uint64_t const latest_height = std::max(m_core.get_current_blockchain_height(), m_core.get_target_blockchain_height());
    if (!verify_vote_age(vote, latest_height, vvc))
      return false;

    std::shared_ptr<const testing_quorum> quorum = m_core.get_testing_quorum(vote.type, vote.block_height);
    if (!quorum)
    {
      vvc.m_invalid_block_height = true;
      LOG_ERROR("Quorum state for vote height " << vote.block_height << " was not cached in daemon!");
      return false;
    }

    if (!verify_vote_against_quorum(vote, vvc, *quorum))
      return false;

    std::vector<pool_vote_entry> votes = m_vote_pool.add_pool_vote_if_unique(vote, vvc);
    if (!vvc.m_added_to_pool)
      return true;

    bool result = true;
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
          cryptonote::Blockchain &blockchain = m_core.get_blockchain_storage();

          blockchain.lock();
          ARQMA_DEFER { blockchain.unlock(); };

          bool update_checkpoint = true;
          if (blockchain.get_checkpoint(vote.block_height, checkpoint) && checkpoint.block_hash == vote.checkpoint.block_hash)
          {
            update_checkpoint = checkpoint.signatures.size() != service_nodes::CHECKPOINT_QUORUM_SIZE;
            if (update_checkpoint)
            {
              checkpoint.signatures.reserve(service_nodes::CHECKPOINT_QUORUM_SIZE);
              std::sort(checkpoint.signatures.begin(), checkpoint.signatures.end(),
                        [](service_nodes::voter_to_signature const &lhs, service_nodes::voter_to_signature const &rhs)
              {
                return lhs.voter_index < rhs.voter_index;
              });

              for (pool_vote_entry const &pool_vote : votes)
              {
                auto it = std::lower_bound(checkpoint.signatures.begin(), checkpoint.signatures.end(), pool_vote,
                                           [](voter_to_signature const &lhs, pool_vote_entry const &vote)
                {
                  return lhs.voter_index < vote.vote.index_in_group;
                });

                if (it == checkpoint.signatures.end() || pool_vote.vote.index_in_group != it->voter_index)
                {
                  update_checkpoint = true;
                  voter_to_signature vts = {};
                  vts.voter_index = pool_vote.vote.index_in_group;
                  vts.signature = pool_vote.vote.signature;
                  checkpoint.signatures.insert(it, vts);
                }
              }
            }
          }
          else
          {
            checkpoint = {};
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
          }

          if (update_checkpoint)
            m_core.get_blockchain_storage().update_checkpoint(checkpoint);
        }
        else
        {
          LOG_PRINT_L2("Don't have enough votes yet to submit a checkpoint: have " << votes.size() << " of " << CHECKPOINT_MIN_VOTES << " required");
        }
      }
      break;
    }
    return result;
  }

  // Calculate the decommission credit for a service node.  If the SN is current decommissioned this
  // returns the number of blocks remaining in the credit; otherwise this is the number of currently
  // accumulated blocks.
  int64_t quorum_cop::calculate_decommission_credit(const service_node_info &info, uint64_t current_height)
  {
    // If currently decommissioned, we need to know how long it was up before being decommissioned;
    // otherwise we need to know how long since it last become active until now.
    int64_t blocks_up;
    if (!info.is_fully_funded())
      blocks_up = 0;
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
