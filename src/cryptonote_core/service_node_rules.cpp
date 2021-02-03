#include "cryptonote_config.h"
#include "common/exp2.h"
#include "common/int-util.h"
#include <vector>

#include "service_node_rules.h"

namespace service_nodes
{
  uint64_t get_staking_requirement(cryptonote::network_type m_nettype, uint64_t height)
  {
    if(m_nettype != cryptonote::MAINNET)
      return arqma_bc::ARQMA * 1000;
    // We will need to work at some equation
    return arqma_bc::ARQMA * 10000;
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
    uint64_t portions_left = STAKING_SHARE_PARTS;

    for(const auto portion : portions)
    {
      const uint64_t min_portions = std::min(portions_left, STAKING_SHARE_PARTS);
      if(portion < min_portions || portion > portions_left)
        return false;
      portions_left -= portion;
    }

    return true;
  }

}
