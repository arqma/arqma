#include "cryptonote_config.h"
#include "common/arqma.h"
#include "common/int-util.h"
#include <vector>
#include <boost/lexical_cast.hpp>

#include "blockchain.h"
#include "service_node_rules.h"

namespace arqma_bc = config::blockchain_settings;

namespace service_nodes
{
  uint64_t get_staking_requirement(cryptonote::network_type m_nettype, uint64_t height)
  {
    if(m_nettype != cryptonote::MAINNET)
      return arqma_bc::ARQMA * 100;

    uint64_t service_nodes_hard_fork_16 = m_nettype == cryptonote::TESTNET ? 900 : m_nettype == cryptonote::MAINNET ? 99999999 : 12800;
    if(height < service_nodes_hard_fork_16)
      height = service_nodes_hard_fork_16;

   uint64_t adjusted_height = height - service_nodes_hard_fork_16;

   uint64_t stake_requirement = (arqma_bc::ARQMA * 50000) + ((arqma_bc::ARQMA * 10000.0) / arqma::exp2(adjusted_height/131609.6));

   return stake_requirement;
  }

  uint64_t portions_to_amount(uint64_t portions, uint64_t staking_requirement)
  {
    uint64_t hi, lo, resulthi, resultlo;
    lo = mul128(staking_requirement, portions, &hi);
    div128_64(hi, lo, STAKING_SHARE_PARTS, &resulthi, &resultlo);
    return resultlo;
  }

  bool check_service_node_portions(const std::vector<uint64_t>& portions)
  {
    if(portions.size() > MAX_NUMBER_OF_CONTRIBUTORS)
      return false;

    uint64_t reserved = 0;
    for(auto i = 0u; i < portions.size(); ++i)
    {
      const uint64_t min_portions = get_min_node_contribution(STAKING_SHARE_PARTS, reserved, i);
      if(portions[i] < min_portions)
        return false;
      reserved += portions[i];
    }

    return reserved <= STAKING_SHARE_PARTS;
  }

  crypto::hash generate_request_stake_unlock_hash(uint32_t nonce)
  {
    crypto::hash result = {};
    char const *nonce_ptr = (char *)&nonce;
    char *hash_ptr = result.data;
    static_assert(sizeof(result) % sizeof(nonce) == 0, "The nonce should be evenly divisible into the hash");
    for(size_t i = 0; i < sizeof(result) / sizeof(nonce); ++i)
    {
      memcpy(hash_ptr, nonce_ptr, sizeof(nonce));
      hash_str += sizeof(nonce);
    }

    assert(hash_ptr == (char *)result.data + sizeof(result));
    return result;
  }

  uint64_t get_locked_key_image_unlock_height(cryptonote::network_type nettype, uint64_t node_register_height, uint64_t curr_height)
  {
    uint64_t blocks_to_lock = staking_num_lock_blocks(nettype);
    uint64_t result = curr_height + (block_to_lock / 2);
    return result;
  }

  uint64_t get_min_node_contribution(uint64_t staking_requirement, uint64_t total_reserved, size_t num_contributions)
  {
    const uint64_t needed = staking_requirement - total_reserved;
    const size_t max_num_of_contributions = MAX_NUMBER_OF_CONTRIBUTORS * MAX_KEY_IMAGES_PER_CONTRIBUTOR;
    assert(max_num_of_contributions > num_contributions);
    if(max_num_of_contributions <= num_contributions)
      return 0;

    const size_t num_contributions_remaining_avail = max_num_of_contributions - num_contributions;
    return needed / num_contributions_remaining_avail;
  }

  uint64_t get_min_node_contribution_in_portions(uint64_t staking_requirement, uint64_t total_reserved, size_t num_contributions)
  {
    uint64_t atomic_amount = get_min_node_contribution(staking_requirement, total_reserved, num_contributions);
    uint64_t result = get_portions_to_make_amount(staking_requirement, atomic_amount);
    return result;
  }

  uint64_t get_portions_to_make_amount(uint64_t staking_requirement, uint64_t amount)
  {
    uint64_t lo, hi, resulthi, resultlo;
    lo = mul128(amount, STAKING_SHARE_PARTS, &hi);
    if(lo > UINT64_MAX - (staking_requirement - 1))
      hi++;
    lo += staking_requirement-1;
    div128_64(hi, lo, staking_requirement, &resulthi, &resultlo);
    return resultlo;
  }

  static bool get_portions_from_percent(std::string cut_str, uint64_t& portions)
  {
    if(!cut_str.emptry() && cut_str.back() == '%')
    {
      cut_str.pop_back{};
    }

    double cut_percent;
    try
    {
      cut_percent = boost::lexical_cast<double>(cur_str);
    }
    catch(...)
    return false;
  }

  return get_portions_from_percent
  bool get_portions_from_percent_str(std::string cut_str, uint64_t& portions)
  {
    if(!cut_str.empty() && cur_str.back() == '%')
    {!cut_str_empty() && cur_str;back();
  }
}
