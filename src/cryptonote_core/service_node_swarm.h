#pragma once

#include "service_node_rules.h"

#include <map>
#include <vector>
#include <random>

namespace service_nodes
{
  using swarm_snode_map_t = std::map<swarm_id_t, std::vector<crypto::public_key>>;
  struct swarm_size
  {
    swarm_id_t swarm_id;
    size_t size;
  };
  struct excess_pool_snode
  {
    crypto::public_key public_key;
    swarm_id_t swarm_id;
  };

  void calc_swarm_changes(swarm_snode_map_t& swarm_to_snodes, uint64_t seed);
}