#pragma once

#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "service_node_deregister.h"

#include <random>

namespace service_nodes
{
  constexpr size_t   QUORUM_SIZE                      = 10;
  constexpr size_t   QUORUM_LIFETIME                  = (6 * deregister_vote::DEREGISTER_LIFETIME_BY_HEIGHT);
  constexpr size_t   MIN_VOTES_TO_KICK_SERVICE_NODE   = 7;
  constexpr size_t   MIN_VOTES_TO_CHECKPOINT          = MIN_VOTES_TO_KICK_SERVICE_NODE;
  constexpr size_t   NTH_OF_THE_NETWORK_TO_TEST       = 100;
  constexpr size_t   MIN_NODES_TO_TEST                = 50;
  constexpr size_t   MIN_SWARM_SIZE                   = 5;
  constexpr size_t   MAX_SWARM_SIZE                   = 10;
  constexpr size_t   IDEAL_SWARM_MARGIN               = 2;
  constexpr size_t   IDEAL_SWARM_SIZE                 = MIN_SWARM_SIZE + IDEAL_SWARM_MARGIN;
  constexpr size_t   EXCESS_BASE                      = MIN_SWARM_SIZE;
  constexpr size_t   NEW_SWARM_SIZE                   = IDEAL_SWARM_SIZE;
  // The lower swarm percentile that will be randomly filled with new service nodes
  constexpr size_t   FILL_SWARM_LOWER_PERCENTILE      = 25;
  // Redistribute decommissioned snodes to the smallest swarms
  constexpr size_t   DECOMMISSIONED_REDISTRIBUTION_LOWER_PERCENTILE = 0;
  // The upper swarm percentile that will be randomly selected during stealing
  constexpr size_t   STEALING_SWARM_UPPER_PERCENTILE  = 75;
  // We never create a new swarm unless there are SWARM_BUFFER extra nodes
  // available in the queue.
  constexpr size_t   SWARM_BUFFER                     = 5;
  // if a swarm has strictly less nodes than this, it is considered unhealthy
  // and nearby swarms will mirror it's data. It will disappear, and is already considered gone.
  constexpr int      MAX_KEY_IMAGES_PER_CONTRIBUTOR   = 1;
  constexpr uint64_t QUEUE_SWARM_ID                   = 0;
  constexpr uint64_t KEY_IMAGE_AWAITING_UNLOCK_HEIGHT = 0;
  constexpr uint64_t CHECKPOINT_INTERVAL              = 4;

  using swarm_id_t = uint64_t;
  constexpr swarm_id_t UNASSIGNED_SWARM_ID = UINT64_MAX;
  static_assert(MIN_VOTES_TO_KICK_SERVICE_NODE <= QUORUM_SIZE, "The number of votes required to kick can't exceed the actual quorum size, otherwise we never kick.");
  static_assert(MIN_VOTES_TO_CHECKPOINT <= QUORUM_SIZE, "The number of votes required to kick can't exceed the actual quorum size, otherwise we never kick.");


  inline uint64_t staking_num_lock_blocks(cryptonote::network_type nettype)
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

  uint64_t uniform_distribution_portable(std::mt19937_64& mersenne_twister, uint64_t n);
}

