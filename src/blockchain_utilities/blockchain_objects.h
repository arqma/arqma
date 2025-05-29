#ifndef BLOCKCHAIN_OBJECTS_H
#define BLOCKCHAIN_OBJECTS_H

#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/tx_pool.h"
#include "cryptonote_core/service_node_list.h"
#include "cryptonote_core/service_node_voting.h"

struct BlockchainAndSNlistAndPool
{
  cryptonote::Blockchain blockchain;
  cryptonote::tx_memory_pool tx_pool;
  service_nodes::service_node_list sn_list;
  BlockchainAndSNlistAndPool() :
    blockchain(tx_pool, sn_list),
    sn_list(blockchain),
    tx_pool(blockchain) { }
};

#endif