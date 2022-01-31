#include "cryptonote_config.h"
#include "common/arqma.h"
#include "int-util.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include <limits>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <cfenv>

#include "blockchain.h"
#include "service_node_rules.h"

namespace arqma_bc = config::blockchain_settings;

namespace service_nodes
{
  uint64_t get_staking_requirement(cryptonote::network_type m_nettype, uint64_t height, uint8_t hard_fork_version)
  {
    if(m_nettype != cryptonote::MAINNET)
    {
      constexpr int64_t heights[] = {
        12800,
        12825,
        12850,
        12875,
        12900,
        12950,
        13100,
        14500,
      };

      constexpr int64_t stake_req = 100;
      constexpr int64_t max_stake_req = 250;
      constexpr int64_t stake_inc = 25;

      assert(static_cast<int64_t>(height) >= heights[0]);
      constexpr uint64_t LAST_HT = heights[arqma::array_count(heights) - 1];
      if(height >= LAST_HT)
        return max_stake_req * arqma_bc::ARQMA;

      size_t i = 0;
      for(size_t index = 1; index < arqma::array_count(heights); index++)
      {
        if(heights[index] > static_cast<int64_t>(height))
        {
          i = (index - 1);
          break;
        }
      }

      int64_t result = stake_req + (i * stake_inc);
      return static_cast<uint64_t>(result) * arqma_bc::ARQMA;
    }

    constexpr int64_t heights[] = {
      99999999,
      99999999,
      99999999,
      // to be filled upon release
    };

    constexpr int64_t stake_req = 25000;
    constexpr int64_t max_stake_req = 100000;
    constexpr int64_t stake_inc = 10000;

    assert(static_cast<int64_t>(height) >= heights[0]);
    constexpr uint64_t LAST_HEIGHT = heights[arqma::array_count(heights) - 1];
    if(height >= LAST_HEIGHT)
      return max_stake_req * arqma_bc::ARQMA;

    size_t i = 0;
    for(size_t index = 1; index < arqma::array_count(heights); index++)
    {
      if(heights[index] > static_cast<int64_t>(height))
      {
        i = (index - 1);
        break;
      }
    }

    int64_t result = stake_req + (i * stake_inc);
    return static_cast<uint64_t>(result) * arqma_bc::ARQMA;
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
      hash_ptr += sizeof(nonce);
    }

    assert(hash_ptr == (char *)result.data + sizeof(result));
    return result;
  }

  uint64_t get_locked_key_image_unlock_height(cryptonote::network_type nettype, uint64_t node_register_height, uint64_t curr_height)
  {
    uint64_t blocks_to_lock = staking_num_lock_blocks(nettype);
    uint64_t result = curr_height + (blocks_to_lock / 2);
    return result;
  }

  uint64_t get_min_node_contribution(uint64_t staking_requirement, uint64_t total_reserved, size_t num_contributions)
  {
    const uint64_t needed = staking_requirement - total_reserved;
    const size_t max_num_of_contributions = MAX_NUMBER_OF_CONTRIBUTORS * MAX_KEY_IMAGES_PER_CONTRIBUTOR;
    assert(max_num_of_contributions > num_contributions);
    if(max_num_of_contributions <= num_contributions)
      return UINT64_MAX;

    const size_t num_contributions_remaining_avail = max_num_of_contributions - num_contributions;
    return needed / num_contributions_remaining_avail;
  }

  uint64_t get_min_node_contribution_in_portions(uint64_t staking_requirement, uint64_t total_reserved, size_t num_contributions)
  {
    uint64_t atomic_amount = get_min_node_contribution(staking_requirement, total_reserved, num_contributions);
    uint64_t result = (atomic_amount == UINT64_MAX) ? UINT64_MAX : (get_portions_to_make_amount(staking_requirement, atomic_amount));
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

  static bool get_portions_from_percent(double cur_percent, uint64_t& portions)
  {
    if(cur_percent < 0.0 || cur_percent > 100.0)
      return false;

    if(cur_percent == 100.0)
    {
      portions = STAKING_SHARE_PARTS;
    }
    else
    {
      portions = (cur_percent / 100.0) * (double)STAKING_SHARE_PARTS;
    }

    return true;
  }

  bool get_portions_from_percent_str(std::string cut_str, uint64_t& portions)
  {
    if(!cut_str.empty() && cut_str.back() == '%')
    {
      cut_str.pop_back();
    }

    double cut_percent;
    try
    {
      cut_percent = boost::lexical_cast<double>(cut_str);
    }
    catch(...)
    {
      return false;
    }

    return get_portions_from_percent(cut_percent, portions);
  }

  uint64_t uniform_distribution_portable(std::mt19937_64& mersenne_twister, uint64_t n)
  {
    uint64_t secureMax = mersenne_twister.max() - mersenne_twister.max() % n;
    uint64_t x;
    do x = mersenne_twister();
    while(x >= secureMax);
    return x / (secureMax / n);
  }

  template<typename... Args>
  static bool check_condition(bool condition, std::string* reason, Args&&... args)
  {
    if(condition && reason)
    {
      std::ostringstream os;
      (os << ... << std::forward<Args>(args));
      *reason = os.str();
    }
    return condition;
  }

  bool validate_unstake_tx(uint64_t blockchain_height, cryptonote::transaction const &tx, cryptonote::tx_extra_field &extra, std::string *reason)
  {
    if(check_condition(tx.type != cryptonote::txtype::key_image_unlock, reason, tx, ", uses wrong transaction type, expected: ", cryptonote::txtype::key_image_unlock))
      return false;

    return true;
  }

}