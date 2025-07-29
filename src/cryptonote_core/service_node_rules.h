#pragma once

#include "crypto/crypto.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_config.h"
#include "service_node_voting.h"

namespace service_nodes
{
  constexpr int64_t DECOMMISSION_CREDIT_PER_DAY = BLOCKS_EXPECTED_IN_HOURS(24) / 30;
  constexpr int64_t DECOMMISSION_INITIAL_CREDIT = BLOCKS_EXPECTED_IN_HOURS(2);
  constexpr int64_t DECOMMISSION_MAX_CREDIT     = BLOCKS_EXPECTED_IN_HOURS(48);
  constexpr int64_t DECOMMISSION_MINIMUM        = BLOCKS_EXPECTED_IN_HOURS(2);

  static_assert(DECOMMISSION_INITIAL_CREDIT <= DECOMMISSION_MAX_CREDIT, "Initial registration decommission credit cannot be larger than the maximum decommission credit");

  constexpr uint64_t CHECKPOINT_NUM_CHECKPOINTS_FOR_CHAIN_FINALITY = 2;
  constexpr uint64_t CHECKPOINT_INTERVAL                           = 4;
  constexpr uint64_t CHECKPOINT_STORE_PERSISTENTLY_INTERVAL        = 60;
  constexpr uint64_t CHECKPOINT_VOTE_LIFETIME                      = CHECKPOINT_STORE_PERSISTENTLY_INTERVAL;

  constexpr int16_t CHECKPOINT_NUM_QUORUMS_TO_PARTICIPATE_IN = 8;
  constexpr int16_t CHECKPOINT_MAX_MISSABLE_VOTES            = 4;
  static_assert(CHECKPOINT_MAX_MISSABLE_VOTES < CHECKPOINT_NUM_QUORUMS_TO_PARTICIPATE_IN,
                "The maximum number of votes a service node can miss can not be greater than the amount of checkpoint "
                "quorums they must participate in before we check if they should be deregistered or not.");

  constexpr size_t STATE_CHANGE_NTH_OF_THE_NETWORK_TO_TEST         = 100;
  constexpr size_t STATE_CHANGE_MIN_NODES_TO_TEST                  = 50;
  constexpr uint64_t VOTE_LIFETIME                                 = BLOCKS_EXPECTED_IN_HOURS(2);

  constexpr size_t STATE_CHANGE_MIN_VOTES_TO_CHANGE_STATE          = 7;
  constexpr size_t STATE_CHANGE_QUORUM_SIZE                        = 10;
  constexpr int MIN_TIME_IN_S_BEFORE_VOTING                        = UPTIME_PROOF_MAX_TIME_IN_SECONDS;
  constexpr size_t CHECKPOINT_QUORUM_SIZE                          = 20;
  constexpr size_t CHECKPOINT_MIN_VOTES                            = 13;

  static_assert(STATE_CHANGE_MIN_VOTES_TO_CHANGE_STATE <= STATE_CHANGE_QUORUM_SIZE, "The number of votes required to kick can't exceed the actual quorum size, otherwise we never kick.");
  static_assert(CHECKPOINT_MIN_VOTES <= CHECKPOINT_QUORUM_SIZE, "The number of votes required to kick can't exceed the actual quorum size, otherwise we never kick.");

  constexpr uint64_t REORG_SAFETY_BUFFER_IN_BLOCKS                  = (CHECKPOINT_INTERVAL * CHECKPOINT_NUM_CHECKPOINTS_FOR_CHAIN_FINALITY) + (CHECKPOINT_INTERVAL - 1);
  static_assert(REORG_SAFETY_BUFFER_IN_BLOCKS < VOTE_LIFETIME, "Safety buffer should always less than the vote lifetime");

  constexpr uint64_t IP_CHANGE_WINDOW_IN_SECONDS                    = 86400;
  constexpr uint64_t IP_CHANGE_BUFFER_IN_SECONDS                    = 7200;

  constexpr size_t   MIN_SWARM_SIZE                                 = 5;
  constexpr size_t   MAX_SWARM_SIZE                                 = 10;
  constexpr size_t   IDEAL_SWARM_MARGIN                             = 2;
  constexpr size_t   IDEAL_SWARM_SIZE                               = MIN_SWARM_SIZE + IDEAL_SWARM_MARGIN;
  constexpr size_t   EXCESS_BASE                                    = MIN_SWARM_SIZE;
  constexpr size_t   NEW_SWARM_SIZE                                 = IDEAL_SWARM_SIZE;
  // The lower swarm percentile that will be randomly filled with new service nodes
  constexpr size_t   FILL_SWARM_LOWER_PERCENTILE                    = 25;
  // Redistribute snodes for decommissioned swarms to the smallest swarms
  constexpr size_t   DECOMMISSIONED_REDISTRIBUTION_LOWER_PERCENTILE = 0;
  // The upper swarm percentile that will be randomly selected during stealing
  constexpr size_t   STEALING_SWARM_UPPER_PERCENTILE                = 75;
  // We never create a new swarm unless there are SWARM_BUFFER extra nodes
  // available in the queue.
  constexpr size_t   SWARM_BUFFER                                   = 5;
  // if a swarm has strictly less nodes than this, it is considered unhealthy
  // and nearby swarms will mirror it's data. It will disappear, and is already considered gone.
  constexpr uint64_t QUEUE_SWARM_ID                                 = 0;
  constexpr uint64_t KEY_IMAGE_AWAITING_UNLOCK_HEIGHT               = 0;

  constexpr uint64_t STATE_CHANGE_TX_LIFETIME_IN_BLOCKS             = VOTE_LIFETIME;

  constexpr uint64_t VOTE_OR_TX_VERIFY_HEIGHT_BUFFER                = 5;

  constexpr std::array<int, 3> MIN_STORAGE_SERVER_VERSION{{1, 0, 0}};
//  constexpr std::array<int, 3> MIN_ARQNET_VERSION{{1, 0, 0}};

  struct proof_version
  {
    uint8_t hardfork;
    std::array<uint16_t, 3> version;
  };

  constexpr proof_version MIN_UPTIME_PROOF_VERSIONS[] = {
    {cryptonote::network_version_17, {8,0,0}},
    {cryptonote::network_version_16, {7,2,0}},
  };

  using swarm_id_t = uint64_t;
  constexpr swarm_id_t UNASSIGNED_SWARM_ID                          = UINT64_MAX;

  constexpr size_t min_votes_for_quorum_type(quorum_type q)
  {
    return q == quorum_type::obligations ? STATE_CHANGE_MIN_VOTES_TO_CHANGE_STATE :
           q == quorum_type::checkpointing ? CHECKPOINT_MIN_VOTES :
           std::numeric_limits<size_t>::max();
  };

  constexpr quorum_type max_quorum_type_for_hf(uint8_t hard_fork_version)
  {
    return hard_fork_version <= cryptonote::network_version_16 ? quorum_type::obligations : quorum_type::checkpointing;
  }

  constexpr uint64_t staking_num_lock_blocks(cryptonote::network_type nettype)
  {
    switch(nettype)
    {
      case cryptonote::TESTNET:
      case cryptonote::STAGENET:
        return BLOCKS_EXPECTED_IN_DAYS(2);
      default:
        return BLOCKS_EXPECTED_IN_DAYS(30);
    }
  }

  static_assert(STAKING_SHARE_PARTS != UINT64_MAX, "UINT64_MAX is used as the invalid value for failing to calculate min_node_contribution");
  uint64_t get_min_node_contribution(uint64_t staking_requirement, uint64_t total_reserved, size_t num_contributions);
  uint64_t get_min_node_contribution_in_portions(uint64_t staking_requirement, uint64_t total_reserved, size_t num_contributions);

  uint64_t get_staking_requirement(cryptonote::network_type nettype, uint64_t height);

  uint64_t portions_to_amount(uint64_t portions, uint64_t staking_requirement);

  bool check_service_node_portions(const std::vector<uint64_t>& portions);

  crypto::hash generate_request_stake_unlock_hash(uint32_t nonce);
  uint64_t get_locked_key_image_unlock_height(cryptonote::network_type nettype, uint64_t node_register_height, uint64_t curr_height);

  uint64_t get_portions_to_make_amount(uint64_t staking_requirement, uint64_t amount);

  bool get_portions_from_percent_str(std::string cut_str, uint64_t& portions);

  bool validate_unstake_tx(uint64_t blockchain_height, cryptonote::transaction const &tx, cryptonote::tx_extra_field &extra, std::string *reason);
}
