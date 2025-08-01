// Copyright (c) 2018-2022, The Arqma Network
// Copyright (c) 2014-2018, The Monero Project
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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <algorithm>
#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "common/rules.h"
#include "include_base_utils.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "tx_pool.h"
#include "blockchain.h"
#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_boost_serialization.h"
#include "cryptonote_config.h"
#include "cryptonote_core/miner.h"
#include "misc_language.h"
#include "profile_tools.h"
#include "file_io_utils.h"
#include "int-util.h"
#include "common/threadpool.h"
#include "common/boost_serialization_helper.h"
#include "warnings.h"
#include "crypto/hash.h"
#include "cryptonote_core.h"
#include "ringct/rctSigs.h"
#include "common/perf_timer.h"
#include "common/notify.h"
#include "service_node_voting.h"
#include "service_node_list.h"
#include "common/varint.h"
#include "common/pruning.h"
#include "common/lock.h"
#include "time_helper.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "blockchain"

#define FIND_BLOCKCHAIN_SUPPLEMENT_MAX_SIZE (100*1024*1024) // 100 MB

using namespace crypto;


//#include "serialization/json_archive.h"

/* TODO:
 *  Clean up code:
 *    Possibly change how outputs are referred to/indexed in blockchain and wallets
 *
 */

namespace arqma_bc = config::blockchain_settings;
using namespace cryptonote;
using epee::string_tools::pod_to_hex;
extern "C" void rx_slow_hash_allocate_state();
extern "C" void rx_slow_hash_free_state();

DISABLE_VS_WARNINGS(4267)

#define MERROR_VER(x) MCERROR("verify", x)

// used to overestimate the block reward when estimating a per kB to use
#define BLOCK_REWARD_OVERESTIMATE (10 * 1000000000000)

Blockchain::block_extended_info::block_extended_info(const alt_block_data_t &src, block const &blk, checkpoint_t const *checkpoint)
{
  assert((src.checkpointed) == (checkpoint != nullptr));
  *this                         = {};
  this->bl                      = blk;
  this->checkpointed            = src.checkpointed;
  if (checkpoint) this->checkpoint = *checkpoint;
  this->height                  = src.height;
  this->block_cumulative_weight = src.cumulative_weight;
  this->cumulative_difficulty   = src.cumulative_difficulty;
  this->already_generated_coins = src.already_generated_coins;
}

//------------------------------------------------------------------
Blockchain::Blockchain(tx_memory_pool& tx_pool, service_nodes::service_node_list& service_node_list) :
  m_db(), m_tx_pool(tx_pool), m_hardfork(NULL), m_timestamps_and_difficulties_height(0), m_current_block_cumul_weight_limit(0), m_current_block_cumul_weight_median(0),
  m_max_prepare_blocks_threads(8), m_db_sync_on_blocks(true), m_db_sync_threshold(1), m_db_sync_mode(db_async), m_db_default_sync(false),
  m_fast_sync(true), m_show_time_stats(false), m_sync_counter(0), m_bytes_to_sync(0), m_cancel(false),
  m_long_term_block_weights_window(CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE),
  m_long_term_effective_median_block_weight(0),
  m_long_term_block_weights_cache_tip_hash(crypto::null_hash),
  m_long_term_block_weights_cache_rolling_median(CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE),
  m_difficulty_for_next_block_top_hash(crypto::null_hash),
  m_difficulty_for_next_block(1),
  m_service_node_list(service_node_list),
  m_btc_valid(false),
  m_batch_success(true),
  m_prepare_height(0)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
}
//------------------------------------------------------------------
Blockchain::~Blockchain()
{
  try { deinit(); }
  catch(const std::exception &e) { }
}
//------------------------------------------------------------------
bool Blockchain::have_tx(const crypto::hash &id) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->tx_exists(id);
}
//------------------------------------------------------------------
bool Blockchain::have_tx_keyimg_as_spent(const crypto::key_image &key_im) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return  m_db->has_key_image(key_im);
}
//------------------------------------------------------------------
// This function makes sure that each "input" in an input (mixins) exists
// and collects the public key for each from the transaction it was included in
// via the visitor passed to it.
template <class visitor_t>
bool Blockchain::scan_outputkeys_for_indexes(const txin_to_key& tx_in_to_key, visitor_t &vis, const crypto::hash &tx_prefix_hash, uint64_t* pmax_related_block_height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  // ND: Disable locking and make method private.
  //std::unique_lock lock{*this};

  // verify that the input has key offsets (that it exists properly, really)
  if(!tx_in_to_key.key_offsets.size())
    return false;

  // cryptonote_format_utils uses relative offsets for indexing to the global
  // outputs list.  that is to say that absolute offset #2 is absolute offset
  // #1 plus relative offset #2.
  // TODO: Investigate if this is necessary / why this is done.
  std::vector<uint64_t> absolute_offsets = relative_output_offsets_to_absolute(tx_in_to_key.key_offsets);
  std::vector<output_data_t> outputs;

  bool found = false;
  auto it = m_scan_table.find(tx_prefix_hash);
  if (it != m_scan_table.end())
  {
    auto its = it->second.find(tx_in_to_key.k_image);
    if (its != it->second.end())
    {
      outputs = its->second;
      found = true;
    }
  }

  if (!found)
  {
    try
    {
      m_db->get_output_key(epee::span<const uint64_t>(&tx_in_to_key.amount, 1), absolute_offsets, outputs, true);
      if (absolute_offsets.size() != outputs.size())
      {
        MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
        return false;
      }
    }
    catch (...)
    {
      MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
      return false;
    }
  }
  else
  {
    // check for partial results and add the rest if needed;
    if (outputs.size() < absolute_offsets.size() && outputs.size() > 0)
    {
      MDEBUG("Additional outputs needed: " << absolute_offsets.size() - outputs.size());
      std::vector<uint64_t> add_offsets;
      std::vector<output_data_t> add_outputs;
      add_outputs.reserve(absolute_offsets.size() - outputs.size());
      for (size_t i = outputs.size(); i < absolute_offsets.size(); i++)
        add_offsets.push_back(absolute_offsets[i]);
      try
      {
        m_db->get_output_key(epee::span<const uint64_t>(&tx_in_to_key.amount, 1), add_offsets, add_outputs, true);
        if (add_offsets.size() != add_outputs.size())
        {
          MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
          return false;
        }
      }
      catch (...)
      {
        MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
        return false;
      }
      outputs.insert(outputs.end(), add_outputs.begin(), add_outputs.end());
    }
  }

  size_t count = 0;
  for (const uint64_t& i : absolute_offsets)
  {
    try
    {
      output_data_t output_index;
      try
      {
        // get tx hash and output index for output
        if (count < outputs.size())
          output_index = outputs.at(count);
        else
          output_index = m_db->get_output_key(tx_in_to_key.amount, i);

        // call to the passed boost visitor to grab the public key for the output
        if (!vis.handle_output(output_index.unlock_time, output_index.pubkey, output_index.commitment))
        {
          MERROR_VER("Failed to handle_output for output no = " << count << ", with absolute offset " << i);
          return false;
        }
      }
      catch (...)
      {
        MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount << ", absolute_offset = " << i);
        return false;
      }

      // if on last output and pmax_related_block_height not null pointer
      if(++count == absolute_offsets.size() && pmax_related_block_height)
      {
        // set *pmax_related_block_height to tx block height for this output
        auto h = output_index.height;
        if(*pmax_related_block_height < h)
        {
          *pmax_related_block_height = h;
        }
      }
    }
    catch (const OUTPUT_DNE& e)
    {
      MERROR_VER("Output does not exist: " << e.what());
      return false;
    }
    catch (const TX_DNE& e)
    {
      MERROR_VER("Transaction does not exist: " << e.what());
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
uint64_t Blockchain::get_current_blockchain_height() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->height();
}
//------------------------------------------------------------------
//FIXME: possibly move this into the constructor, to avoid accidentally
//       dereferencing a null BlockchainDB pointer
bool Blockchain::init(BlockchainDB* db, const network_type nettype, bool offline, const cryptonote::test_options *test_options, difficulty_type fixed_difficulty, const GetCheckpointsCallback& get_checkpoints/* = nullptr*/)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  CHECK_AND_ASSERT_MES(nettype != FAKECHAIN || test_options, false, "fake chain network type used without options");

  auto lock = tools::unique_locks(m_tx_pool, *this);

  if (db == nullptr)
  {
    LOG_ERROR("Attempted to init Blockchain with null DB");
    return false;
  }
  if (!db->is_open())
  {
    LOG_ERROR("Attempted to init Blockchain with unopened DB");
    delete db;
    return false;
  }

  m_db = db;

  m_nettype = test_options != NULL ? FAKECHAIN : nettype;

  if(!m_checkpoints.init(m_nettype, m_db))
    throw std::runtime_error("Failed to initialize checkpoints");

  m_offline = offline;
  m_fixed_difficulty = fixed_difficulty;
  if (m_hardfork == nullptr)
    m_hardfork = new HardFork(*db, 1);

  if (test_options)
  {
    for (auto n = 0u; n < test_options->hard_forks.size(); ++n)
    {
      const auto& hf = test_options->hard_forks.at(n);
      m_hardfork->add_fork(hf.first, hf.second, 0, n + 1);
    }
  }
  else
  {
    for (const auto &record : HardFork::get_hardcoded_hard_forks(m_nettype))
    {
      m_hardfork->add_fork(record.version, record.height, record.threshold, record.time);
    }
  }

  m_hardfork->init();

  m_db->set_hard_fork(m_hardfork);

  // if the blockchain is new, add the genesis block
  // this feels kinda kludgy to do it this way, but can be looked at later.
  // TODO: add function to create and store genesis block,
  //       taking testnet into account
  if(!m_db->height())
  {
    MINFO("Blockchain not loaded, generating genesis block.");
    block bl;
    block_verification_context bvc{};
    generate_genesis_block(bl);
    db_wtxn_guard wtxn_guard(m_db);
    add_new_block(bl, bvc, nullptr/*checkpoint*/);
    CHECK_AND_ASSERT_MES(!bvc.m_verification_failed, false, "Failed to add genesis block to blockchain");
  }
  // TODO: if blockchain load successful, verify blockchain against both
  //       hard-coded and runtime-loaded (and enforced) checkpoints.
  else
  {
  }

  if (m_nettype != FAKECHAIN)
  {
    // ensure we fixup anything we found and fix in the future
    m_db->fixup();
  }

  db_rtxn_guard rtxn_guard(m_db);

  // check how far behind we are
  uint64_t top_block_timestamp = m_db->get_top_block_timestamp();
  uint64_t timestamp_diff = time(NULL) - top_block_timestamp;

  // genesis block has no timestamp, could probably change it to have timestamp of 1341378000...
  if(!top_block_timestamp)
    timestamp_diff = time(NULL) - 1523689524;

  // create general purpose async service queue

  m_async_work_idle = std::make_unique<work_type>(m_async_service.get_executor());
  // we only need 1
  m_async_pool.create_thread([this] { m_async_service.run(); });

#if defined(PER_BLOCK_CHECKPOINT)
  if (m_nettype != FAKECHAIN)
    load_compiled_in_block_hashes(get_checkpoints);
#endif

  MINFO("Blockchain initialized. last block: " << m_db->height() - 1 << ", " << epee::misc_utils::get_time_interval_string(timestamp_diff) << " time ago, current difficulty: " << get_difficulty_for_next_block());

  rtxn_guard.stop();

  uint64_t num_popped_blocks = 0;
  while (!m_db->is_read_only())
  {
    uint64_t top_height;
    const crypto::hash top_id = m_db->top_block_hash(&top_height);
    const block top_block = m_db->get_top_block();
    const uint8_t ideal_hf_version = get_ideal_hard_fork_version(top_height);
    if (ideal_hf_version <= 1 || ideal_hf_version == top_block.major_version)
    {
      if (num_popped_blocks > 0)
        MGINFO("Initial popping done, top block: " << top_id << ", top height: " << top_height << ", block version: " << (uint64_t)top_block.major_version);
      break;
    }
    else
    {
      if (num_popped_blocks == 0)
        MGINFO("Current top block " << top_id << " at height " << top_height << " has version " << (uint64_t)top_block.major_version << " which disagrees with the ideal version " << (uint64_t)ideal_hf_version);
      if (num_popped_blocks % 100 == 0)
        MGINFO("Popping blocks... " << top_height);
      ++num_popped_blocks;
      block popped_block;
      std::vector<transaction> popped_txs;
      try
      {
        m_db->pop_block(popped_block, popped_txs);
      }
      // anything that could cause this to throw is likely catastrophic,
      // so we re-throw
      catch (const std::exception& e)
      {
        MERROR("Error popping block from blockchain: " << e.what());
        throw;
      }
      catch (...)
      {
        MERROR("Error popping block from blockchain, throwing!");
        throw;
      }
    }
  }
  if (num_popped_blocks > 0)
  {
    m_timestamps_and_difficulties_height = 0;
    m_hardfork->reorganize_from_chain_height(get_current_blockchain_height());
    m_tx_pool.on_blockchain_dec();
  }

  if (test_options && test_options->long_term_block_weight_window)
  {
    m_long_term_block_weights_window = test_options->long_term_block_weight_window;
    m_long_term_block_weights_cache_rolling_median = epee::misc_utils::rolling_median_t<uint64_t>(m_long_term_block_weights_window);
  }

  {
    db_txn_guard txn_guard(m_db, m_db->is_read_only());
    if(!update_next_cumulative_weight_limit())
      return false;
  }

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
  {
    const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(m_db->height()));
    if (seedhash != crypto::null_hash)
      rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());
  }

  hook_block_added(m_checkpoints);
  hook_blockchain_detached(m_checkpoints);
  for (InitHook* hook : m_init_hooks) hook->init();

  return true;
}
//------------------------------------------------------------------
bool Blockchain::init(BlockchainDB* db, HardFork*& hf, const network_type nettype, bool offline)
{
  if (hf != nullptr)
    m_hardfork = hf;
  bool res = init(db, nettype, offline, NULL);
  if (hf == nullptr)
    hf = m_hardfork;
  return res;
}
//------------------------------------------------------------------
bool Blockchain::store_blockchain()
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // lock because the rpc_thread command handler also calls this
  std::unique_lock lock{m_db->m_synchronization_lock};

  TIME_MEASURE_START(save);
  // TODO: make sure sync(if this throws that it is not simply ignored higher
  // up the call stack
  try
  {
    m_db->sync();
  }
  catch (const std::exception& e)
  {
    MERROR(std::string("Error syncing blockchain db: ") + e.what() + "-- shutting down now to prevent issues!");
    throw;
  }
  catch (...)
  {
    MERROR("There was an issue storing the blockchain, shutting down now to prevent issues!");
    throw;
  }

  TIME_MEASURE_FINISH(save);
  if(m_show_time_stats)
    MINFO("Blockchain stored OK, took: " << save << " ms");
  return true;
}
//------------------------------------------------------------------
bool Blockchain::deinit()
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  MTRACE("Stopping blockchain read/write activity");

  // stop async service
  m_async_work_idle.reset();
  m_async_pool.join_all();
  m_async_service.stop();


  // as this should be called if handling a SIGSEGV, need to check
  // if m_db is a NULL pointer (and thus may have caused the illegal
  // memory operation), otherwise we may cause a loop.
  try
  {
    if(m_db)
    {
      m_db->close();
      MTRACE("Local blockchain read/write activity stopped successfully");
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR(std::string("Error closing blockchain db: ") + e.what());
  }
  catch (...)
  {
    LOG_ERROR("There was an issue closing/storing the blockchain, shutting down now to prevent issues!");
  }

  delete m_hardfork;
  m_hardfork = NULL;
  delete m_db;
  m_db = NULL;

  return true;
}
//------------------------------------------------------------------
// This function removes blocks from the top of blockchain.
// It starts a batch and calls private method pop_block_from_blockchain().
void Blockchain::pop_blocks(uint64_t nblocks)
{
  uint64_t i = 0;
  auto lock = tools::unique_locks(m_tx_pool, *this);

  bool stop_batch = m_db->batch_start();

  try
  {
    const uint64_t blockchain_height = m_db->height();
    if (blockchain_height > 0)
      nblocks = std::min(nblocks, blockchain_height - 1);
    while(i < nblocks)
    {
      pop_block_from_blockchain();
      ++i;
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR("Error when popping blocks after processing " << i << " blocks: " << e.what());
    if(stop_batch)
      m_db->batch_abort();
    return;
  }

  auto split_height = m_db->height();
  for (BlockchainDetachedHook* hook : m_blockchain_detached_hooks) hook->blockchain_detached(split_height);

  if(stop_batch)
    m_db->batch_stop();

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
  {
    const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(m_db->height()));
    rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());
  }
}
//------------------------------------------------------------------
// This function tells BlockchainDB to remove the top block from the
// blockchain and then returns all transactions (except the miner tx, of course)
// from it to the tx_pool
block Blockchain::pop_block_from_blockchain()
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  m_timestamps_and_difficulties_height = 0;

  block popped_block;
  std::vector<transaction> popped_txs;

  CHECK_AND_ASSERT_THROW_MES(m_db->height() > 1, "It is forbidden to remove ArQmA Genesis Block.");

  try
  {
    m_db->pop_block(popped_block, popped_txs);
  }
  // anything that could cause this to throw is likely catastrophic,
  // so we re-throw
  catch (const std::exception& e)
  {
    LOG_ERROR("Error popping block from blockchain: " << e.what());
    throw;
  }
  catch (...)
  {
    LOG_ERROR("Error popping block from blockchain, throwing!");
    throw;
  }

  m_hardfork->on_block_popped(1);

  // return transactions from popped block to the tx_pool
  size_t pruned = 0;
  for (transaction& tx : popped_txs)
  {
    if (tx.pruned)
    {
      ++pruned;
      continue;
    }
    if (!is_coinbase(tx))
    {
      cryptonote::tx_verification_context tvc{};

      // FIXME: HardFork
      // Besides the below, popping a block should also remove the last entry
      // in hf_versions.
      uint8_t version = get_ideal_hard_fork_version(m_db->height());

      // We assume that if they were in a block, the transactions are already
      // known to the network as a whole. However, if we had mined that block,
      // that might not be always true. Unlikely though, and always relaying
      // these again might cause a spike of traffic as many nodes re-relay
      // all the transactions in a popped block when a reorg happens.
      bool r = m_tx_pool.add_tx(tx, tvc, tx_pool_options::from_block(), version);
      if (!r)
      {
        LOG_ERROR("Error returning transaction to tx_pool");
      }
    }
  }
  if (pruned)
    MWARNING(pruned << " pruned txes could not be added back to the txpool");

  m_blocks_longhash_table.clear();
  m_scan_table.clear();
  m_blocks_txs_check.clear();

  CHECK_AND_ASSERT_THROW_MES(update_next_cumulative_weight_limit(), "Error updating next cumulative weight limit");
  m_tx_pool.on_blockchain_dec();
  invalidate_block_template_cache();

  return popped_block;
}
//------------------------------------------------------------------
bool Blockchain::reset_and_set_genesis_block(const block& b)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  m_timestamps_and_difficulties_height = 0;
  invalidate_block_template_cache();
  m_db->reset();
  m_db->drop_alt_blocks();
  m_hardfork->init();

  for(InitHook* hook : m_init_hooks) hook->init();

  db_wtxn_guard wtxn_guard(m_db);
  block_verification_context bvc{};
  add_new_block(b, bvc, nullptr/*checkpoint*/);
  if (!update_next_cumulative_weight_limit())
    return false;
  return bvc.m_added_to_main_chain && !bvc.m_verification_failed;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_tail_id(uint64_t& height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  return m_db->top_block_hash(&height);
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_tail_id() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->top_block_hash();
}
//------------------------------------------------------------------
/* Builds a list of block hashes representing certain blocks from the blockchain in reverse
 * chronological order; used when synchronizing to verify that a peer's chain matches ours.
 *
 * The blocks chosen for height H, are:
 *   - the most recent 11 (H-1, H-2, ..., H-10, H-11)
 *   - base-2 exponential drop off from there, so: H-13, H-17, H-25, etc... (going down to, at smallest, height 1)
 *   - the genesis block (height 0)
 */
void Blockchain::get_short_chain_history(std::list<crypto::hash>& ids) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  uint64_t sz = m_db->height();

  if(!sz)
    return;

  db_rtxn_guard rtxn_guard(m_db);
  for (uint64_t i = 0, decr = 1, offset = 1; offset < sz; ++i)
  {
    ids.push_back(m_db->get_block_hash_from_height(sz - offset));
    if (i >= 10) decr *= 2;
    offset += decr;
  }

  ids.push_back(m_db->get_block_hash_from_height(0));
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_block_id_by_height(uint64_t height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  try
  {
    return m_db->get_block_hash_from_height(height);
  }
  catch (const BLOCK_DNE& e)
  {
  }
  catch (const std::exception& e)
  {
    MERROR(std::string("Something went wrong fetching block hash by height: ") + e.what());
    throw;
  }
  catch (...)
  {
    MERROR(std::string("Something went wrong fetching block hash by height"));
    throw;
  }
  return null_hash;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_pending_block_id_by_height(uint64_t height) const
{
  if(m_prepare_height && height >= m_prepare_height && height - m_prepare_height < m_prepare_nblocks)
    return (*m_prepare_blocks)[height - m_prepare_height].hash;
  return get_block_id_by_height(height);
}
//------------------------------------------------------------------
bool Blockchain::get_block_by_hash(const crypto::hash &h, block &blk, bool *orphan) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  // try to find block in main chain
  try
  {
    blk = m_db->get_block(h);
    if (orphan)
      *orphan = false;
    return true;
  }
  // try to find block in alternative chain
  catch (const BLOCK_DNE& e)
  {
    alt_block_data_t data;
    cryptonote::blobdata blob;
    if(m_db->get_alt_block(h, &data, &blob, nullptr))
    {
      if(!cryptonote::parse_and_validate_block_from_blob(blob, blk))
      {
        MERROR("Found Block " << h << " in alt chain, but failed to parse it.");
        throw std::runtime_error("Found Block in alt chain, but failed to parse it.");
      }
      if(orphan)
        *orphan = true;
      return true;
    }
  }
  catch (const std::exception& e)
  {
    MERROR(std::string("Something went wrong fetching block by hash: ") + e.what());
    throw;
  }
  catch (...)
  {
    MERROR(std::string("Something went wrong fetching block hash by hash"));
    throw;
  }

  return false;
}
//------------------------------------------------------------------
size_t get_difficulty_blocks_count(uint8_t version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  if(version < 7)
    return DIFFICULTY_BLOCKS_COUNT;
  else if(version < 9)
    return DIFFICULTY_BLOCKS_COUNT_V2;
  else if(version < 10)
    return DIFFICULTY_BLOCKS_COUNT_V3;
  else if(version < 16)
    return DIFFICULTY_BLOCKS_COUNT_V11;

  return DIFFICULTY_BLOCKS_COUNT_V16;
}
//-----------------------------------------------------------------
uint8_t get_current_diff_target(uint8_t version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  if(version < 10)
    return DIFFICULTY_TARGET_V2;
  else if(version < 16)
    return DIFFICULTY_TARGET_V11;

  return DIFFICULTY_TARGET_V16;
}
//------------------------------------------------------------------
// This function aggregates the cumulative difficulties and timestamps of the
// last DIFFICULTY_BLOCKS_COUNT blocks and passes them to next_difficulty,
// returning the result of that call.  Ignores the genesis block, and can use
// less blocks than desired if there aren't enough.
difficulty_type Blockchain::get_difficulty_for_next_block()
{
  if (m_fixed_difficulty)
  {
    return m_db->height() ? m_fixed_difficulty : 1;
  }

  LOG_PRINT_L3("Blockchain::" << __func__);

  crypto::hash top_hash = get_tail_id();
  {
    std::unique_lock diff_lock{m_difficulty_lock};
    // we can call this without the blockchain lock, it might just give us
    // something a bit out of date, but that's fine since anything which
    // requires the blockchain lock will have acquired it in the first place,
    // and it will be unlocked only when called from the getinfo RPC
    if (top_hash == m_difficulty_for_next_block_top_hash)
      return m_difficulty_for_next_block;
  }

  std::unique_lock lock{*this};
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> difficulties;
  uint64_t height;
  top_hash = get_tail_id(height); // get it again now that we have the lock
  ++height; // top block height to blockchain height

  uint8_t version = get_current_hard_fork_version();
  size_t difficulty_blocks_count = get_difficulty_blocks_count(version);

  if (m_timestamps_and_difficulties_height != 0 && ((height - m_timestamps_and_difficulties_height) == 1) && m_timestamps.size() >= difficulty_blocks_count)
  {
    uint64_t index = height - 1;
    m_timestamps.push_back(m_db->get_block_timestamp(index));
    m_difficulties.push_back(m_db->get_block_cumulative_difficulty(index));

    while (m_timestamps.size() > difficulty_blocks_count)
      m_timestamps.erase(m_timestamps.begin());
    while (m_difficulties.size() > difficulty_blocks_count)
      m_difficulties.erase(m_difficulties.begin());

    m_timestamps_and_difficulties_height = height;
    timestamps = m_timestamps;
    difficulties = m_difficulties;
  }
  else
  {
    uint64_t offset = height - std::min <uint64_t> (height, static_cast<uint64_t>(difficulty_blocks_count));
    if (offset == 0)
      ++offset;

    timestamps.clear();
    difficulties.clear();
    if (height > offset)
    {
      timestamps.reserve(height - offset);
      difficulties.reserve(height - offset);
    }
    for (; offset < height; offset++)
    {
      timestamps.push_back(m_db->get_block_timestamp(offset));
      difficulties.push_back(m_db->get_block_cumulative_difficulty(offset));
    }

    m_timestamps_and_difficulties_height = height;
    m_timestamps = timestamps;
    m_difficulties = difficulties;
  }

  if(version >= 16) {
    return next_difficulty_v16(timestamps, difficulties);
  } else if(version >= 10) {
    return next_difficulty_lwma_4(timestamps, difficulties);
  } else if(version >= 7) {
    return next_difficulty_lwma(timestamps, difficulties, version);
  } else {
    return next_difficulty(timestamps, difficulties, DIFFICULTY_TARGET_V2);
  }

}
//------------------------------------------------------------------
// This function removes blocks from the blockchain until it gets to the
// position where the blockchain switch started and then re-adds the blocks
// that had been removed.
bool Blockchain::rollback_blockchain_switching(const std::list<block_and_checkpoint>& original_chain, uint64_t rollback_height)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  // fail if rollback_height passed is too high
  if (rollback_height > m_db->height())
  {
    return true;
  }

  m_timestamps_and_difficulties_height = 0;

  // remove blocks from blockchain until we get back to where we should be.
  while (m_db->height() != rollback_height)
  {
    pop_block_from_blockchain();
  }

  // Revert all changes made from switching to alt_chain before adding the original chain back in
  for(BlockchainDetachedHook* hook : m_blockchain_detached_hooks)
    hook->blockchain_detached(rollback_height);

  // make sure the hard fork object updates its current version
  m_hardfork->reorganize_from_chain_height(rollback_height);

  //return back original chain
  for (auto& entry : original_chain)
  {
    block_verification_context bvc{};
    bool r = handle_block_to_main_chain(entry.block, cryptonote::get_block_hash(entry.block), bvc, entry.checkpointed ? &entry.checkpoint : nullptr);
    CHECK_AND_ASSERT_MES(r && bvc.m_added_to_main_chain, false, "PANIC! failed to add (again) block while chain switching during the rollback!");
  }

  m_hardfork->reorganize_from_chain_height(rollback_height);
  MINFO("Rollback to height " << rollback_height << " was successful.");
  if (original_chain.size())
  {
    MINFO("Restoration to previous blockchain successful as well.");
  }
  return true;
}
//------------------------------------------------------------------
// This function attempts to switch to an alternate chain, returning
// boolean based on success therein.
bool Blockchain::switch_to_alternative_blockchain(const std::list<block_extended_info> &alt_chain, bool keep_disconnected_chain)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  m_timestamps_and_difficulties_height = 0;

  // if empty alt chain passed (not sure how that could happen), return false
  CHECK_AND_ASSERT_MES(alt_chain.size(), false, "switch_to_alternative_blockchain: empty chain passed");

  // verify that main chain has front of alt chain's parent block
  if (!m_db->block_exists(alt_chain.front().bl.prev_id))
  {
    LOG_ERROR("Attempting to move to an alternate chain, but it doesn't appear to connect to the main chain!");
    return false;
  }

  // pop blocks from the blockchain until the top block is the parent
  // of the front block of the alt chain.
  std::list<block_and_checkpoint> disconnected_chain;
  while (m_db->top_block_hash() != alt_chain.front().bl.prev_id)
  {
    block_and_checkpoint entry = {};
    entry.block                = pop_block_from_blockchain();
    entry.checkpointed         = m_db->get_block_checkpoint(cryptonote::get_block_height(entry.block), entry.checkpoint);
    disconnected_chain.push_front(entry);
  }

  auto split_height = m_db->height();

  for(BlockchainDetachedHook* hook : m_blockchain_detached_hooks)
    hook->blockchain_detached(split_height);

  //connecting new alternative chain
  for(auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++)
  {
    const auto &bei = *alt_ch_iter;
    block_verification_context bvc{};

    // add block to main chain
    bool r = handle_block_to_main_chain(bei.bl, cryptonote::get_block_hash(bei.bl), bvc, bei.checkpointed ? &bei.checkpoint : nullptr);

    // if adding block to main chain failed, rollback to previous state and
    // return false
    if(!r || !bvc.m_added_to_main_chain)
    {
      MERROR("Failed to switch to alternative blockchain");

      // rollback_blockchain_switching should be moved to two different
      // functions: rollback and apply_chain, but for now we pretend it is
      // just the latter (because the rollback was done above).
      rollback_blockchain_switching(disconnected_chain, split_height);

      const crypto::hash blkid = cryptonote::get_block_hash(bei.bl);
      add_block_as_invalid(bei.bl);
      MERROR("Block was inserted as invalid while connecting new alt chain, block_id: " << blkid);
      m_db->remove_alt_block(blkid);
      alt_ch_iter++;

      for(auto alt_ch_to_orph_iter = alt_ch_iter; alt_ch_to_orph_iter != alt_chain.end(); )
      {
        const auto &bei = *alt_ch_to_orph_iter++;
        add_block_as_invalid(bei.bl);
        m_db->remove_alt_block(blkid);
      }
      return false;
    }
  }

  if (keep_disconnected_chain) // pushing old chain as alternative chain
  {
    for (auto& old_ch_ent : disconnected_chain)
    {
      block_verification_context bvc{};
      bool r = handle_alternative_block(old_ch_ent.block, cryptonote::get_block_hash(old_ch_ent.block), bvc, old_ch_ent.checkpointed ? &old_ch_ent.checkpoint : nullptr);
      if (!r)
      {
        MERROR("Failed to push ex-main chain blocks to alternative chain ");
        // previously this would fail the blockchain switching, but I don't
        // think this is bad enough to warrant that.
      }
    }
  }

  // removing alt_chain entries from alternative chains container
  for (const auto &bei: alt_chain)
  {
    m_db->remove_alt_block(cryptonote::get_block_hash(bei.bl));
  }

  m_hardfork->reorganize_from_chain_height(split_height);

  std::shared_ptr<tools::Notify> reorg_notify = m_reorg_notify;
  if (reorg_notify)
    reorg_notify->notify("%s", std::to_string(split_height).c_str(), "%h", std::to_string(m_db->height()).c_str(),
        "%n", std::to_string(m_db->height() - split_height).c_str());

  const uint64_t new_height = m_db->height();
  const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(new_height));

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
    rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());

  std::shared_ptr<tools::Notify> block_notify = m_block_notify;
  if (block_notify)
    for (const auto &bei : alt_chain)
      block_notify->notify("%s", epee::string_tools::pod_to_hex(get_block_hash(bei.bl)).c_str(), NULL);

  MGINFO_GREEN("REORGANIZE SUCCESS! on height: " << split_height << ", new blockchain size: " << m_db->height());
  return true;
}
//------------------------------------------------------------------
// This function calculates the difficulty target for the block being added to
// an alternate chain.
difficulty_type Blockchain::get_next_difficulty_for_alternative_chain(const std::list<block_extended_info>& alt_chain, uint64_t alt_block_height) const
{
  if (m_fixed_difficulty)
  {
    return m_db->height() ? m_fixed_difficulty : 1;
  }

  LOG_PRINT_L3("Blockchain::" << __func__);
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> cumulative_difficulties;
  uint8_t version = get_current_hard_fork_version();
  size_t difficulty_blocks_count = get_difficulty_blocks_count(version);
  uint64_t height = m_db->height();

  // if the alt chain isn't long enough to calculate the difficulty target
  // based on its blocks alone, need to get more blocks from the main chain
  if(alt_chain.size() < difficulty_blocks_count)
  {
    std::unique_lock lock{*this};

    // Figure out start and stop offsets for main chain blocks
    size_t main_chain_stop_offset = alt_chain.size() ? alt_chain.front().height : alt_block_height;
    size_t main_chain_count = difficulty_blocks_count - std::min(static_cast<size_t>(difficulty_blocks_count), alt_chain.size());
    main_chain_count = std::min(main_chain_count, main_chain_stop_offset);
    size_t main_chain_start_offset = main_chain_stop_offset - main_chain_count;

    if(!main_chain_start_offset)
      ++main_chain_start_offset; //skip genesis block

    // get difficulties and timestamps from relevant main chain blocks
    for(; main_chain_start_offset < main_chain_stop_offset; ++main_chain_start_offset)
    {
      timestamps.push_back(m_db->get_block_timestamp(main_chain_start_offset));
      cumulative_difficulties.push_back(m_db->get_block_cumulative_difficulty(main_chain_start_offset));
    }

    // make sure we haven't accidentally grabbed too many blocks...maybe don't need this check?
    CHECK_AND_ASSERT_MES((alt_chain.size() + timestamps.size()) <= difficulty_blocks_count, false, "Internal error, alt_chain.size()[" << alt_chain.size() << "] + vtimestampsec.size()[" << timestamps.size() << "] NOT <= DIFFICULTY_WINDOW_V2[]" << DIFFICULTY_BLOCKS_COUNT_V2);

    for (const auto &bei : alt_chain)
    {
      timestamps.push_back(bei.bl.timestamp);
      cumulative_difficulties.push_back(bei.cumulative_difficulty);
    }
  }
  // if the alt chain is long enough for the difficulty calc, grab difficulties
  // and timestamps from it alone
  else
  {
    timestamps.resize(static_cast<size_t>(difficulty_blocks_count));
    cumulative_difficulties.resize(static_cast<size_t>(difficulty_blocks_count));
    size_t count = 0;
    size_t max_i = timestamps.size()-1;
    // get difficulties and timestamps from most recent blocks in alt chain
    for (const auto& bei: boost::adaptors::reverse(alt_chain))
    {
      timestamps[max_i - count] = bei.bl.timestamp;
      cumulative_difficulties[max_i - count] = bei.cumulative_difficulty;
      count++;
      if(count >= difficulty_blocks_count)
        break;
    }
  }

  // FIXME: This will fail if fork activation heights are subject to voting
  if(version >= 16) {
    return next_difficulty_v16(timestamps, cumulative_difficulties);
  } else if(version >= 10) {
    return next_difficulty_lwma_4(timestamps, cumulative_difficulties);
  } else if(version >= 7) {
    return next_difficulty_lwma(timestamps, cumulative_difficulties, version);
  } else {
    return next_difficulty(timestamps, cumulative_difficulties, DIFFICULTY_TARGET_V2);
  }

}
//------------------------------------------------------------------
// This function does a sanity check on basic things that all miner
// transactions have in common, such as:
//   one input, of type txin_gen, with height set to the block's height
//   correct miner tx unlock time
//   a non-overflowing tx amount (dubious necessity on this check)
bool Blockchain::prevalidate_miner_transaction(const block& b, uint64_t height, uint8_t hard_fork_version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CHECK_AND_ASSERT_MES(b.miner_tx.vin.size() == 1, false, "coinbase transaction in the block has no inputs");
  CHECK_AND_ASSERT_MES(b.miner_tx.vin[0].type() == typeid(txin_gen), false, "coinbase transaction in the block has the wrong type");
  CHECK_AND_ASSERT_MES(b.miner_tx.version > txversion::v2 || hard_fork_version < 16, false, "Invalid coinbase transaction version");
  if(hard_fork_version >= 16 && b.miner_tx.version >= txversion::v3)
  {
    CHECK_AND_ASSERT_MES(b.miner_tx.rct_signatures.type == rct::RCTTypeNull, false, "RingCT signatures not allowed in coinbase transactions");
  }
  if(boost::get<txin_gen>(b.miner_tx.vin[0]).height != height)
  {
    MWARNING("The miner transaction in block has invalid height: " << boost::get<txin_gen>(b.miner_tx.vin[0]).height << ", expected: " << height);
    return false;
  }
  MDEBUG("Miner tx hash: " << get_transaction_hash(b.miner_tx));
  CHECK_AND_ASSERT_MES(b.miner_tx.unlock_time == height + config::blockchain_settings::ARQMA_BLOCK_UNLOCK_CONFIRMATIONS, false, "coinbase transaction transaction has the wrong unlock time=" << b.miner_tx.unlock_time << ", expected " << height + config::blockchain_settings::ARQMA_BLOCK_UNLOCK_CONFIRMATIONS);

  //check outs overflow
  //NOTE: not entirely sure this is necessary, given that this function is
  //      designed simply to make sure the total amount for a transaction
  //      does not overflow a uint64_t, and this transaction *is* a uint64_t...
  if(!check_outs_overflow(b.miner_tx))
  {
    MERROR("miner transaction has money overflow in block " << get_block_hash(b));
    return false;
  }

  return true;
}
//------------------------------------------------------------------
// This function validates the miner transaction reward
bool Blockchain::validate_miner_transaction(const block& b, size_t cumulative_block_weight, uint64_t fee, uint64_t& base_reward, uint64_t already_generated_coins, uint8_t hard_fork_version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  //validate reward
  uint64_t money_in_use = 0;
  for (auto& o: b.miner_tx.vout)
    money_in_use += o.amount;

  if(b.miner_tx.vout.size() == 0)
  {
    MERROR_VER("Miner_TX has no outputs");
    return false;
  }

  uint64_t height = cryptonote::get_block_height(b);
  std::vector<uint64_t> last_blocks_weights;
  get_last_n_blocks_weights(last_blocks_weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW);

  arqma_block_reward_context block_reward_context = {};
  block_reward_context.fee = fee;
  block_reward_context.height = height;

  block_reward_parts reward_parts;
  if(!get_arqma_block_reward(epee::misc_utils::median(last_blocks_weights), cumulative_block_weight, already_generated_coins, hard_fork_version, reward_parts, block_reward_context))
  {
    MERROR_VER("block weight " << cumulative_block_weight << " is bigger than allowed for this blockchain");
    return false;
  }

  for(ValidateMinerTxHook* hook : m_validate_miner_tx_hooks)
  {
    if(!hook->validate_miner_tx(b.prev_id, b.miner_tx, height, hard_fork_version, reward_parts))
      return false;
  }

  if(hard_fork_version >= 16)
  {
    size_t vout_end = b.miner_tx.vout.size();

    if(b.miner_tx.vout[vout_end - 3].amount != reward_parts.gov)
    {
      MERROR("Governance reward amount incorrect. Should be: " << print_money(reward_parts.gov) << ", While is: " << print_money(b.miner_tx.vout[vout_end - 3].amount));
      return false;
    }

    if(!validate_gov_reward_key(height, *cryptonote::get_config(m_nettype, hard_fork_version).GOV_WALLET_ADDRESS, vout_end - 3, boost::get<cryptonote::txout_to_key>(b.miner_tx.vout[vout_end - 3].target).key, m_nettype))
    {
      MERROR("Governance reward public key incorrect.");
      return false;
    }

    if(b.miner_tx.vout[vout_end - 2].amount != reward_parts.dev)
    {
      MERROR_VER("Dev_Fund reward amount incorrect. Should be: " << print_money(reward_parts.dev) << ", While is: " << print_money(b.miner_tx.vout[vout_end - 2].amount));
      return false;
    }

    if(!validate_dev_reward_key(height, *cryptonote::get_config(m_nettype, hard_fork_version).DEV_WALLET_ADDRESS, vout_end - 2, boost::get<cryptonote::txout_to_key>(b.miner_tx.vout[vout_end - 2].target).key, m_nettype))
    {
      MERROR_VER("Dev_Fund reward public key incorrect.");
      return false;
    }

    if (b.miner_tx.vout[vout_end - 1].amount != reward_parts.net)
    {
      MERROR_VER("Net_fund reward amount incorrect. Should be: " << print_money(reward_parts.net) << ", While is: " << print_money(b.miner_tx.vout[vout_end - 1].amount));
      return false;
    }

    if (!validate_net_reward_key(height, *cryptonote::get_config(m_nettype, hard_fork_version).NET_WALLET_ADDRESS, vout_end - 1, boost::get<cryptonote::txout_to_key>(b.miner_tx.vout[vout_end - 1].target).key, m_nettype))
    {
      MERROR_VER("Net_fund reward public key incorrect.");
      return false;
    }

    if(b.miner_tx.tx_type != txtype::standard)
    {
      MERROR("Coinbase transaction invalid type");
      return false;
    }

    txversion min_version = transaction::get_max_version_for_hf(hard_fork_version);
    txversion max_version = transaction::get_min_version_for_hf(hard_fork_version);

    if(b.miner_tx.version < min_version || b.miner_tx.version > max_version)
    {
      MERROR_VER("Coinbase transaction invalid version: " << b.miner_tx.version << " for hardfork: " << hard_fork_version);
      return false;
    }
  }

  base_reward = reward_parts.adjusted_base_reward;
  // Due to errors in floating point or rounding which happen quite often we will add +1 atomic to workaround errors
  uint64_t max_base_reward = reward_parts.base_miner + reward_parts.net + reward_parts.gov + reward_parts.dev + reward_parts.service_node_paid + 1;
  uint64_t max_money_in_use = max_base_reward + fee;
  if(money_in_use > max_money_in_use)
  {
    MERROR_VER("Coinbase Transaction spends too much money ( " << print_money(money_in_use) << " ). Maximum block reward is " << print_money(max_money_in_use) << " where: ( " << print_money(max_base_reward) << " base + " << print_money(fee) << " fees)");
    return false;
  }

  CHECK_AND_ASSERT_MES(money_in_use >= fee, false, "Base Reward calculation bug");
  base_reward = money_in_use - fee;

/*
  base_reward = reward_parts.adjusted_base_reward;
  if(base_reward + fee < money_in_use)
  {
    MERROR_VER("Coinbase transaction spend too much money: ( " << print_money(money_in_use) << " ). <-> Block Reward is: " << print_money(base_reward + fee) << " ( " << print_money(base_reward) << " + " << print_money(fee) << " )");
    return false;
  }

  CHECK_AND_ASSERT_MES(money_in_use - fee <= base_reward, false, "base reward calculation bug");
  base_reward = money_in_use - fee;
*/
  return true;
}
//------------------------------------------------------------------
// get the block weights of the last <count> blocks, and return by reference <sz>.
void Blockchain::get_last_n_blocks_weights(std::vector<uint64_t>& weights, size_t count) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  auto h = m_db->height();

  // this function is meaningless for an empty blockchain...granted it should never be empty
  if(h == 0)
    return;

  // add weight of last <count> blocks to vector <weights> (or less, if blockchain size < count)
  size_t start_offset = h - std::min<size_t>(h, count);
  weights = m_db->get_block_weights(start_offset, count);
}
//------------------------------------------------------------------
uint64_t Blockchain::get_long_term_block_weight_median(uint64_t start_height, size_t count) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  PERF_TIMER(get_long_term_block_weights);

  CHECK_AND_ASSERT_THROW_MES(count > 0, "count == 0");

  bool cached = false;
  uint64_t blockchain_height = m_db->height();
  uint64_t tip_height = start_height + count - 1;
  crypto::hash tip_hash = crypto::null_hash;
  if(tip_height < blockchain_height && count == (size_t)m_long_term_block_weights_cache_rolling_median.size())
  {
    tip_hash = m_db->get_block_hash_from_height(tip_height);
    cached = tip_hash == m_long_term_block_weights_cache_tip_hash;
  }

  if(cached)
  {
    MTRACE("Requesting " << count << " from " << start_height << ", cached");
    return m_long_term_block_weights_cache_rolling_median.median();
  }

  if(tip_height > 0 && count == (size_t)m_long_term_block_weights_cache_rolling_median.size() && tip_height < blockchain_height)
  {
    crypto::hash old_tip_hash = m_db->get_block_hash_from_height(tip_height - 1);
    if(old_tip_hash == m_long_term_block_weights_cache_tip_hash)
    {
      MTRACE("requesting " << count << " from " << start_height << ", incremental");
      m_long_term_block_weights_cache_tip_hash = tip_hash;
      m_long_term_block_weights_cache_rolling_median.insert(m_db->get_block_long_term_weight(tip_height));
      return m_long_term_block_weights_cache_rolling_median.median();
    }
  }

  MTRACE("requesting " << count << " from " << start_height << ", uncached");
  std::vector<uint64_t> weights = m_db->get_long_term_block_weights(start_height, count);
  m_long_term_block_weights_cache_tip_hash = tip_hash;
  m_long_term_block_weights_cache_rolling_median.clear();
  for(uint64_t w : weights)
    m_long_term_block_weights_cache_rolling_median.insert(w);
  return m_long_term_block_weights_cache_rolling_median.median();
}
//------------------------------------------------------------------
uint64_t Blockchain::get_current_cumulative_block_weight_limit() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  return m_current_block_cumul_weight_limit;
}
//------------------------------------------------------------------
uint64_t Blockchain::get_current_cumulative_block_weight_median() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  return m_current_block_cumul_weight_median;
}
//------------------------------------------------------------------
//TODO: This function only needed minor modification to work with BlockchainDB,
//      and *works*.  As such, to reduce the number of things that might break
//      in moving to BlockchainDB, this function will remain otherwise
//      unchanged for the time being.
//
// This function makes a new block for a miner to mine the hash for
bool Blockchain::create_block_template(block& b, const crypto::hash *from_block, const account_public_address& miner_address, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  size_t median_weight;
  uint64_t already_generated_coins;
  uint64_t pool_cookie;

  seed_hash = crypto::null_hash;

  auto lock = tools::unique_locks(m_tx_pool, *this);
  if (m_btc_valid && !from_block) {
    // The pool cookie is atomic. The lack of locking is OK, as if it changes
    // just as we compare it, we'll just use a slightly old template, but
    // this would be the case anyway if we'd lock, and the change happened
    // just after the block template was created
    if (!memcmp(&miner_address, &m_btc_address, sizeof(cryptonote::account_public_address)) && m_btc_nonce == ex_nonce && m_btc_pool_cookie == m_tx_pool.cookie() && m_btc.prev_id == get_tail_id()) {
      MDEBUG("Using cached template");
      m_btc.timestamp = time(NULL); // update timestamp unconditionally
      b = m_btc;
      diffic = m_btc_difficulty;
      height = m_btc_height;
      expected_reward = m_btc_expected_reward;
      seed_height = m_btc_seed_height;
      seed_hash = m_btc_seed_hash;
      return true;
    }
    MDEBUG("Not using cached template: address " << (!memcmp(&miner_address, &m_btc_address, sizeof(cryptonote::account_public_address))) << ", nonce " << (m_btc_nonce == ex_nonce) << ", cookie " << (m_btc_pool_cookie == m_tx_pool.cookie()) << ", from_block " << (!from_block));
    invalidate_block_template_cache();
  }

  if (from_block)
  {
    //build alternative subchain, front -> mainchain, back -> alternative head
    //block is not related with head of main chain
    //first of all - look in alternative chains container
    alt_block_data_t prev_data;
    bool parent_in_alt = m_db->get_alt_block(*from_block, &prev_data, NULL, nullptr);
    bool parent_in_main = m_db->block_exists(*from_block);
    if(!parent_in_alt && !parent_in_main)
    {
      MERROR("Unknown from block");
      return false;
    }

    //we have new block in alternative chain
    std::list<block_extended_info> alt_chain;
    block_verification_context bvc{};
    std::vector<uint64_t> timestamps;
    if (!build_alt_chain(*from_block, alt_chain, timestamps, bvc, nullptr, nullptr))
      return false;

    if (parent_in_main)
    {
      cryptonote::block prev_block;
      CHECK_AND_ASSERT_MES(get_block_by_hash(*from_block, prev_block), false, "From block not found"); // TODO
      uint64_t from_block_height = cryptonote::get_block_height(prev_block);
      height = from_block_height + 1;
      if(m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
      {
        uint64_t seed_height, next_height;
        crypto::rx_seedheights(height, &seed_height, &next_height);
        seed_hash = get_block_id_by_height(seed_height);
      }
    }
    else
    {
      height = alt_chain.back().height + 1;
      uint64_t next_height, seed_height;
      crypto::rx_seedheights(height, &seed_height, &next_height);

      if(alt_chain.size() && alt_chain.front().height <= seed_height)
      {
        for(auto it=alt_chain.begin(); it != alt_chain.end(); it++)
        {
          if(it->height == seed_height+1)
          {
            seed_hash = it->bl.prev_id;
            break;
          }
        }
      }
      else
      {
        seed_hash = get_block_id_by_height(seed_height);
      }
    }
    b.major_version = m_hardfork->get_ideal_version(height);
    b.minor_version = m_hardfork->get_ideal_version();
    b.prev_id = *from_block;

    // cheat and use the weight of the block we start from, virtually certain to be acceptable
    // and use 1.9 times rather than 2 times so we're even more sure
    if (parent_in_main)
    {
      median_weight = m_db->get_block_weight(height - 1);
      already_generated_coins = m_db->get_block_already_generated_coins(height - 1);
    }
    else
    {
      median_weight = prev_data.cumulative_weight - prev_data.cumulative_weight / 20;
      already_generated_coins = alt_chain.back().already_generated_coins;
    }

    // FIXME: consider moving away from block_extended_info at some point
    block_extended_info bei{};
    bei.bl = b;
    bei.height = alt_chain.size() ? prev_data.height + 1 : m_db->get_block_height(*from_block) + 1;

    diffic = get_next_difficulty_for_alternative_chain(alt_chain, bei.height);
  }
  else
  {
    height = m_db->height();
    b.major_version = m_hardfork->get_current_version();
    b.minor_version = m_hardfork->get_ideal_version();
    b.prev_id = get_tail_id();
    median_weight = m_current_block_cumul_weight_limit / 2;
    diffic = get_difficulty_for_next_block();
    already_generated_coins = m_db->get_block_already_generated_coins(height - 1);
    if(m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
    {
      uint64_t next_height, seed_height;
      crypto::rx_seedheights(height, &seed_height, &next_height);
      seed_hash = get_block_id_by_height(seed_height);
    }
  }

  b.timestamp = time(NULL);

  uint8_t hf_version = m_hardfork->get_current_version();
  uint64_t blockchain_timestamp_check_window = hf_version > 9 ? BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V11 : BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V9;

  if(m_db->height() >= blockchain_timestamp_check_window)
  {
    std::vector<uint64_t> timestamps;
    auto h = m_db->height();

    for(size_t offset = h - blockchain_timestamp_check_window; offset < h; ++offset)
    {
      timestamps.push_back(m_db->get_block_timestamp(offset));
    }
    uint64_t median_ts = epee::misc_utils::median(timestamps);
    if (b.timestamp < median_ts)
    {
      b.timestamp = median_ts;
    }
  }

  CHECK_AND_ASSERT_MES(diffic, false, "difficulty overhead.");

  size_t txs_weight;
  uint64_t fee;
  if (!m_tx_pool.fill_block_template(b, median_weight, already_generated_coins, txs_weight, fee, expected_reward, b.major_version, m_db->height()))
  {
    return false;
  }
  pool_cookie = m_tx_pool.cookie();

  /*
   two-phase miner transaction generation: we don't know exact block weight until we prepare block, but we don't know reward until we know
   block weight, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block weight
   */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob weight
  uint8_t hard_fork_version = b.major_version;
  arqma_miner_tx_context miner_tx_context(m_nettype, m_service_node_list.get_block_winner());

  bool r = construct_miner_tx(this, height, median_weight, already_generated_coins, txs_weight, fee, miner_address, b.miner_tx, ex_nonce, hard_fork_version, miner_tx_context);
  CHECK_AND_ASSERT_MES(r, false, "Failed to construct miner tx, first chance");
  size_t cumulative_weight = txs_weight + get_transaction_weight(b.miner_tx);
  for (size_t try_count = 0; try_count != 10; ++try_count)
  {
    r = construct_miner_tx(this, height, median_weight, already_generated_coins, cumulative_weight, fee, miner_address, b.miner_tx, ex_nonce, hard_fork_version, miner_tx_context);
    CHECK_AND_ASSERT_MES(r, false, "Failed to construct miner tx, second chance");
    size_t coinbase_weight = get_transaction_weight(b.miner_tx);
    if (coinbase_weight > cumulative_weight - txs_weight)
    {
      cumulative_weight = txs_weight + coinbase_weight;
      continue;
    }

    if (coinbase_weight < cumulative_weight - txs_weight)
    {
      size_t delta = cumulative_weight - txs_weight - coinbase_weight;
      b.miner_tx.extra.insert(b.miner_tx.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_weight != txs_weight + get_transaction_weight(b.miner_tx))
      {
        CHECK_AND_ASSERT_MES(cumulative_weight + 1 == txs_weight + get_transaction_weight(b.miner_tx), false, "unexpected case: cumulative_weight=" << cumulative_weight << " + 1 is not equal txs_cumulative_weight=" << txs_weight << " + get_transaction_weight(b.miner_tx)=" << get_transaction_weight(b.miner_tx));
        b.miner_tx.extra.resize(b.miner_tx.extra.size() - 1);
        if (cumulative_weight != txs_weight + get_transaction_weight(b.miner_tx))
        {
          //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_weight
          MDEBUG("Miner tx creation has no luck with delta_extra size = " << delta << " and " << delta - 1);
          cumulative_weight += delta - 1;
          continue;
        }
        MDEBUG("Setting extra for block: " << b.miner_tx.extra.size() << ", try_count=" << try_count);
      }
    }
    CHECK_AND_ASSERT_MES(cumulative_weight == txs_weight + get_transaction_weight(b.miner_tx), false, "unexpected case: cumulative_weight=" << cumulative_weight << " is not equal txs_cumulative_weight=" << txs_weight << " + get_transaction_weight(b.miner_tx)=" << get_transaction_weight(b.miner_tx));
    if (!from_block)
      cache_block_template(b, miner_address, ex_nonce, diffic, height, expected_reward, seed_height, seed_hash, pool_cookie);
    return true;
  }
  LOG_ERROR("Failed to create_block_template with " << 10 << " tries");
  return false;
}
//------------------------------------------------------------------
bool Blockchain::create_block_template(block& b, const account_public_address& miner_address, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
{
  return create_block_template(b, NULL, miner_address, diffic, height, expected_reward, ex_nonce, seed_height, seed_hash);
}
//------------------------------------------------------------------
// for an alternate chain, get the timestamps from the main chain to complete
// the needed number of timestamps for the BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW.
bool Blockchain::complete_timestamps_vector(uint64_t start_top_height, std::vector<uint64_t>& timestamps) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  if(timestamps.size() >= BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V11)
    return true;

  std::unique_lock lock{*this};
  size_t need_elements = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V11 - timestamps.size();
  CHECK_AND_ASSERT_MES(start_top_height < m_db->height(), false, "internal error: passed start_height not < " << " m_db->height() -- " << start_top_height << " >= " << m_db->height());
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements : 0;
  timestamps.reserve(timestamps.size() + start_top_height - stop_offset);
  while (start_top_height != stop_offset)
  {
    timestamps.push_back(m_db->get_block_timestamp(start_top_height));
    --start_top_height;
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::build_alt_chain(const crypto::hash &prev_id, std::list<block_extended_info> &alt_chain, std::vector<uint64_t> &timestamps, block_verification_context& bvc, int *num_alt_checkpoints, int *num_checkpoints) const
{
  //build alternative subchain, front -> mainchain, back -> alternative head
  cryptonote::alt_block_data_t data;
  cryptonote::blobdata blob;
  timestamps.clear();

  int alt_checkpoint_count = 0;
  int checkpoint_count = 0;
  crypto::hash prev_hash = crypto::null_hash;
  block_extended_info bei = {};
  blobdata checkpoint_blob;
  for (bool found = m_db->get_alt_block(prev_id, &data, &blob, &checkpoint_blob);
       found;
       found = m_db->get_alt_block(prev_hash, &data, &blob, &checkpoint_blob))
  {
    CHECK_AND_ASSERT_MES(cryptonote::parse_and_validate_block_from_blob(blob, bei.bl), false, "Failed to parse Alt Block");

    if (data.checkpointed)
    {
      CHECK_AND_ASSERT_MES(t_serializable_object_from_blob(bei.checkpoint, checkpoint_blob), false, "Failed to parse alt checkpoint from blob");
      alt_checkpoint_count++;
    }

    bool height_is_checkpointed = false;
    bool alt_block_matches_checkpoint = m_checkpoints.check_block(data.height, get_block_hash(bei.bl), &height_is_checkpointed, nullptr);

    if (height_is_checkpointed)
    {
      if (alt_block_matches_checkpoint)
      {
        if (!data.checkpointed)
        {
          data.checkpointed = true;
          CHECK_AND_ASSERT_MES(get_checkpoint(data.height, bei.checkpoint), false, "Unexpected failure to retrieve checkpoint after checking it existed");
          checkpoint_count++;
        }
      }
      else
        checkpoint_count++;
    }

    bei.height = data.height;
    bei.block_cumulative_weight = data.cumulative_weight;
    bei.cumulative_difficulty = data.cumulative_difficulty;
    bei.already_generated_coins = data.already_generated_coins;
    bei.checkpointed            = data.checkpointed;

    prev_hash = bei.bl.prev_id;
    timestamps.push_back(bei.bl.timestamp);
    alt_chain.push_front(std::move(bei));
    bei = {};
  }

  if (num_alt_checkpoints) *num_alt_checkpoints = alt_checkpoint_count;
  if (num_checkpoints) *num_checkpoints = checkpoint_count;

  // if block to be added connects to known blocks that aren't part of the
  // main chain -- that is, if we're adding on to an alternate chain
  if(!alt_chain.empty())
  {
    bool failed = false;
    // make sure alt chain doesn't somehow start past the end of the main chain
    if (m_db->height() < alt_chain.front().height)
    {
      LOG_PRINT_L1("main blockchain wrong height: " << m_db->height() << ", alt_chain: " << alt_chain.front().height);
      failed = true;
    }

    // make sure that the blockchain contains the block that should connect
    // this alternate chain with it.
    if (!failed && !m_db->block_exists(alt_chain.front().bl.prev_id))
    {
      LOG_PRINT_L1("alternate chain does not appear to connect to main chain...: " << alt_chain.front().bl.prev_id);
      failed = true;
    }

    // make sure block connects correctly to the main chain
    auto h = m_db->get_block_hash_from_height(alt_chain.front().height - 1);
    if (!failed && h != alt_chain.front().bl.prev_id)
    {
      LOG_PRINT_L1("alternative chain has wrong connection to main chain: " << h << ", mismatched with: " << alt_chain.front().bl.prev_id);
      failed = true;
    }

    if (failed)
    {
      for (auto const &bei : alt_chain)
        m_db->remove_alt_block(cryptonote::get_block_hash(bei.bl));

      return false;
    }

    complete_timestamps_vector(m_db->get_block_height(alt_chain.front().bl.prev_id), timestamps);
  }
  // if block not associated with known alternate chain
  else
  {
    // if block parent is not part of main chain or an alternate chain,
    // we ignore it
    bool parent_in_main = m_db->block_exists(prev_id);
    CHECK_AND_ASSERT_MES(parent_in_main, false, "internal error: broken imperative condition: parent_in_main");

    complete_timestamps_vector(m_db->get_block_height(prev_id), timestamps);
  }

  return true;
}
//------------------------------------------------------------------
// If a block is to be added and its parent block is not the current
// main chain top block, then we need to see if we know about its parent block.
// If its parent block is part of a known forked chain, then we need to see
// if that chain is long enough to become the main chain and re-org accordingly
// if so.  If not, we need to hang on to the block in case it becomes part of
// a long forked chain eventually.
bool Blockchain::handle_alternative_block(const block& b, const crypto::hash& id, block_verification_context& bvc, checkpoint_t const *checkpoint)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  m_timestamps_and_difficulties_height = 0;
  uint64_t const block_height = get_block_height(b);
  if(0 == block_height)
  {
    MERROR_VER("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative), but miner tx says height is 0.");
    bvc.m_verification_failed = true;
    return false;
  }

  bool service_node_checkpoint = false;
  if (!m_checkpoints.is_alternative_block_allowed(get_current_blockchain_height(), block_height, &service_node_checkpoint))
  {
    if (!service_node_checkpoint || b.major_version >= cryptonote::network_version_16)
    {
      MERROR_VER("Block with id: " << id << std::endl << " can NOT be accepted for alternative chain, block_height: "
                 << block_height << std::endl << " blockchain height: " << get_current_blockchain_height());
      bvc.m_verification_failed = true;
      return false;
    }
  }

  // this is a cheap test
  const uint8_t hard_fork_version = m_hardfork->get_ideal_version(block_height);
  if (!m_hardfork->check_for_height(b, block_height))
  {
    LOG_PRINT_L1("Block with id: " << id << std::endl << "has old version for height " << block_height);
    bvc.m_verification_failed = true;
    return false;
  }

  //block is not related with head of main chain
  //first of all - look in alternative chains container
  uint64_t const curr_blockchain_height = get_current_blockchain_height();
  alt_block_data_t prev_data;
  bool parent_in_alt = m_db->get_alt_block(b.prev_id, &prev_data, NULL, nullptr);
  bool parent_in_main = m_db->block_exists(b.prev_id);
  if(parent_in_alt || parent_in_main)
  {
    //we have new block in alternative chain
    std::list<block_extended_info> alt_chain;
    std::vector<uint64_t> timestamps;
    int num_checkpoints_on_alt_chain = 0;
    int num_checkpoints_on_chain = 0;
    if (!build_alt_chain(b.prev_id, alt_chain, timestamps, bvc, &num_checkpoints_on_alt_chain, &num_checkpoints_on_chain))
      return false;

    // verify that the block's timestamp is within the acceptable range
    // (not earlier than the median of the last X blocks)
    if(!check_block_timestamp(timestamps, b))
    {
      MERROR_VER("Block with id: " << id << std::endl << " for alternative chain, has invalid timestamp: " << b.timestamp);
      bvc.m_verification_failed = true;
      return false;
    }

    // Check the block's hash against the difficulty target for its alt chain
    difficulty_type current_diff = get_next_difficulty_for_alternative_chain(alt_chain, block_height);
    CHECK_AND_ASSERT_MES(current_diff, false, "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!");
    crypto::hash proof_of_work;
    memset(proof_of_work.data, 0xff, sizeof(proof_of_work.data));
    if(b.major_version >= RX_BLOCK_VERSION)
    {
      crypto::hash seedhash = null_hash;
      uint64_t seedheight = rx_seedheight(block_height);
      // seedblock is on the alt chain somewhere
      if(alt_chain.size() && alt_chain.front().height <= seedheight)
      {
        for (auto it=alt_chain.begin(); it != alt_chain.end(); it++)
        {
          if(it->height == seedheight+1)
          {
            seedhash = it->bl.prev_id;
            break;
          }
        }
      }
      else
      {
        seedhash = get_block_id_by_height(seedheight);
      }
      get_altblock_longhash(b, proof_of_work, seedhash);
    }
    else
    {
      get_block_longhash(this, b, proof_of_work, block_height, 0);
    }
    if(!check_hash(proof_of_work, current_diff))
    {
      MERROR_VER("Block with id: " << id << std::endl << " for alternative chain, does not have enough proof of work: " << proof_of_work << std::endl << " expected difficulty: " << current_diff);
      bvc.m_verification_failed = true;
      return false;
    }

    if(!prevalidate_miner_transaction(b, block_height, hard_fork_version))
    {
      MERROR_VER("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative) has incorrect miner transaction.");
      bvc.m_verification_failed = true;
      return false;
    }

    // FIXME:
    // this brings up an interesting point: consider allowing to get block
    // difficulty both by height OR by hash, not just height.
    cryptonote::alt_block_data_t alt_data = {};
    difficulty_type main_chain_cumulative_difficulty = m_db->get_block_cumulative_difficulty(m_db->height() - 1);
    if(alt_chain.size())
    {
      alt_data.cumulative_difficulty = prev_data.cumulative_difficulty;
    }
    else
    {
      // passed-in block's previous block's cumulative difficulty, found on the main chain
      alt_data.cumulative_difficulty = m_db->get_block_cumulative_difficulty(m_db->get_block_height(b.prev_id));
    }
    alt_data.cumulative_difficulty += current_diff;

    CHECK_AND_ASSERT_MES(!m_db->get_alt_block(id, NULL, NULL, NULL), false, "insertion of new alternative block returned as it already exists");
    {
      cryptonote::blobdata checkpoint_blob;
      if (checkpoint)
      {
        alt_data.checkpointed = true;
        checkpoint_blob       = t_serializable_object_to_blob(*checkpoint);
        num_checkpoints_on_alt_chain++;
      }

      alt_data.height                   = block_height;
      alt_data.already_generated_coins  = get_outs_money_amount(b.miner_tx);
      alt_data.already_generated_coins += (alt_chain.size() ? prev_data.already_generated_coins : m_db->get_block_already_generated_coins(block_height - 1));
      m_db->add_alt_block(id, alt_data, cryptonote::block_to_blob(b), checkpoint_blob.empty() ? nullptr : &checkpoint_blob);

      bool height_is_checkpointed = false;
      bool alt_block_matches_checkpoint = m_checkpoints.check_block(alt_data.height, id, &height_is_checkpointed, nullptr);
      if (height_is_checkpointed)
      {
        if (!alt_block_matches_checkpoint)
          num_checkpoints_on_chain++;
      }
    }
    alt_chain.push_back(block_extended_info(alt_data, b, checkpoint));

    bool service_node_checkpoint = false;
    if (!checkpoint && !m_checkpoints.check_block(block_height, id, nullptr, &service_node_checkpoint))
    {
      if (!service_node_checkpoint || b.major_version >= cryptonote::network_version_16)
      {
        LOG_ERROR("CHECKPOINT VALIDATION FAILED FOR ALT BLOCK");
        bvc.m_verification_failed = true;
        return false;
      }
    }

    bool alt_chain_has_greater_pow       = alt_data.cumulative_difficulty > main_chain_cumulative_difficulty;
    bool alt_chain_has_more_checkpoints  = (num_checkpoints_on_alt_chain > num_checkpoints_on_chain);
    bool alt_chain_has_equal_checkpoints = (num_checkpoints_on_alt_chain == num_checkpoints_on_chain);
    {
      std::vector<transaction> txs;
      std::vector<crypto::hash> missed;
      if (!get_transactions(b.tx_hashes, txs, missed))
      {
        bvc.m_verification_failed = true;
        return false;
      }

      for (AltBlockAddedHook *hook : m_alt_block_added_hooks)
      {
        if (!hook->alt_block_added(b, txs, checkpoint))
            return false;
      }
    }

    if (b.major_version >= network_version_16)
    {
      if (alt_chain_has_more_checkpoints || (alt_chain_has_greater_pow && alt_chain_has_equal_checkpoints))
      {
        bool keep_alt_chain = false;
        if (alt_chain_has_more_checkpoints)
        {
          MGINFO_GREEN("###### REORGANIZE on height: " << alt_chain.front().height << " of " << m_db->height() - 1 << ", checkpoint is found in alternative chain on height: "  << block_height);
        }
        else
        {
          keep_alt_chain = true;
          MGINFO_GREEN("###### REORGANIZE on height: " << alt_chain.front().height << " of " << m_db->height() - 1 << " with cum_difficulty " << m_db->get_block_cumulative_difficulty(m_db->height() - 1) << std::endl << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << alt_data.cumulative_difficulty);
        }

        bool r = switch_to_alternative_blockchain(alt_chain, keep_alt_chain);
        if (r)
          bvc.m_added_to_main_chain = true;
        else
          bvc.m_verification_failed = true;
        return r;
      }
      else
      {
        MGINFO_BLUE("----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << block_height << std::endl << "id:\t" << id << std::endl << "PoW:\t" << proof_of_work << std::endl << "difficulty:\t" << current_diff);
        return true;
      }
    }
    else
    {
      if (alt_chain_has_greater_pow)
      {
        MGINFO_GREEN("###### REORGANIZE on height: " << alt_chain.front().height << " of " << m_db->height() - 1 << " with cum_difficulty " << m_db->get_block_cumulative_difficulty(m_db->height() - 1) << std::endl << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << alt_data.cumulative_difficulty);
        bool r = switch_to_alternative_blockchain(alt_chain, true);
        if (r)
          bvc.m_added_to_main_chain = true;
        else
          bvc.m_verification_failed = true;
        return r;
      }
      else
      {
        MGINFO_BLUE("----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << block_height << std::endl << "id:\t" << id << std::endl << "PoW:\t" << proof_of_work << std::endl << "difficulty:\t" << current_diff);
        return true;
      }
    }
  }
  else
  {
    //block orphaned
    bvc.m_marked_as_orphaned = true;
    MERROR_VER("Block recognized as orphaned and rejected, id = " << id << ", height " << block_height
        << ", parent in alt " << parent_in_alt << ", parent in main " << parent_in_main
        << " (parent " << b.prev_id << ", current top " << get_tail_id() << ", chain height " << curr_blockchain_height << ")");
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks, std::vector<cryptonote::blobdata>& txs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  if(start_offset >= m_db->height())
    return false;

  if (!get_blocks(start_offset, count, blocks))
  {
    return false;
  }

  for(const auto& blk : blocks)
  {
    std::vector<crypto::hash> missed_ids;
    get_transactions_blobs(blk.second.tx_hashes, txs, missed_ids);
    CHECK_AND_ASSERT_MES(!missed_ids.size(), false, "has missed transactions in own block in main blockchain");
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  const uint64_t height = m_db->height();
  if(start_offset >= height)
    return false;

  const size_t num_blocks = std::min<uint64_t>(height - start_offset, count);
  blocks.reserve(blocks.size() + num_blocks);
  for (size_t i = 0; i < num_blocks; i++)
  {
    blocks.emplace_back(m_db->get_block_blob_from_height(start_offset + i), block{});
    if (!parse_and_validate_block_from_blob(blocks.back().first, blocks.back().second))
    {
      LOG_ERROR("Invalid block");
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
//TODO: This function *looks* like it won't need to be rewritten
//      to use BlockchainDB, as it calls other functions that were,
//      but it warrants some looking into later.
//
//FIXME: This function appears to want to return false if any transactions
//       that belong with blocks are missing, but not if blocks themselves
//       are missing.
bool Blockchain::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  db_rtxn_guard rtxn_guard(m_db);
  rsp.current_blockchain_height = get_current_blockchain_height();
  std::vector<std::pair<cryptonote::blobdata,block>> blocks;
  get_blocks(arg.blocks, blocks, rsp.missed_ids);

  uint64_t const top_height = (m_db->height() - 1);
  uint64_t const earliest_height_to_sync_checkpoints_granularly = (top_height < service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL) ? 0 : top_height - service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL;

  for(auto& bl : blocks)
  {
    std::vector<crypto::hash> missed_tx_ids;

    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();

    uint64_t const block_height = get_block_height(bl.second);
    uint64_t checkpoint_interval = service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL;
    if (block_height >= earliest_height_to_sync_checkpoints_granularly)
      checkpoint_interval = service_nodes::CHECKPOINT_INTERVAL;

    if ((block_height % checkpoint_interval) == 0)
    {
      try
      {
        checkpoint_t checkpoint;
        if (get_checkpoint(block_height, checkpoint))
          e.checkpoint = t_serializable_object_to_blob(checkpoint);
      }
      catch (const std::exception &e)
      {
        MERROR("Get block checkpoint from DB failed non-trivially at height: " << block_height << ", what = " << e.what());
        return false;
      }
    }

    // FIXME: s/rsp.missed_ids/missed_tx_id/ ?  Seems like rsp.missed_ids
    //        is for missed blocks, not missed transactions as well.
    get_transactions_blobs(bl.second.tx_hashes, e.txs, missed_tx_ids);

    if (missed_tx_ids.size() != 0)
    {
      LOG_ERROR("Error retrieving blocks, missed " << missed_tx_ids.size()
          << " transactions for block with hash: " << get_block_hash(bl.second)
          << std::endl
      );

      // append missed transaction hashes to response missed_ids field,
      // as done below if any standalone transactions were requested
      // and missed.
      rsp.missed_ids.insert(rsp.missed_ids.end(), missed_tx_ids.begin(), missed_tx_ids.end());
      return false;
    }

    //pack block
    e.block = std::move(bl.first);
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_alternative_blocks(std::vector<block>& blocks) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  blocks.reserve(m_db->get_alt_block_count());
  m_db->for_all_alt_blocks([&blocks](const crypto::hash &blkid, const cryptonote::alt_block_data_t &data, const cryptonote::blobdata *block_blob, const cryptonote::blobdata *checkpoint_blob)
  {
    if (!block_blob)
    {
      MERROR("No blob, but blobs were requested");
      return false;
    }
    cryptonote::block bl;
    if (cryptonote::parse_and_validate_block_from_blob(*block_blob, bl))
      blocks.push_back(std::move(bl));
    else
      MERROR("Failed to parse block from blob");
    return true;
  }, true);
  return true;
}
//------------------------------------------------------------------
size_t Blockchain::get_alternative_blocks_count() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  return m_db->get_alt_block_count();
}
//------------------------------------------------------------------
// This function adds the output specified by <amount, i> to the result_outs container
// unlocked and other such checks should be done by here.
uint64_t Blockchain::get_num_mature_outputs(uint64_t amount) const
{
  uint64_t num_outs = m_db->get_num_outputs(amount);
  // ensure we don't include outputs that aren't yet eligible to be used
  // outpouts are sorted by height
  const uint64_t blockchain_height = m_db->height();
  while (num_outs > 0)
  {
    const tx_out_index toi = m_db->get_output_tx_and_index(amount, num_outs - 1);
    const uint64_t height = m_db->get_tx_block_height(toi.first);
    if (height + config::tx_settings::ARQMA_TX_CONFIRMATIONS_REQUIRED <= blockchain_height)
      break;
    --num_outs;
  }

  return num_outs;
}

crypto::public_key Blockchain::get_output_key(uint64_t amount, uint64_t global_index) const
{
  output_data_t data = m_db->get_output_key(amount, global_index);
  return data.pubkey;
}

//------------------------------------------------------------------
bool Blockchain::get_outs(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  res.outs.clear();
  res.outs.reserve(req.outputs.size());

  std::vector<cryptonote::output_data_t> data;
  try
  {
    std::vector<uint64_t> amounts, offsets;
    amounts.reserve(req.outputs.size());
    offsets.reserve(req.outputs.size());
    for (const auto &i: req.outputs)
    {
      amounts.push_back(i.amount);
      offsets.push_back(i.index);
    }
    m_db->get_output_key(epee::span<const uint64_t>(amounts.data(), amounts.size()), offsets, data);
    if(data.size() != req.outputs.size())
    {
      MERROR("Unexpected output data size: expected " << req.outputs.size() << ", got " << data.size());
      return false;
    }
    for(const auto &t: data)
      res.outs.push_back({t.pubkey, t.commitment, is_output_spendtime_unlocked(t.unlock_time), t.height, crypto::null_hash});

    if (req.get_txid)
    {
      for (size_t i = 0; i < req.outputs.size(); ++i)
      {
        tx_out_index toi = m_db->get_output_tx_and_index(req.outputs[i].amount, req.outputs[i].index);
        res.outs[i].txid = toi.first;
      }
    }
  }
  catch (const std::exception& e)
  {
    return false;
  }
  return true;
}
//------------------------------------------------------------------
void Blockchain::get_output_key_mask_unlocked(const uint64_t& amount, const uint64_t& index, crypto::public_key& key, rct::key& mask, bool& unlocked) const
{
  const auto o_data = m_db->get_output_key(amount, index);
  key = o_data.pubkey;
  mask = o_data.commitment;
  unlocked = is_output_spendtime_unlocked(o_data.unlock_time);
}
//------------------------------------------------------------------
bool Blockchain::get_output_distribution(uint64_t amount, uint64_t from_height, uint64_t to_height, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) const
{
  // Arqma did start from v7 at blockheight 1 so our start is always 0

  start_height = 0;
  base = 0;

  if (to_height > 0 && to_height < from_height)
    return false;

  const uint64_t real_start_height = start_height;
  if (from_height > start_height)
    start_height = from_height;

  distribution.clear();
  uint64_t db_height = m_db->height();
  if (db_height == 0)
    return false;
  if (start_height >= db_height || to_height >= db_height)
    return false;

  if (amount == 0)
  {
    std::vector<uint64_t> heights;
    heights.reserve(to_height + 1 - start_height);
    const uint64_t real_start_height = start_height > 0 ? start_height-1 : start_height;
    for (uint64_t h = real_start_height; h <= to_height; ++h)
      heights.push_back(h);
    distribution = m_db->get_block_cumulative_rct_outputs(heights);
    if (start_height > 0)
    {
      base = distribution[0];
      distribution.erase(distribution.begin());
    }
    return true;
  }
  else
  {
    return m_db->get_output_distribution(amount, start_height, to_height, distribution, base);
  }
}
//------------------------------------------------------------------
bool Blockchain::get_output_blacklist(std::vector<uint64_t> &blacklist) const
{
  return m_db->get_output_blacklist(blacklist);
}
//------------------------------------------------------------------
// This function takes a list of block hashes from another node
// on the network to find where the split point is between us and them.
// This is used to see what to send another node that needs to sync.
bool Blockchain::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  // make sure the request includes at least the genesis block, otherwise
  // how can we expect to sync from the client that the block list came from?
  if(!qblock_ids.size())
  {
    MCERROR("net.p2p", "Client sent wrong NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << qblock_ids.size() << /*", m_height=" << req.m_total_height <<*/ ", dropping connection");
    return false;
  }

  db_rtxn_guard rtxn_guard(m_db);
  // make sure that the last block in the request's block list matches
  // the genesis block
  auto gen_hash = m_db->get_block_hash_from_height(0);
  if(qblock_ids.back() != gen_hash)
  {
    MCERROR("net.p2p", "Client sent wrong NOTIFY_REQUEST_CHAIN: genesis block mismatch: " << std::endl << "id: " << qblock_ids.back() << ", " << std::endl << "expected: " << gen_hash << "," << std::endl << " dropping connection");
    return false;
  }

  // Find the first block the foreign chain has that we also have.
  // Assume qblock_ids is in reverse-chronological order.
  auto bl_it = qblock_ids.begin();
  uint64_t split_height = 0;
  for(; bl_it != qblock_ids.end(); bl_it++)
  {
    try
    {
      if (m_db->block_exists(*bl_it, &split_height))
        break;
    }
    catch (const std::exception& e)
    {
      MWARNING("Non-critical error trying to find block by hash in BlockchainDB, hash: " << *bl_it);
      return false;
    }
  }

  // this should be impossible, as we checked that we share the genesis block,
  // but just in case...
  if(bl_it == qblock_ids.end())
  {
    MERROR("Internal error handling connection, can't find split point");
    return false;
  }

  //we start to put block ids INCLUDING last known id, just to make other side be sure
  starter_offset = split_height;
  return true;
}
//------------------------------------------------------------------
uint64_t Blockchain::block_difficulty(uint64_t i) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  try
  {
    return m_db->get_block_difficulty(i);
  }
  catch (const BLOCK_DNE& e)
  {
    MERROR("Attempted to get block difficulty for height above blockchain height");
  }
  return 0;
}
//------------------------------------------------------------------
//TODO: return type should be void, throw on exception
//       alternatively, return true only if no blocks missed
bool Blockchain::get_blocks(const std::vector<crypto::hash>& block_ids, std::vector<std::pair<cryptonote::blobdata, block>>& blocks, std::vector<crypto::hash>& missed_bs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  blocks.reserve(block_ids.size());
  for (const auto& block_hash : block_ids)
  {
    try
    {
      uint64_t height = 0;
      if (m_db->block_exists(block_hash, &height))
      {
        blocks.push_back(std::make_pair(m_db->get_block_blob_from_height(height), block()));
        if (!parse_and_validate_block_from_blob(blocks.back().first, blocks.back().second))
        {
          LOG_ERROR("Invalid block: " << block_hash);
          blocks.pop_back();
          missed_bs.push_back(block_hash);
        }
      }
      else
        missed_bs.push_back(block_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
//TODO: return type should be void, throw on exception
//       alternatively, return true only if no transactions missed
bool Blockchain::get_transactions_blobs(const std::vector<crypto::hash>& txs_ids, std::vector<cryptonote::blobdata>& txs, std::vector<crypto::hash>& missed_txs, bool pruned) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  txs.reserve(txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      cryptonote::blobdata tx;
      if (pruned && m_db->get_pruned_tx_blob(tx_hash, tx))
        txs.push_back(std::move(tx));
      else if(!pruned && m_db->get_tx_blob(tx_hash, tx))
        txs.push_back(std::move(tx));
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
std::vector<uint64_t> Blockchain::get_transactions_heights(const std::vector<crypto::hash>& txs_ids) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  auto heights = m_db->get_tx_block_heights(txs_ids);
  for (auto &h : heights)
    if (h == std::numeric_limits<uint64_t>::max())
      h == 0;

  return heights;
}
//------------------------------------------------------------------
size_t get_transaction_version(const cryptonote::blobdata &bd)
{
  size_t version;
  const char* begin = static_cast<const char*>(bd.data());
  const char* end = begin + bd.size();
  int read = tools::read_varint(begin, end, version);
  if (read <= 0)
    throw std::runtime_error("Internal error getting transaction version");
  return version;
}
//------------------------------------------------------------------
bool Blockchain::get_split_transactions_blobs(const std::vector<crypto::hash>& txs_ids, std::vector<std::tuple<crypto::hash, cryptonote::blobdata, crypto::hash, cryptonote::blobdata>>& txs, std::vector<crypto::hash>& missed_txs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  txs.reserve(txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      cryptonote::blobdata tx;
      if (m_db->get_pruned_tx_blob(tx_hash, tx))
      {
        txs.push_back(std::make_tuple(tx_hash, std::move(tx), crypto::null_hash, cryptonote::blobdata()));
        if (!is_v1_tx(std::get<1>(txs.back())) && !m_db->get_prunable_tx_hash(tx_hash, std::get<2>(txs.back())))
        {
          MERROR("Prunable data hash not found for " << tx_hash);
          return false;
        }
        if (!m_db->get_prunable_tx_blob(tx_hash, std::get<3>(txs.back())))
          std::get<3>(txs.back()).clear();
      }
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_transactions(const std::vector<crypto::hash>& txs_ids, std::vector<transaction>& txs, std::vector<crypto::hash>& missed_txs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  txs.reserve(txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      cryptonote::blobdata tx;
      if (m_db->get_tx_blob(tx_hash, tx))
      {
        auto& added_tx = txs.emplace_back();
        if (!parse_and_validate_tx_from_blob(tx, added_tx))
        {
          LOG_ERROR("Invalid transaction");
          return false;
        }
      }
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
// Find the split point between us and foreign blockchain and return
// (by reference) the most recent common block hash along with up to
// BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT additional (more recent) hashes.
bool Blockchain::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::vector<crypto::hash>& hashes, uint64_t& start_height, uint64_t& current_height, bool clip_pruned) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  // if we can't find the split point, return false
  if(!find_blockchain_supplement(qblock_ids, start_height))
  {
    return false;
  }

  db_rtxn_guard rtxn_guard(m_db);
  current_height = get_current_blockchain_height();
  uint64_t stop_height = current_height;
  if (clip_pruned)
  {
    const uint32_t pruning_seed = get_blockchain_pruning_seed();
    start_height = tools::get_next_unpruned_block_height(start_height, current_height, pruning_seed);
    stop_height = tools::get_next_pruned_block_height(start_height, current_height, pruning_seed);
  }
  size_t count = 0;
  hashes.reserve(std::min((size_t)(stop_height - start_height), (size_t)BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT));
  for(size_t i = start_height; i < stop_height && count < BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT; i++, count++)
  {
    hashes.push_back(m_db->get_block_hash_from_height(i));
  }

  return true;
}

bool Blockchain::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  bool result = find_blockchain_supplement(qblock_ids, resp.m_block_ids, resp.start_height, resp.total_height, true);
  if (result)
    resp.cumulative_difficulty = m_db->get_block_cumulative_difficulty(resp.total_height - 1);

  return result;
}
//------------------------------------------------------------------
//FIXME: change argument to std::vector, low priority
// find split point between ours and foreign blockchain (or start at
// blockchain height <req_start_block>), and return up to max_count FULL
// blocks by reference.
bool Blockchain::find_blockchain_supplement(const uint64_t req_start_block, const std::list<crypto::hash>& qblock_ids, std::vector<std::pair<std::pair<cryptonote::blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, cryptonote::blobdata> > > >& blocks, uint64_t& total_height, uint64_t& start_height, bool pruned, bool get_miner_tx_hash, size_t max_block_count, size_t max_tx_count) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  // if a specific start height has been requested
  if(req_start_block > 0)
  {
    // if requested height is higher than our chain, return false -- we can't help
    if (req_start_block >= m_db->height())
    {
      return false;
    }
    start_height = req_start_block;
  }
  else
  {
    if(!find_blockchain_supplement(qblock_ids, start_height))
    {
      return false;
    }
  }

  db_rtxn_guard rtxn_guard(m_db);
  total_height = get_current_blockchain_height();
  blocks.reserve(std::min(std::min(max_block_count, (size_t)10000), (size_t)(total_height - start_height)));
  CHECK_AND_ASSERT_MES(m_db->get_blocks_from(start_height, 3, max_block_count, max_tx_count, FIND_BLOCKCHAIN_SUPPLEMENT_MAX_SIZE, blocks, pruned, true, get_miner_tx_hash), false, "Error getting blocks");

  return true;
}
//------------------------------------------------------------------
bool Blockchain::add_block_as_invalid(cryptonote::block const &block)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  auto i_res = m_invalid_blocks.insert(get_block_hash(block));
  CHECK_AND_ASSERT_MES(i_res.second, false, "at insertion invalid block returned status failed");
  MINFO("BLOCK ADDED AS INVALID: " << (*i_res.first) << std::endl << ", prev_id=" << block.prev_id << ", m_invalid_blocks count=" << m_invalid_blocks.size());
  return true;
}
//------------------------------------------------------------------
bool Blockchain::have_block(const crypto::hash& id) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  if(m_db->block_exists(id))
  {
    LOG_PRINT_L3("block exists in main chain");
    return true;
  }

  if(m_db->get_alt_block(id, NULL, NULL, NULL))
  {
    LOG_PRINT_L3("block found in alternative chains");
    return true;
  }

  if(m_invalid_blocks.count(id))
  {
    LOG_PRINT_L3("block found in m_invalid_blocks");
    return true;
  }

  return false;
}
//------------------------------------------------------------------
bool Blockchain::handle_block_to_main_chain(const block& bl, block_verification_context& bvc)
{
    LOG_PRINT_L3("Blockchain::" << __func__);
    crypto::hash id = get_block_hash(bl);
    return handle_block_to_main_chain(bl, id, bvc, nullptr /*checkpoint*/);
}
//------------------------------------------------------------------
size_t Blockchain::get_total_transactions() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->get_tx_count();
}
//------------------------------------------------------------------
// This function checks each input in the transaction <tx> to make sure it
// has not been used already, and adds its key to the container <keys_this_block>.
//
// This container should be managed by the code that validates blocks so we don't
// have to store the used keys in a given block in the permanent storage only to
// remove them later if the block fails validation.
bool Blockchain::check_for_double_spend(const transaction& tx, key_images_container& keys_this_block) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  struct add_transaction_input_visitor: public boost::static_visitor<bool>
  {
    key_images_container& m_spent_keys;
    BlockchainDB* m_db;
    add_transaction_input_visitor(key_images_container& spent_keys, BlockchainDB* db) :
      m_spent_keys(spent_keys), m_db(db)
    {
    }
    bool operator()(const txin_to_key& in) const
    {
      const crypto::key_image& ki = in.k_image;

      // attempt to insert the newly-spent key into the container of
      // keys spent this block.  If this fails, the key was spent already
      // in this block, return false to flag that a double spend was detected.
      //
      // if the insert into the block-wide spent keys container succeeds,
      // check the blockchain-wide spent keys container and make sure the
      // key wasn't used in another block already.
      auto r = m_spent_keys.insert(ki);
      if(!r.second || m_db->has_key_image(ki))
      {
        //double spend detected
        return false;
      }

      // if no double-spend detected, return true
      return true;
    }

    bool operator()(const txin_gen& tx) const
    {
      return true;
    }
    bool operator()(const txin_to_script& tx) const
    {
      return false;
    }
    bool operator()(const txin_to_scripthash& tx) const
    {
      return false;
    }
  };

  for (const txin_v& in : tx.vin)
  {
    if(!boost::apply_visitor(add_transaction_input_visitor(keys_this_block, m_db), in))
    {
      LOG_ERROR("Double spend detected!");
      return false;
    }
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_tx_outputs_gindexs(const crypto::hash& tx_id, size_t n_txes, std::vector<std::vector<uint64_t>>& indexs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  uint64_t tx_index;
  if (!m_db->tx_exists(tx_id, tx_index))
  {
    MERROR_VER("get_tx_outputs_gindexs failed to find transaction with id = " << tx_id);
    return false;
  }
  indexs = m_db->get_tx_amount_output_indices(tx_index, n_txes);
  CHECK_AND_ASSERT_MES(n_txes == indexs.size(), false, "Wrong indexs size");

  return true;
}
//--------------------------------------------------------------------
bool Blockchain::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};
  uint64_t tx_index;
  if (!m_db->tx_exists(tx_id, tx_index))
  {
    MERROR_VER("get_tx_outputs_gindexs failed to find transaction with id = " << tx_id);
    return false;
  }
  std::vector<std::vector<uint64_t>> indices = m_db->get_tx_amount_output_indices(tx_index, 1);
  CHECK_AND_ASSERT_MES(indices.size() == 1, false, "Wrong indices size");
  indexs = indices.front();
  return true;
}
//------------------------------------------------------------------
void Blockchain::on_new_tx_from_block(const cryptonote::transaction &tx)
{
#if defined(PER_BLOCK_CHECKPOINT)
  // check if we're doing per-block checkpointing
  if (m_db->height() < m_blocks_hash_check.size())
  {
    TIME_MEASURE_START(a);
    m_blocks_txs_check.push_back(get_transaction_hash(tx));
    TIME_MEASURE_FINISH(a);
    if(m_show_time_stats)
    {
      size_t ring_size = !tx.vin.empty() && tx.vin[0].type() == typeid(txin_to_key) ? boost::get<txin_to_key>(tx.vin[0]).key_offsets.size() : 0;
      MINFO("HASH: " << "-" << " I/M/O: " << tx.vin.size() << "/" << ring_size << "/" << tx.vout.size() << " H: " << 0 << " chcktx: " << a);
    }
  }
#endif
}
//------------------------------------------------------------------
//FIXME: it seems this function is meant to be merely a wrapper around
//       another function of the same name, this one adding one bit of
//       functionality.  Should probably move anything more than that
//       (getting the hash of the block at height max_used_block_id)
//       to the other function to keep everything in one place.
// This function overloads its sister function with
// an extra value (hash of highest block that holds an output used as input)
// as a return-by-reference.
bool Blockchain::check_tx_inputs(transaction& tx, uint64_t& max_used_block_height, crypto::hash& max_used_block_id, tx_verification_context &tvc, bool kept_by_block) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

#if defined(PER_BLOCK_CHECKPOINT)
  // check if we're doing per-block checkpointing
  if (m_db->height() < m_blocks_hash_check.size() && kept_by_block)
  {
    max_used_block_id = null_hash;
    max_used_block_height = 0;
    return true;
  }
#endif

  TIME_MEASURE_START(a);
  bool res = check_tx_inputs(tx, tvc, &max_used_block_height);
  TIME_MEASURE_FINISH(a);
  if(m_show_time_stats)
  {
    size_t ring_size = !tx.vin.empty() && tx.vin[0].type() == typeid(txin_to_key) ? boost::get<txin_to_key>(tx.vin[0]).key_offsets.size() : 0;
    MINFO("HASH: " <<  get_transaction_hash(tx) << " I/M/O: " << tx.vin.size() << "/" << ring_size << "/" << tx.vout.size() << " H: " << max_used_block_height << " ms: " << a + m_fake_scan_time << " B: " << get_object_blobsize(tx) << " W: " << get_transaction_weight(tx));
  }
  if (!res)
    return false;

  CHECK_AND_ASSERT_MES(max_used_block_height < m_db->height(), false,  "internal error: max used block index=" << max_used_block_height << " is not less then blockchain size = " << m_db->height());
  max_used_block_id = m_db->get_block_hash_from_height(max_used_block_height);
  return true;
}
//------------------------------------------------------------------
bool Blockchain::check_tx_outputs(const transaction& tx, tx_verification_context &tvc) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  std::unique_lock lock{*this};

  for(const auto &o: tx.vout)
  {
    if(tx.version == txversion::v1)
    {
      if(!is_valid_decomposed_amount(o.amount))
      {
        tvc.m_invalid_output = true;
        return true;
      }
    }
    else
    {
      if(o.amount != 0)
      {
        tvc.m_invalid_output = true;
        return false;
      }
    }

    if(o.target.type() == typeid(txout_to_key))
    {
      const txout_to_key& out_to_key = boost::get<txout_to_key>(o.target);
      if(!crypto::check_key(out_to_key.key))
      {
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  const uint8_t hf_version = m_hardfork->get_current_version();
  if(hf_version < 8)
  {
    const bool bulletproof = rct::is_rct_bulletproof(tx.rct_signatures.type);
    if(bulletproof || !tx.rct_signatures.p.bulletproofs.empty())
    {
      MERROR("Bulletproofs are not allowed before v8");
      tvc.m_invalid_output = true;
      return false;
    }
  }

  if(hf_version < 13 && tx.rct_signatures.type == rct::RCTTypeBulletproof)
  {
    const bool bulletproof = rct::is_rct_bulletproof(tx.rct_signatures.type);
    if(bulletproof || !tx.rct_signatures.p.bulletproofs.empty())
    {
      MERROR_VER("RingCT type: " << (unsigned)rct::RCTTypeBulletproof << " is not allowed before v" << network_version_13);
      tvc.m_invalid_output = true;
      return false;
    }
  }

  // from v14, forbid borromean range proofs
  if(hf_version > 13)
  {
    const bool borromean = rct::is_rct_borromean(tx.rct_signatures.type);
    if(borromean)
    {
      MERROR_VER("Borromean range proofs are not allowed after v13");
      tvc.m_invalid_output = true;
      return false;
    }
  }

  // from v16 (Service Nodes) allow Smaller Bulletproofs
  if(hf_version < 16)
  {
    if(tx.rct_signatures.type == rct::RCTTypeBulletproof2)
    {
      MERROR_VER("RingCT type: " << (unsigned)rct::RCTTypeBulletproof2 << " is not allowed before Hard Fork v" << network_version_16);
      tvc.m_invalid_output = true;
      return false;
    }
  }

  if(hf_version > 16)
  {
    if(tx.version >= txversion::v3 && tx.is_transfer())
    {
      if(tx.rct_signatures.type == rct::RCTTypeSimpleBulletproof || tx.rct_signatures.type == rct::RCTTypeFullBulletproof || tx.rct_signatures.type == rct::RCTTypeBulletproof)
      {
        MERROR_VER("RingCT type: " << (unsigned)rct::RCTTypeBulletproof << " is not allowed after Hard Fork v" << (network_version_16 + 1));
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::have_tx_keyimges_as_spent(const transaction &tx) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  for (const txin_v& in: tx.vin)
  {
    CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, in_to_key, true);
    if(have_tx_keyimg_as_spent(in_to_key.k_image))
      return true;
  }
  return false;
}
//-----------------------------------------------------------------
bool Blockchain::expand_transaction_2(transaction &tx, const crypto::hash &tx_prefix_hash, const std::vector<std::vector<rct::ctkey>> &pubkeys) const
{
  PERF_TIMER(expand_transaction_2);
  CHECK_AND_ASSERT_MES(tx.version >= txversion::v2, false, "Transaction version is less than 2");

  rct::rctSig &rv = tx.rct_signatures;

  // message - hash of the transaction prefix
  rv.message = rct::hash2rct(tx_prefix_hash);

  // mixRing - full and simple store it in opposite ways
  if(rv.type == rct::RCTTypeFull || rv.type == rct::RCTTypeFullBulletproof)
  {
    CHECK_AND_ASSERT_MES(!pubkeys.empty() && !pubkeys[0].empty(), false, "empty pubkeys");
    rv.mixRing.resize(pubkeys[0].size());
    for(size_t m = 0; m < pubkeys[0].size(); ++m)
      rv.mixRing[m].clear();
    for(size_t n = 0; n < pubkeys.size(); ++n)
    {
      CHECK_AND_ASSERT_MES(pubkeys[n].size() <= pubkeys[0].size(), false, "More inputs that first ring");
      for(size_t m = 0; m < pubkeys[n].size(); ++m)
      {
        rv.mixRing[m].push_back(pubkeys[n][m]);
      }
    }
  }
  else if(rv.type == rct::RCTTypeSimple || rv.type == rct::RCTTypeSimpleBulletproof || rv.type == rct::RCTTypeBulletproof || rv.type == rct::RCTTypeBulletproof2)
  {
    CHECK_AND_ASSERT_MES(!pubkeys.empty() && !pubkeys[0].empty(), false, "empty pubkeys");
    rv.mixRing.resize(pubkeys.size());
    for(size_t n = 0; n < pubkeys.size(); ++n)
    {
      rv.mixRing[n].clear();
      for(size_t m = 0; m < pubkeys[n].size(); ++m)
      {
        rv.mixRing[n].push_back(pubkeys[n][m]);
      }
    }
  }
  else
  {
    CHECK_AND_ASSERT_MES(false, false, "Unsupported rct tx type: " + boost::lexical_cast<std::string>(rv.type));
  }

  // II
  if(rv.type == rct::RCTTypeFull || rv.type == rct::RCTTypeFullBulletproof)
  {
    rv.p.MGs.resize(1);
    rv.p.MGs[0].II.resize(tx.vin.size());
    for (size_t n = 0; n < tx.vin.size(); ++n)
      rv.p.MGs[0].II[n] = rct::ki2rct(boost::get<txin_to_key>(tx.vin[n]).k_image);
  }
  else if(rv.type == rct::RCTTypeSimple || rv.type == rct::RCTTypeSimpleBulletproof || rv.type == rct::RCTTypeBulletproof || rv.type == rct::RCTTypeBulletproof2)
  {
    CHECK_AND_ASSERT_MES(rv.p.MGs.size() == tx.vin.size(), false, "Bad MGs size");
    for(size_t n = 0; n < tx.vin.size(); ++n)
    {
      rv.p.MGs[n].II.resize(1);
      rv.p.MGs[n].II[0] = rct::ki2rct(boost::get<txin_to_key>(tx.vin[n]).k_image);
    }
  }
  else
  {
    CHECK_AND_ASSERT_MES(false, false, "Unsupported rct tx type: " + boost::lexical_cast<std::string>(rv.type));
  }

  // outPk was already done by handle_incoming_tx

  return true;
}
//------------------------------------------------------------------
// This function validates transaction inputs and their keys.
// FIXME: consider moving functionality specific to one input into
//        check_tx_input() rather than here, and use this function simply
//        to iterate the inputs as necessary (splitting the task
//        using threads, etc.)
bool Blockchain::check_tx_inputs(transaction &tx, tx_verification_context &tvc, uint64_t* pmax_used_block_height) const
{
  PERF_TIMER(check_tx_inputs);
  LOG_PRINT_L3("Blockchain::" << __func__);
  size_t sig_index = 0;
  if(pmax_used_block_height)
    *pmax_used_block_height = 0;

  const auto hard_fork_version = m_hardfork->get_current_version();

  {
    txtype max_type = transaction::get_max_type_for_hf(hard_fork_version);
    txversion min_version = transaction::get_min_version_for_hf(hard_fork_version);
    txversion max_version = transaction::get_max_version_for_hf(hard_fork_version);
    tvc.m_invalid_type = (tx.tx_type > max_type);
    tvc.m_invalid_version = tx.version < min_version || tx.version > max_version;
    if(tvc.m_invalid_version || tvc.m_invalid_type)
    {
      if(tvc.m_invalid_version)
        MERROR_VER("TX Invalid version: " << tx.version << " for hardfork: " << hard_fork_version << " min/max version:  " << min_version << "/" << max_version);
      if(tvc.m_invalid_type)
        MERROR_VER("TX Invalid type: " << tx.tx_type << " for hardfork: " << hard_fork_version << " max type: " << max_type);
      return false;
    }
  }

  if(tx.is_transfer())
  {
    crypto::hash tx_prefix_hash = get_transaction_prefix_hash(tx);

    size_t n_unmixable = 0, n_mixable = 0;
    size_t mixin = std::numeric_limits<size_t>::max();
    const size_t min_mixin = hard_fork_version >= 13 ? 10 : 6;
    for(const auto& txin : tx.vin)
    {
      if(txin.type() == typeid(txin_to_key))
      {
        const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);
        if(in_to_key.amount == 0)
          ++n_mixable;
        else
        {
          uint64_t n_outputs = m_db->get_num_outputs(in_to_key.amount);
          MDEBUG("output size " << print_money(in_to_key.amount) << ": " << n_outputs << " available");
          if(n_outputs <= min_mixin)
            ++n_unmixable;
          else
            ++n_mixable;
        }
        if(in_to_key.key_offsets.size() - 1 < mixin)
          mixin = in_to_key.key_offsets.size() - 1;
      }
    }

    if(hard_fork_version >= 13 && mixin != 10)
    {
      MERROR_VER("Transaction: " << get_transaction_hash(tx) << " has invalid ring size (" << (mixin + 1) << "), it should be 11");
      tvc.m_low_mixin = true;
      return false;
    }

    if(mixin < min_mixin)
    {
      if(n_unmixable == 0)
      {
        MERROR_VER("Tx " << get_transaction_hash(tx) << " has too low ring size (" << (mixin + 1) << "), and no unmixable inputs");
        tvc.m_low_mixin = true;
        return false;
      }
      if(n_mixable > 1)
      {
        MERROR_VER("Tx " << get_transaction_hash(tx) << " has too low ring size (" << (mixin + 1) << "), and more than one mixable input with unmixable inputs");
        tvc.m_low_mixin = true;
        return false;
      }
    }

    if (hard_fork_version >= 7)
    {
      const crypto::key_image *last_key_image = NULL;
      for(size_t n = 0; n < tx.vin.size(); ++n)
      {
        const txin_v &txin = tx.vin[n];
        if(txin.type() == typeid(txin_to_key))
        {
          const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);
          if (last_key_image && memcmp(&in_to_key.k_image, last_key_image, sizeof(*last_key_image)) >= 0)
          {
            MERROR_VER("transaction has unsorted inputs");
            tvc.m_verification_failed = true;
            return false;
          }
          last_key_image = &in_to_key.k_image;
        }
      }
    }

    std::vector<std::vector<rct::ctkey>> pubkeys(tx.vin.size());
    std::vector<uint64_t> results;
    results.resize(tx.vin.size(), 0);

    tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
    tools::threadpool::waiter waiter(tpool);
    int threads = tpool.get_max_concurrency();

    uint64_t max_used_block_height = 0;
    if (!pmax_used_block_height)
      pmax_used_block_height = &max_used_block_height;
    for (const auto& txin : tx.vin)
    {
      CHECK_AND_ASSERT_MES(txin.type() == typeid(txin_to_key), false, "wrong type id in tx input at Blockchain::check_tx_inputs");
      const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);

      CHECK_AND_ASSERT_MES(in_to_key.key_offsets.size(), false, "empty in_to_key.key_offsets in transaction with id " << get_transaction_hash(tx));

      if(have_tx_keyimg_as_spent(in_to_key.k_image))
      {
        MERROR_VER("Key image already spent in blockchain: " << epee::string_tools::pod_to_hex(in_to_key.k_image));
        tvc.m_double_spend = true;
        return false;
      }

      if(tx.version == txversion::v1)
      {
        CHECK_AND_ASSERT_MES(sig_index < tx.signatures.size(), false, "wrong transaction: not signature entry for input with index= " << sig_index);
      }

      if(!check_tx_input(tx.version, in_to_key, tx_prefix_hash, tx.version == txversion::v1 ? tx.signatures[sig_index] : std::vector<crypto::signature>(), tx.rct_signatures, pubkeys[sig_index], pmax_used_block_height))
      {
        MERROR_VER("Failed to check ring signature for tx " << get_transaction_hash(tx) << "  vin key with k_image: " << in_to_key.k_image << "  sig_index: " << sig_index);
        if(pmax_used_block_height)
        {
          MERROR_VER("  *pmax_used_block_height: " << *pmax_used_block_height);
        }
        return false;
      }

      if(tx.version == txversion::v1)
      {
        if (threads > 1)
        {
          tpool.submit(&waiter, boost::bind(&Blockchain::check_ring_signature, this, std::cref(tx_prefix_hash), std::cref(in_to_key.k_image), std::cref(pubkeys[sig_index]), std::cref(tx.signatures[sig_index]), std::ref(results[sig_index])), true);
        }
        else
        {
          check_ring_signature(tx_prefix_hash, in_to_key.k_image, pubkeys[sig_index], tx.signatures[sig_index], results[sig_index]);
          if (!results[sig_index])
          {
            MERROR_VER("Failed to check ring signature for tx " << get_transaction_hash(tx) << "  vin key with k_image: " << in_to_key.k_image << "  sig_index: " << sig_index);

            if (pmax_used_block_height)  // a default value of NULL is used when called from Blockchain::handle_block_to_main_chain()
            {
              MERROR_VER("*pmax_used_block_height: " << *pmax_used_block_height);
            }

            return false;
          }
        }
      }

      if(hard_fork_version >= 16)
      {
        const auto &blacklist = m_service_node_list.get_blacklisted_key_images();
        for(const auto &entry : blacklist)
        {
          if(in_to_key.k_image == entry.key_image)
          {
            MERROR_VER("Key image: " << epee::string_tools::pod_to_hex(entry.key_image) << " is blacklisted by the Service Node Network");
            tvc.m_key_image_blacklisted = true;
            return false;
          }
        }

        uint64_t unlock_height = 0;
        if(m_service_node_list.is_key_image_locked(in_to_key.k_image, &unlock_height))
        {
          MERROR_VER("Key image: " << epee::string_tools::pod_to_hex(in_to_key.k_image) << " is locked in a stake untill blockheight: " << unlock_height);
          tvc.m_key_image_locked_by_snode = true;
          return false;
        }
      }

      sig_index++;
    }
    if(tx.version == txversion::v1 && threads > 1)
      if (!waiter.wait())
        return false;

    if(tx.version == txversion::v1)
    {
      if(threads > 1)
      {
        bool failed = false;
        for(size_t i = 0; i < tx.vin.size(); i++)
        {
          if(!failed && !results[i])
            failed = true;
        }

        if(failed)
        {
          MERROR_VER("Failed to check ring signatures");
          return false;
        }
      }
    }
    else
    {
      if(!expand_transaction_2(tx, tx_prefix_hash, pubkeys))
      {
        MERROR_VER("Failed to expand rct signatures!");
        return false;
      }

      const rct::rctSig &rv = tx.rct_signatures;
      switch (rv.type)
      {
      case rct::RCTTypeNull: {
        // we only accept no signatures for coinbase txes
        MERROR_VER("Null rct signature on non-coinbase tx");
        return false;
      }
      case rct::RCTTypeSimple:
      case rct::RCTTypeSimpleBulletproof:
      case rct::RCTTypeBulletproof:
      case rct::RCTTypeBulletproof2:
      {
        // check all this, either reconstructed (so should really pass), or not
        {
          if (pubkeys.size() != rv.mixRing.size())
          {
            MERROR_VER("Failed to check ringct signatures: mismatched pubkeys/mixRing size");
            return false;
          }
          for (size_t i = 0; i < pubkeys.size(); ++i)
          {
            if (pubkeys[i].size() != rv.mixRing[i].size())
            {
              MERROR_VER("Failed to check ringct signatures: mismatched pubkeys/mixRing size");
              return false;
            }
          }

          for (size_t n = 0; n < pubkeys.size(); ++n)
          {
            for (size_t m = 0; m < pubkeys[n].size(); ++m)
            {
              if (pubkeys[n][m].dest != rct::rct2pk(rv.mixRing[n][m].dest))
              {
                MERROR_VER("Failed to check ringct signatures: mismatched pubkey at vin " << n << ", index " << m);
                return false;
              }
              if (pubkeys[n][m].mask != rct::rct2pk(rv.mixRing[n][m].mask))
              {
                MERROR_VER("Failed to check ringct signatures: mismatched commitment at vin " << n << ", index " << m);
                return false;
              }
            }
          }
        }

        if (rv.p.MGs.size() != tx.vin.size())
        {
          MERROR_VER("Failed to check ringct signatures: mismatched MGs/vin sizes");
          return false;
        }
        for (size_t n = 0; n < tx.vin.size(); ++n)
        {
          if (rv.p.MGs[n].II.empty() || memcmp(&boost::get<txin_to_key>(tx.vin[n]).k_image, &rv.p.MGs[n].II[0], 32))
          {
            MERROR_VER("Failed to check ringct signatures: mismatched key image");
            return false;
          }
        }

        if (!rct::verRctNonSemanticsSimple(rv))
        {
          MERROR_VER("Failed to check ringct signatures!");
          return false;
        }
        break;
      }
      case rct::RCTTypeFull:
      case rct::RCTTypeFullBulletproof:
      {
        // check all this, either reconstructed (so should really pass), or not
        {
          bool size_matches = true;
          for (size_t i = 0; i < pubkeys.size(); ++i)
            size_matches &= pubkeys[i].size() == rv.mixRing.size();
          for (size_t i = 0; i < rv.mixRing.size(); ++i)
            size_matches &= pubkeys.size() == rv.mixRing[i].size();
          if (!size_matches)
          {
            MERROR_VER("Failed to check ringct signatures: mismatched pubkeys/mixRing size");
            return false;
          }

          for (size_t n = 0; n < pubkeys.size(); ++n)
          {
            for (size_t m = 0; m < pubkeys[n].size(); ++m)
            {
              if (pubkeys[n][m].dest != rct::rct2pk(rv.mixRing[m][n].dest))
              {
                MERROR_VER("Failed to check ringct signatures: mismatched pubkey at vin " << n << ", index " << m);
                return false;
              }
              if (pubkeys[n][m].mask != rct::rct2pk(rv.mixRing[m][n].mask))
              {
                MERROR_VER("Failed to check ringct signatures: mismatched commitment at vin " << n << ", index " << m);
                return false;
              }
            }
          }
        }

        if (rv.p.MGs.size() != 1)
        {
          MERROR_VER("Failed to check ringct signatures: Bad MGs size");
          return false;
        }
        if (rv.p.MGs.empty() || rv.p.MGs[0].II.size() != tx.vin.size())
        {
          MERROR_VER("Failed to check ringct signatures: mismatched II/vin sizes");
          return false;
        }
        for (size_t n = 0; n < tx.vin.size(); ++n)
        {
          if (memcmp(&boost::get<txin_to_key>(tx.vin[n]).k_image, &rv.p.MGs[0].II[n], 32))
          {
            MERROR_VER("Failed to check ringct signatures: mismatched II/vin sizes");
            return false;
          }
        }

        if (!rct::verRct(rv, false))
        {
          MERROR_VER("Failed to check ringct signatures!");
          return false;
        }
        break;
      }
      default:
        MERROR_VER("Unsupported rct type: " << rv.type);
        return false;
      }

      // for bulletproofs, check they're only multi-output after v13
      if(rct::is_rct_bulletproof(rv.type))
      {
        if(hard_fork_version < 13)
        {
          for(const rct::Bulletproof &proof: rv.p.bulletproofs)
          {
            if(proof.V.size() > 1)
            {
              MERROR_VER("Multi output bulletproofs are invalid before v13");
              return false;
            }
          }
        }
      }
    }
  }
  else
  {
    CHECK_AND_ASSERT_MES(tx.vin.size() == 0, false, "Transaction type: " << tx.tx_type << " should have 0 inputs and should be rejected in check_tx_semantic!");

    if (tx.rct_signatures.txnFee != 0)
    {
      tvc.m_invalid_input = true;
      tvc.m_verification_failed = true;
      MERROR_VER("Transaction type: " << tx.tx_type << " should have 0 fee");
      return false;
    }

    if (tx.tx_type == txtype::state_change)
    {
      tx_extra_service_node_state_change state_change;
      if (!get_service_node_state_change_from_tx_extra(tx.extra, state_change))
      {
        MERROR_VER("TX did not have the state change metadata in the tx_extra");
        return false;
      }

      auto const quorum_type = service_nodes::quorum_type::obligations;
      auto const quorum = m_service_node_list.get_quorum(quorum_type, state_change.block_height);
      {
        if (!quorum)
        {
          MERROR_VER("could not get obligations quorum for recent state change tx");
          return false;
        }

        if(!service_nodes::verify_tx_state_change(state_change, get_current_blockchain_height(), tvc, *quorum, hard_fork_version))
        {
          MERROR_VER("Transaction: " << get_transaction_hash(tx) << " : state change tx could not be completely verified. Reason: " << print_vote_verification_context(tvc.m_vote_ctx));
          return false;
        }
      }

      crypto::public_key const &state_change_service_node_pubkey = quorum->workers[state_change.service_node_index];
      std::vector<service_nodes::service_node_pubkey_info> service_node_array = m_service_node_list.get_service_node_list_state({state_change_service_node_pubkey});
      if (service_node_array.empty())
      {
        LOG_PRINT_L2("Service Node no longer exists on the network, state change can be ignored");
        return false;
      }

      service_nodes::service_node_info const &service_node_info = *service_node_array[0].info;
      if (!service_node_info.can_transition_to_state(hard_fork_version, state_change.block_height, state_change.state))
      {
        LOG_PRINT_L2("State change trying to vote Service Node into the same state it already is in, (similar to double_spend)");
        tvc.m_double_spend = true;
        return false;
      }
    }
    else if (tx.tx_type == txtype::key_image_unlock)
    {
      cryptonote::tx_extra_tx_key_image_unlock unlock;
      if(!cryptonote::get_tx_key_image_unlock_from_tx_extra(tx.extra, unlock))
      {
        MERROR("TX extra didn't have key image unlock in the tx_extra");
        return false;
      }

      service_nodes::service_node_info::contribution_t contribution = {};
      uint64_t unlock_height = 0;
      if(!m_service_node_list.is_key_image_locked(unlock.key_image, &unlock_height, &contribution))
      {
        MERROR_VER("Requested key image: " << epee::string_tools::pod_to_hex(unlock.key_image) << " to unlock is not locked");
        tvc.m_invalid_input = true;
        return false;
      }

      crypto::hash const hash = service_nodes::generate_request_stake_unlock_hash(unlock.nonce);
      if(!crypto::check_signature(hash, contribution.key_image_pub_key, unlock.signature))
      {
        MERROR("Could not verify key image unlock transaction signature for tx: " << get_transaction_hash(tx));
        return false;
      }

      if(unlock_height != service_nodes::KEY_IMAGE_AWAITING_UNLOCK_HEIGHT)
      {
        tvc.m_double_spend = true;
        return false;
      }
    }
    else
    {
      MERROR_VER("Unhandled tx type: " << tx.tx_type << " rejecting tx: " << get_transaction_hash(tx));
      tvc.m_invalid_type = true;;
      return false;
    }
  }

  return true;
}
//------------------------------------------------------------------
void Blockchain::check_ring_signature(const crypto::hash &tx_prefix_hash, const crypto::key_image &key_image, const std::vector<rct::ctkey> &pubkeys, const std::vector<crypto::signature>& sig, uint64_t &result) const
{
  std::vector<const crypto::public_key *> p_output_keys;
  p_output_keys.reserve(pubkeys.size());
  for (auto &key : pubkeys)
  {
    // rct::key and crypto::public_key have the same structure, avoid object ctor/memcpy
    p_output_keys.push_back(&(const crypto::public_key&)key.dest);
  }

  result = crypto::check_ring_signature(tx_prefix_hash, key_image, p_output_keys, sig.data()) ? 1 : 0;
}
//------------------------------------------------------------------
uint64_t Blockchain::get_dynamic_base_fee(uint64_t block_reward, size_t median_block_weight, uint8_t version)
{
  const uint64_t min_block_weight = get_min_block_weight(version);
  if (median_block_weight < min_block_weight)
    median_block_weight = min_block_weight;
  uint64_t hi, lo;

  if (version >= HF_VERSION_PER_BYTE_FEE)
  {
    const uint64_t ref_fee = version >= 16 ? HF_16_DYNAMIC_REFERENCE_FEE : DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT;
    lo = mul128(block_reward, ref_fee, &hi);
    div128_32(hi, lo, min_block_weight, &hi, &lo);
    div128_32(hi, lo, median_block_weight, &hi, &lo);
    assert(hi == 0);
    lo /= 5;
    return lo;
  }

  constexpr uint64_t fee_base = DYNAMIC_FEE_PER_BYTE_BASE_FEE_V13;

  uint64_t unscaled_fee_base = (fee_base * min_block_weight / median_block_weight);
  lo = mul128(unscaled_fee_base, block_reward, &hi);
  static_assert(DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD % 1000000 == 0, "DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD must be divisible by 1000000");
  static_assert(DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD / 1000000 <= std::numeric_limits<uint32_t>::max(), "DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD is too large");

  // divide in two steps, since the divisor must be 32 bits, but DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD isn't
  div128_32(hi, lo, DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD / 1000000, &hi, &lo);
  div128_32(hi, lo, 1000000, &hi, &lo);
  assert(hi == 0);

  // quantize fee up to 8 decimals
  uint64_t mask = get_fee_quantization_mask();
  uint64_t qlo = (lo + mask - 1) / mask * mask;
  MDEBUG("lo " << print_money(lo) << ", qlo " << print_money(qlo) << ", mask " << mask);

  return qlo;
}

//------------------------------------------------------------------
bool Blockchain::check_fee(const transaction& tx, size_t tx_weight, uint64_t fee) const
{
  const uint8_t version = get_current_hard_fork_version();

  uint64_t median = 0;
  uint64_t already_generated_coins = 0;
  uint64_t base_reward = 0;
  median = m_current_block_cumul_weight_limit / 2;
  const uint64_t blockchain_height = m_db->height();
  already_generated_coins = blockchain_height ? m_db->get_block_already_generated_coins(blockchain_height - 1) : 0;

  if(!get_base_block_reward(median, 1, already_generated_coins, base_reward, version, blockchain_height))
    return false;

  uint64_t needed_fee;
  if(version >= HF_VERSION_PER_BYTE_FEE)
  {
    const bool use_long_term_median_in_fee = version >= HF_VERSION_LONG_TERM_BLOCK_WEIGHT;
    uint64_t fee_per_byte = get_dynamic_base_fee(base_reward, use_long_term_median_in_fee ? std::min<uint64_t>(median, m_long_term_effective_median_block_weight) : median, version);
    MDEBUG("Using " << print_money(fee_per_byte) << "/byte fee");
    needed_fee = tx_weight * fee_per_byte;
    // quantize fee up to 8 decimals
    const uint64_t mask = get_fee_quantization_mask();
    needed_fee = (needed_fee + mask - 1) / mask * mask;
  }
  else
  {
    uint64_t fee_per_kb = get_dynamic_base_fee(base_reward, median, version) * 60 / 100;
    MDEBUG("Using " << print_money(fee_per_kb) << "/kB fee");

    needed_fee = tx_weight / 1024;
    needed_fee += (tx_weight % 1024) ? 1 : 0;
    needed_fee *= fee_per_kb;
  }

  if(tx.tx_type == txtype::state_change)
  {
    if (fee < needed_fee - needed_fee / 50) // keep a little 2% buffer on acceptance - no integer overflow
    {
      MERROR_VER("transaction fee is not enough: " << print_money(fee) << ", minimum fee: " << print_money(needed_fee));
      return false;
    }
  }
  return true;
}

//------------------------------------------------------------------
uint64_t Blockchain::get_dynamic_base_fee_estimate(uint64_t grace_blocks) const
{
  const uint8_t hard_fork_version = get_current_hard_fork_version();
  const uint64_t db_height = m_db->height();

  if (grace_blocks >= CRYPTONOTE_REWARD_BLOCKS_WINDOW)
    grace_blocks = CRYPTONOTE_REWARD_BLOCKS_WINDOW - 1;

  const uint64_t min_block_weight = get_min_block_weight(hard_fork_version);
  std::vector<uint64_t> weights;
  get_last_n_blocks_weights(weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW - grace_blocks);
  weights.reserve(grace_blocks);
  for (size_t i = 0; i < grace_blocks; ++i)
    weights.push_back(min_block_weight);

  uint64_t median = epee::misc_utils::median(weights);
  if(median <= min_block_weight)
    median = min_block_weight;

  uint64_t already_generated_coins = db_height ? m_db->get_block_already_generated_coins(db_height - 1) : 0;

  uint64_t base_reward;
  if(!get_base_block_reward(m_current_block_cumul_weight_limit / 2, 1, already_generated_coins, base_reward, hard_fork_version, db_height))
  {
    MERROR("Failed to determine block reward, using placeholder " << print_money(BLOCK_REWARD_OVERESTIMATE) << " as a high bound");
    base_reward = BLOCK_REWARD_OVERESTIMATE;
  }

  const bool use_long_term_median_in_fee = hard_fork_version >= HF_VERSION_LONG_TERM_BLOCK_WEIGHT;
  uint64_t fee = get_dynamic_base_fee(base_reward, use_long_term_median_in_fee ? m_long_term_effective_median_block_weight : median, hard_fork_version);
  const bool per_byte = hard_fork_version < HF_VERSION_PER_BYTE_FEE;
  MDEBUG("Estimating " << grace_blocks << "-block fee at " << print_money(fee) << "/" << (per_byte ? "byte" : "kB"));
  return fee;
}

//------------------------------------------------------------------
// This function checks to see if a tx is unlocked.  unlock_time is either
// a block index or a unix time.
bool Blockchain::is_output_spendtime_unlocked(uint64_t unlock_time) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  const uint64_t height = m_db->height();
  return cryptonote::rules::is_output_unlocked(unlock_time, height);
}
//------------------------------------------------------------------
// This function locates all outputs associated with a given input (mixins)
// and validates that they exist and are usable.  It also checks the ring
// signature for each input.
bool Blockchain::check_tx_input(txversion tx_version, const txin_to_key& txin, const crypto::hash& tx_prefix_hash, const std::vector<crypto::signature>& sig, const rct::rctSig &rct_signatures, std::vector<rct::ctkey> &output_keys, uint64_t* pmax_related_block_height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  // ND:
  // 1. Disable locking and make method private.
  //std::unique_lock lock{*this};

  struct outputs_visitor
  {
    std::vector<rct::ctkey>& m_output_keys;
    const Blockchain& m_bch;
    outputs_visitor(std::vector<rct::ctkey>& output_keys, const Blockchain& bch) :
      m_output_keys(output_keys), m_bch(bch)
    {
    }
    bool handle_output(uint64_t unlock_time, const crypto::public_key &pubkey, const rct::key &commitment)
    {
      //check tx unlock time
      if (!m_bch.is_output_spendtime_unlocked(unlock_time))
      {
        MERROR_VER("One of outputs for one of inputs has wrong unlock_time = " << unlock_time);
        return false;
      }

      // The original code includes a check for the output corresponding to this input
      // to be a txout_to_key. This is removed, as the database does not store this info,
      // but only txout_to_key outputs are stored in the DB in the first place, done in
      // Blockchain*::add_output

      m_output_keys.push_back(rct::ctkey({rct::pk2rct(pubkey), commitment}));
      return true;
    }
  };

  output_keys.clear();

  // collect output keys
  outputs_visitor vi(output_keys, *this);
  if (!scan_outputkeys_for_indexes(txin, vi, tx_prefix_hash, pmax_related_block_height))
  {
    MERROR_VER("Failed to get output keys for tx with amount = " << print_money(txin.amount) << " and count indexes " << txin.key_offsets.size());
    return false;
  }

  if(txin.key_offsets.size() != output_keys.size())
  {
    MERROR_VER("Output keys for tx with amount = " << txin.amount << " and count indexes " << txin.key_offsets.size() << " returned wrong keys count " << output_keys.size());
    return false;
  }
  if (tx_version == txversion::v1) {
    CHECK_AND_ASSERT_MES(sig.size() == output_keys.size(), false, "internal error: tx signatures count=" << sig.size() << " mismatch with outputs keys count for inputs=" << output_keys.size());
  }
  // rct_signatures will be expanded after this
  return true;
}
//------------------------------------------------------------------
//TODO: Is this intended to do something else?  Need to look into the todo there.
uint64_t Blockchain::get_adjusted_time() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  //TODO: add collecting median time
  return time(NULL);
}
//------------------------------------------------------------------
//TODO: revisit, has changed a bit on upstream
bool Blockchain::check_block_timestamp(std::vector<uint64_t>& timestamps, const block& b, uint64_t& median_ts) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  median_ts = epee::misc_utils::median(timestamps);

  uint8_t hf_version = get_current_hard_fork_version();
  size_t blockchain_timestamp_check_window = hf_version < 2 ? BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW : BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V2;

  uint64_t top_block_timestamp = timestamps.back();
  if (hf_version > 9 && b.timestamp < top_block_timestamp - CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V11)
  {
    MERROR_VER("Timestamp of Block with id: " << get_block_hash(b) << ", " << b.timestamp << ", is less than Top Block Timestamp - FTL" << top_block_timestamp - CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V11);
    return false;
  }

  if(b.timestamp < median_ts)
  {
    MERROR_VER("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", less than median of last " << blockchain_timestamp_check_window << " blocks, " << median_ts);
    return false;
  }

  return true;
}

bool Blockchain::check_block_timestamp(std::vector<uint64_t>& timestamps, const block& b) const
{
  uint64_t median_ts;
  return check_block_timestamp(timestamps, b, median_ts);
}

//------------------------------------------------------------------
// This function grabs the timestamps from the most recent <n> blocks,
// where n = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW.  If there are not those many
// blocks in the blockchain, the timestap is assumed to be valid.  If there
// are, this function returns:
//   true if the block's timestamp is not less than the timestamp of the
//       median of the selected blocks
//   false otherwise
bool Blockchain::check_block_timestamp(const block& b) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  uint64_t cryptonote_block_future_time_limit;
  uint8_t hf_version = get_current_hard_fork_version();
  if (hf_version >= 10) {
    cryptonote_block_future_time_limit = CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V11;
  } else if (hf_version >= 7) {
    cryptonote_block_future_time_limit = CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3;
  } else {
    cryptonote_block_future_time_limit = CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT;
  }

  if(b.timestamp > get_adjusted_time() + cryptonote_block_future_time_limit)
  {
    MERROR_VER("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", bigger than adjusted time + " << (get_current_hard_fork_version() < 2 ? "2 hours" : "5 minutes"));
    return false;
  }

  uint64_t median_ts;
  return check_median_block_timestamp(b, median_ts);
}

bool Blockchain::check_median_block_timestamp(const block& b, uint64_t& median_ts) const
{
  size_t blockchain_timestamp_check_window = get_current_hard_fork_version() > 9 ? BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V11 : BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V9;

  const auto h = m_db->height();

  // if not enough blocks, no proper median yet, return true
  if(h < blockchain_timestamp_check_window)
  {
    return true;
  }

  std::vector<uint64_t> timestamps;

  // need most recent 60 blocks, get index of first of those
  size_t offset = h - blockchain_timestamp_check_window;
  timestamps.reserve(h - offset);
  for(; offset < h; ++offset)
  {
    timestamps.push_back(m_db->get_block_timestamp(offset));
  }

  return check_block_timestamp(timestamps, b);
}
//------------------------------------------------------------------
void Blockchain::return_tx_to_pool(std::vector<std::pair<transaction, blobdata>> &txs)
{
  uint8_t version = get_current_hard_fork_version();
  for (auto& tx : txs)
  {
    cryptonote::tx_verification_context tvc{};
    // We assume that if they were in a block, the transactions are already
    // known to the network as a whole. However, if we had mined that block,
    // that might not be always true. Unlikely though, and always relaying
    // these again might cause a spike of traffic as many nodes re-relay
    // all the transactions in a popped block when a reorg happens.
    const size_t weight = get_transaction_weight(tx.first, tx.second.size());
    const crypto::hash tx_hash = get_transaction_hash(tx.first);
    if (!m_tx_pool.add_tx(tx.first, tx_hash, tx.second, weight, tvc, tx_pool_options::from_block(), version))
    {
      MERROR("Failed to return taken transaction with hash: " << get_transaction_hash(tx.first) << " to tx_pool");
    }
  }
}
//------------------------------------------------------------------
bool Blockchain::flush_txes_from_pool(const std::vector<crypto::hash> &txids)
{
  std::unique_lock lock{m_tx_pool};

  bool res = true;
  for (const auto &txid: txids)
  {
    cryptonote::transaction tx;
    cryptonote::blobdata txblob;
    size_t tx_weight;
    uint64_t fee;
    bool relayed, do_not_relay, double_spend_seen;
    MINFO("Removing txid " << txid << " from the pool");
    if(m_tx_pool.have_tx(txid) && !m_tx_pool.take_tx(txid, tx, txblob, tx_weight, fee, relayed, do_not_relay, double_spend_seen))
    {
      MERROR("Failed to remove txid " << txid << " from the pool");
      res = false;
    }
  }
  return res;
}
//------------------------------------------------------------------
//      Needs to validate the block and acquire each transaction from the
//      transaction mem_pool, then pass the block and transactions to
//      m_db->add_block()
bool Blockchain::handle_block_to_main_chain(const block& bl, const crypto::hash& id, block_verification_context& bvc, checkpoint_t const *checkpoint)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  TIME_MEASURE_START(block_processing_time);
  std::unique_lock lock{*this};
  TIME_MEASURE_START(t1);

  static bool seen_future_version = false;

  db_rtxn_guard rtxn_guard(m_db);
  uint64_t blockchain_height;
  const crypto::hash top_hash = get_tail_id(blockchain_height);
  ++blockchain_height; // block height to chain height
  if(bl.prev_id != top_hash)
  {
    MERROR_VER("Block with id: " << id << std::endl << "has wrong prev_id: " << bl.prev_id << std::endl << "expected: " << top_hash);
    bvc.m_verification_failed = true;
    return false;
  }

  // warn users if they're running an old version
  if (!seen_future_version && bl.major_version > m_hardfork->get_ideal_version())
  {
    seen_future_version = true;
    const el::Level level = el::Level::Warning;
    MCLOG_RED(level, "global", "**********************************************************************");
    MCLOG_RED(level, "global", "A block was seen on the network with a version higher than the last");
    MCLOG_RED(level, "global", "known one. This may be an old version of the Arqma daemon, and a software");
    MCLOG_RED(level, "global", "update may be required to sync further. ");
    MCLOG_RED(level, "global", "**********************************************************************");
  }

  // this is a cheap test
  const uint8_t hard_fork_version = get_current_hard_fork_version();
  if (!m_hardfork->check(bl))
  {
    MERROR_VER("Block with id: " << id << std::endl << "has old version: " << (unsigned)bl.major_version << std::endl << "current: " << (unsigned)hard_fork_version);
    bvc.m_verification_failed = true;
    return false;
  }

  TIME_MEASURE_FINISH(t1);
  TIME_MEASURE_START(t2);

  // make sure block timestamp is not less than the median timestamp
  // of a set number of the most recent blocks.
  if(!check_block_timestamp(bl))
  {
    MERROR_VER("Block with id: " << id << std::endl << "has invalid timestamp: " << bl.timestamp);
    bvc.m_verification_failed = true;
    return false;
  }

  TIME_MEASURE_FINISH(t2);
  //check proof of work
  TIME_MEASURE_START(target_calculating_time);

  // get the target difficulty for the block.
  // the calculation can overflow, among other failure cases,
  // so we need to check the return type.
  // FIXME: get_difficulty_for_next_block can also assert, look into
  // changing this to throwing exceptions instead so we can clean up.
  difficulty_type current_diffic = get_difficulty_for_next_block();
  CHECK_AND_ASSERT_MES(current_diffic, false, "!!!!!!!!! difficulty overhead !!!!!!!!!");

  TIME_MEASURE_FINISH(target_calculating_time);

  TIME_MEASURE_START(longhash_calculating_time);

  crypto::hash proof_of_work;
  memset(proof_of_work.data, 0xff, sizeof(proof_of_work.data));

  // Formerly the code below contained an if loop with the following condition
  // !m_checkpoints.is_in_checkpoint_zone(get_current_blockchain_height())
  // however, this caused the daemon to not bother checking PoW for blocks
  // before checkpoints, which is very dangerous behaviour. We moved the PoW
  // validation out of the next chunk of code to make sure that we correctly
  // check PoW now.
  // FIXME: height parameter is not used...should it be used or should it not
  // be a parameter?
  // validate proof_of_work versus difficulty target
  bool precomputed = false;
  bool fast_check = false;
#if defined(PER_BLOCK_CHECKPOINT)
  if (blockchain_height < m_blocks_hash_check.size())
  {
    const auto& expected_hash = m_blocks_hash_check[blockchain_height];
    if (expected_hash != crypto::null_hash)
    {
      if (memcmp(&id, &expected_hash, sizeof(hash)) != 0)
      {
        MERROR_VER("Block with id is INVALID: " << id << ", expected " << expected_hash);
        bvc.m_verification_failed = true;
        return false;
      }
      fast_check = true;
    }
    else
    {
      MCINFO("verify", "No pre-validated hash at height " << blockchain_height << ", verifying fully");
    }
  }
#endif
  if(!fast_check)
  {
    auto it = m_blocks_longhash_table.find(id);
    if (it != m_blocks_longhash_table.end())
    {
      precomputed = true;
      proof_of_work = it->second;
    }
    else
      proof_of_work = get_block_longhash(this, bl, blockchain_height, 0);

    // validate proof_of_work versus difficulty target
    if(!check_hash(proof_of_work, current_diffic))
    {
      MERROR_VER("Block with id: " << id << std::endl << "does not have enough proof of work: " << proof_of_work << " at height " << blockchain_height << ", unexpected difficulty: " << current_diffic);
      bvc.m_verification_failed = true;
      return false;
    }
  }

  // If we're at a checkpoint, ensure that our hardcoded checkpoint hash
  // is correct.
  if (m_checkpoints.is_in_checkpoint_zone(blockchain_height))
  {
    bool service_node_checkpoint = false;
    if (!m_checkpoints.check_block(blockchain_height, id, nullptr, &service_node_checkpoint))
    {
      if (!service_node_checkpoint || (service_node_checkpoint && bl.major_version >= cryptonote::network_version_16))
      {
        LOG_ERROR("CHECKPOINT VALIDATION FAILED");
        bvc.m_verification_failed = true;
        return false;
      }
    }
  }

  TIME_MEASURE_FINISH(longhash_calculating_time);
  if (precomputed)
    longhash_calculating_time += m_fake_pow_calc_time;

  TIME_MEASURE_START(t3);

  // sanity check basic miner tx properties;
  if(!prevalidate_miner_transaction(bl, blockchain_height, hard_fork_version))
  {
    MERROR_VER("Block with id: " << id << " failed to pass prevalidation");
    bvc.m_verification_failed = true;
    return false;
  }

  size_t coinbase_weight = get_transaction_weight(bl.miner_tx);
  size_t cumulative_block_weight = coinbase_weight;

  std::vector<std::pair<transaction, blobdata>> txs;
  key_images_container keys;

  uint64_t fee_summary = 0;
  uint64_t t_checktx = 0;
  uint64_t t_exists = 0;
  uint64_t t_pool = 0;
  uint64_t t_dblspnd = 0;
  TIME_MEASURE_FINISH(t3);

// XXX old code adds miner tx here

  size_t tx_index = 0;
  // Iterate over the block's transaction hashes, grabbing each
  // from the tx_pool and validating them.  Each is then added
  // to txs.  Keys spent in each are added to <keys> by the double spend check.
  txs.reserve(bl.tx_hashes.size());
  for (const crypto::hash& tx_id : bl.tx_hashes)
  {
    transaction tx_tmp;
    blobdata txblob;
    size_t tx_weight = 0;
    uint64_t fee = 0;
    bool relayed = false, do_not_relay = false, double_spend_seen = false;
    TIME_MEASURE_START(aa);

// XXX old code does not check whether tx exists
    if (m_db->tx_exists(tx_id))
    {
      MERROR("Block with id: " << id << " attempting to add transaction already in blockchain with id: " << tx_id);
      bvc.m_verification_failed = true;
      return_tx_to_pool(txs);
      return false;
    }

    TIME_MEASURE_FINISH(aa);
    t_exists += aa;
    TIME_MEASURE_START(bb);

    // get transaction with hash <tx_id> from tx_pool
    if(!m_tx_pool.take_tx(tx_id, tx_tmp, txblob, tx_weight, fee, relayed, do_not_relay, double_spend_seen))
    {
      MERROR_VER("Block with id: " << id  << " has at least one unknown transaction with id: " << tx_id);
      bvc.m_verification_failed = true;
      return_tx_to_pool(txs);
      return false;
    }

    TIME_MEASURE_FINISH(bb);
    t_pool += bb;
    // add the transaction to the temp list of transactions, so we can either
    // store the list of transactions all at once or return the ones we've
    // taken from the tx_pool back to it if the block fails verification.
    txs.push_back(std::make_pair(std::move(tx_tmp), std::move(txblob)));
    transaction &tx = txs.back().first;
    TIME_MEASURE_START(dd);

    // FIXME: the storage should not be responsible for validation.
    //        If it does any, it is merely a sanity check.
    //        Validation is the purview of the Blockchain class
    //        - TW
    //
    // ND: this is not needed, db->add_block() checks for duplicate k_images and fails accordingly.
    // if (!check_for_double_spend(tx, keys))
    // {
    //     LOG_PRINT_L0("Double spend detected in transaction (id: " << tx_id);
    //     bvc.m_verification_failed = true;
    //     break;
    // }

    TIME_MEASURE_FINISH(dd);
    t_dblspnd += dd;
    TIME_MEASURE_START(cc);

#if defined(PER_BLOCK_CHECKPOINT)
    if (!fast_check)
#endif
    {
      // validate that transaction inputs and the keys spending them are correct.
      tx_verification_context tvc{};
      if(!check_tx_inputs(tx, tvc))
      {
        MERROR_VER("Block with id: " << id  << " has at least one transaction (id: " << tx_id << ") with wrong inputs.");

        //TODO: why is this done?  make sure that keeping invalid blocks makes sense.
        add_block_as_invalid(bl);
        MERROR_VER("Block with id " << id << " added as invalid because of wrong inputs in transactions");
        MERROR_VER("tx_index " << tx_index << ", m_blocks_txs_check " << m_blocks_txs_check.size() << ":");
        for (const auto &h: m_blocks_txs_check) MERROR_VER("  " << h);
        bvc.m_verification_failed = true;
        return_tx_to_pool(txs);
        return false;
      }
    }
#if defined(PER_BLOCK_CHECKPOINT)
    else
    {
      // ND: if fast_check is enabled for blocks, there is no need to check
      // the transaction inputs, but do some sanity checks anyway.
      if (tx_index >= m_blocks_txs_check.size() || memcmp(&m_blocks_txs_check[tx_index++], &tx_id, sizeof(tx_id)) != 0)
      {
        MERROR_VER("Block with id: " << id << " has at least one transaction (id: " << tx_id << ") with wrong inputs.");
        //TODO: why is this done?  make sure that keeping invalid blocks makes sense.
        add_block_as_invalid(bl);
        MERROR_VER("Block with id " << id << " added as invalid because of wrong inputs in transactions");
        bvc.m_verification_failed = true;
        return_tx_to_pool(txs);
        return false;
      }
    }
#endif
    TIME_MEASURE_FINISH(cc);
    t_checktx += cc;
    fee_summary += fee;
    cumulative_block_weight += tx_weight;
  }

  m_blocks_txs_check.clear();

  TIME_MEASURE_START(vmt);
  uint64_t base_reward = 0;
  uint64_t already_generated_coins = blockchain_height ? m_db->get_block_already_generated_coins(blockchain_height - 1) : 0;
  if(!validate_miner_transaction(bl, cumulative_block_weight, fee_summary, base_reward, already_generated_coins, m_hardfork->get_current_version()))
  {
    MERROR_VER("Block with id: " << id << " has incorrect miner transaction");
    bvc.m_verification_failed = true;
    return_tx_to_pool(txs);
    return false;
  }

  TIME_MEASURE_FINISH(vmt);
  size_t block_weight;
  difficulty_type cumulative_difficulty;

  // populate various metadata about the block to be stored alongside it.
  block_weight = cumulative_block_weight;
  cumulative_difficulty = current_diffic;
  // In the "tail" state when the minimum subsidy (implemented in get_block_reward) is in effect, the number of
  // coins will eventually exceed MONEY_SUPPLY and overflow a uint64. To prevent overflow, cap already_generated_coins
  // at MONEY_SUPPLY. already_generated_coins is only used to compute the block subsidy and MONEY_SUPPLY yields a
  // subsidy of 0 under the base formula and therefore the minimum subsidy >0 in the tail state.
  uint64_t m_supply = m_hardfork->get_current_version() < cryptonote::network_version_16 ? MONEY_SUPPLY : MONEY_SUPPLY + M_SUPPLY_ADJUST;
  already_generated_coins = base_reward < (m_supply - already_generated_coins) ? already_generated_coins + base_reward : m_supply;

  if(blockchain_height)
    cumulative_difficulty += m_db->get_block_cumulative_difficulty(blockchain_height - 1);

  TIME_MEASURE_FINISH(block_processing_time);
  if(precomputed)
    block_processing_time += m_fake_pow_calc_time;

  rtxn_guard.stop();
  TIME_MEASURE_START(addblock);
  uint64_t new_height = 0;
  uint64_t miner_unlock = 0;
  if (!bvc.m_verification_failed)
  {
    try
    {
      uint64_t long_term_block_weight = get_next_long_term_block_weight(block_weight);
      cryptonote::blobdata bd = cryptonote::block_to_blob(bl);
      new_height = m_db->add_block(std::make_pair(std::move(bl), std::move(bd)), block_weight, long_term_block_weight, cumulative_difficulty, already_generated_coins, txs);
      if(hard_fork_version >= 16)
        miner_unlock = bl.miner_tx.output_unlock_times[0];
      else
        miner_unlock = bl.miner_tx.unlock_time;

    }
    catch (const KEY_IMAGE_EXISTS& e)
    {
      LOG_ERROR("Error adding block with hash: " << id << " to blockchain, what = " << e.what());
      m_batch_success = false;
      bvc.m_verification_failed = true;
      return_tx_to_pool(txs);
      return false;
    }
    catch (const std::exception& e)
    {
      //TODO: figure out the best way to deal with this failure
      LOG_ERROR("Error adding block with hash: " << id << " to blockchain, what = " << e.what());
      m_batch_success = false;
      bvc.m_verification_failed = true;
      return_tx_to_pool(txs);
      return false;
    }
  }
  else
  {
    LOG_ERROR("Blocks that failed verification should not reach here");
  }

  auto abort_block = arqma::defer([&]()
  {
    pop_block_from_blockchain();
    auto old_height = m_db->height();
    for (BlockchainDetachedHook* hook : m_blockchain_detached_hooks)
      hook->blockchain_detached(old_height);
  });

  std::vector<transaction> arq_txs;
  arq_txs.reserve(txs.size());
  for(std::pair<transaction, blobdata> const &tx_pair : txs)
    arq_txs.push_back(tx_pair.first);

  for (BlockAddedHook* hook : m_block_added_hooks)
  {
    if (!hook->block_added(bl, arq_txs, checkpoint))
    {
      MERROR("Block added hook signalled failure");
      return false;
    }
  }

  TIME_MEASURE_FINISH(addblock);

  // do this after updating the hard fork state since the weight limit may change due to fork
  if (!update_next_cumulative_weight_limit())
  {
    MERROR("Failed to update next cumulative weight limit");
    return false;
  }

  abort_block.cancel();
  MINFO(std::endl << "****** BLOCK SUCCESSFULLY ADDED ******" << std::endl << "id:\t" << id << std::endl << "PoW:\t" << proof_of_work << std::endl << "HEIGHT " << new_height-1 << ", difficulty:\t"
         << current_diffic << std::endl << "block reward: " << print_money(base_reward + fee_summary) << "(" << print_money(base_reward) << " + " << print_money(fee_summary) << ")"
         << std::endl << "block reward unlock time: " << miner_unlock << std::endl << "coinbase_weight: " << coinbase_weight
         << ", cumulative weight: " << cumulative_block_weight << ", " << block_processing_time << "(" << target_calculating_time << "/" << longhash_calculating_time << ")ms");
  if(m_show_time_stats)
  {
    MINFO("Height: " << new_height << " coinbase weight: " << coinbase_weight << " cumm: "
        << cumulative_block_weight << " p/t: " << block_processing_time << " ("
        << target_calculating_time << "/" << longhash_calculating_time << "/"
        << t1 << "/" << t2 << "/" << t3 << "/" << t_exists << "/" << t_pool
        << "/" << t_checktx << "/" << t_dblspnd << "/" << vmt << "/" << addblock << ")ms");
  }

  bvc.m_added_to_main_chain = true;
  ++m_sync_counter;

  m_tx_pool.on_blockchain_inc(bl);
  get_difficulty_for_next_block(); // just to cache it
  invalidate_block_template_cache();

  std::shared_ptr<tools::Notify> block_notify = m_block_notify;
  if (block_notify)
    block_notify->notify("%s", epee::string_tools::pod_to_hex(id).c_str(), NULL);

  const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(new_height));

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
    rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());

  return true;
}

//------------------------------------------------------------------
bool Blockchain::prune_blockchain(uint32_t pruning_seed)
{
  auto lock = tools::unique_locks(m_tx_pool, *this);
  return m_db->prune_blockchain(pruning_seed);
}
//------------------------------------------------------------------
bool Blockchain::update_blockchain_pruning()
{
  auto lock = tools::unique_locks(m_tx_pool, *this);
  return m_db->update_pruning();
}
//------------------------------------------------------------------
bool Blockchain::check_blockchain_pruning()
{
  auto lock = tools::unique_locks(m_tx_pool, *this);
  return m_db->check_pruning();
}
//------------------------------------------------------------------
uint64_t Blockchain::get_next_long_term_block_weight(uint64_t block_weight) const
{
  PERF_TIMER(get_next_long_term_block_weight);

  const uint64_t db_height = m_db->height();
  const uint64_t nblocks = std::min<uint64_t>(m_long_term_block_weights_window, db_height);

  const uint8_t hf_version = get_current_hard_fork_version();
  if (hf_version < HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
    return block_weight;

  uint64_t long_term_median = get_long_term_block_weight_median(db_height - nblocks, nblocks);
  uint64_t long_term_effective_median_block_weight = std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, long_term_median);

  uint64_t short_term_constraint = long_term_effective_median_block_weight + long_term_effective_median_block_weight * 2 / 5;
  uint64_t long_term_block_weight = std::min<uint64_t>(block_weight, short_term_constraint);

  return long_term_block_weight;
}
//------------------------------------------------------------------
bool Blockchain::update_next_cumulative_weight_limit(uint64_t *long_term_effective_median_block_weight)
{
  PERF_TIMER(update_next_cumulative_weight_limit);

  LOG_PRINT_L3("Blockchain::" << __func__);

  // when we reach this, the last hf version is not yet written to the db
  const uint64_t db_height = m_db->height();
  const uint8_t hf_version = get_current_hard_fork_version();
  uint64_t full_reward_zone = get_min_block_weight(hf_version);
  uint64_t long_term_block_weight;

  if (hf_version < HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
  {
    std::vector<uint64_t> weights;
    get_last_n_blocks_weights(weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
    m_current_block_cumul_weight_median = epee::misc_utils::median(weights);
  }
  else
  {
    const uint64_t block_weight = m_db->get_block_weight(db_height - 1);

    uint64_t long_term_median;
    if (db_height == 1)
    {
      long_term_median = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5;
    }
    else
    {
      uint64_t nblocks = std::min<uint64_t>(m_long_term_block_weights_window, db_height);
      if (nblocks == db_height)
        --nblocks;
      long_term_median = get_long_term_block_weight_median(db_height - nblocks - 1, nblocks);
    }

    m_long_term_effective_median_block_weight = std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, long_term_median);

    uint64_t short_term_constraint = m_long_term_effective_median_block_weight + m_long_term_effective_median_block_weight * 2 / 5;
    uint64_t long_term_block_weight = std::min<uint64_t>(block_weight, short_term_constraint);

    if(db_height == 1)
    {
      long_term_median = long_term_block_weight;
    }
    else
    {
      m_long_term_block_weights_cache_tip_hash = m_db->get_block_hash_from_height(db_height - 1);
      m_long_term_block_weights_cache_rolling_median.insert(long_term_block_weight);
      long_term_median = m_long_term_block_weights_cache_rolling_median.median();
    }
    m_long_term_effective_median_block_weight = std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, long_term_median);

    std::vector<uint64_t> weights;
    get_last_n_blocks_weights(weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW);

    uint64_t short_term_median = epee::misc_utils::median(weights);
    uint64_t effective_median_block_weight = std::min<uint64_t>(std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, short_term_median), CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR * m_long_term_effective_median_block_weight);

    m_current_block_cumul_weight_median = effective_median_block_weight;
  }

  if (m_current_block_cumul_weight_median <= full_reward_zone)
    m_current_block_cumul_weight_median = full_reward_zone;

  m_current_block_cumul_weight_limit = m_current_block_cumul_weight_median * 2;

  if (long_term_effective_median_block_weight)
    *long_term_effective_median_block_weight = m_long_term_effective_median_block_weight;

  if(!m_db->is_read_only())
    m_db->add_max_block_size(m_current_block_cumul_weight_limit);

  return true;
}
//------------------------------------------------------------------
bool Blockchain::add_new_block(const block& bl, block_verification_context& bvc, checkpoint_t const *checkpoint)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  crypto::hash id = get_block_hash(bl);
  auto lock = tools::unique_locks(m_tx_pool, *this);
  db_rtxn_guard rtxn_guard(m_db);
  if(have_block(id))
  {
    LOG_PRINT_L3("block with id = " << id << " already exists");
    bvc.m_already_exists = true;
    m_blocks_txs_check.clear();
    return false;
  }

  if (checkpoint)
  {
    checkpoint_t existing_checkpoint;
    uint64_t block_height = get_block_height(bl);
    try
    {
      if (get_checkpoint(block_height, existing_checkpoint))
      {
        if (checkpoint->signatures.size() < existing_checkpoint.signatures.size())
          checkpoint = nullptr;
      }
    }
    catch (const std::exception &e)
    {
      MERROR("Get block checkpoint from DB failed at height: " << block_height << ", what = " << e.what());
    }
  }

  bool result = false;
  rtxn_guard.stop();
  //check that block refers to chain tail
  if(bl.prev_id == get_tail_id())
  {
    result = handle_block_to_main_chain(bl, id, bvc, checkpoint);
  }
  else
  {
    //chain switching or wrong block
    bvc.m_added_to_main_chain = false;
    result = handle_alternative_block(bl, id, bvc, checkpoint);
    m_blocks_txs_check.clear();
    //never relay alternative blocks
  }

  return result;
}
//------------------------------------------------------------------
bool Blockchain::update_checkpoints_from_json_file(const std::string& file_path)
{
  std::vector<height_to_hash> checkpoint_hashes;
  if(!cryptonote::load_checkpoints_from_json(file_path, checkpoint_hashes))
    return false;

  std::vector<height_to_hash>::const_iterator first_to_check = checkpoint_hashes.end();
  std::vector<height_to_hash>::const_iterator one_past_last_to_check = checkpoint_hashes.end();

  uint64_t prev_max_height = m_checkpoints.get_max_height();
  LOG_PRINT_L1("Adding checkpoints from blockchain hashfile: " << file_path);
  LOG_PRINT_L1("Hard-coded max checkpoint height is: " << prev_max_height);
  for(std::vector<height_to_hash>::const_iterator it = checkpoint_hashes.begin(); it != one_past_last_to_check; it++)
  {
    uint64_t height;
    height = it->height;
    if(height <= prev_max_height)
    {
      LOG_PRINT_L1("Ignoring checkpoint height: " << height);
    }
    else
    {
      if(first_to_check == checkpoint_hashes.end())
        first_to_check = it;

      std::string blockhash = it->hash;
      LOG_PRINT_L1("Adding checkpoint height: " << height << ", hash: " << blockhash);

      if(!m_checkpoints.add_checkpoint(height, blockhash))
      {
        one_past_last_to_check = it;
        LOG_PRINT_L1("Failed to add checkpoint at height: " << height << ", hash: " << blockhash);
        break;
      }
    }
  }

  bool result = true;
  {
    std::unique_lock lock{*this};
    bool stop_batch = m_db->batch_start();

    for(std::vector<height_to_hash>::const_iterator it = first_to_check; it != one_past_last_to_check; it++)
    {
      uint64_t block_height = it->height;
      if(block_height >= m_db->height())
        break;

      if(m_checkpoints.check_block(block_height, m_db->get_block_hash_from_height(block_height), nullptr))
      {
        LOG_ERROR("Local blockchain failed to pass a checkpoint, rolling back!");
        std::list<block_and_checkpoint> empty;
        rollback_blockchain_switching(empty, block_height - 2);
        result = false;
      }
    }

    if(stop_batch)
      m_db->batch_stop();
  }

  return result;
}
//------------------------------------------------------------------
bool Blockchain::update_checkpoint(cryptonote::checkpoint_t const &checkpoint)
{
  std::unique_lock lock{*this};
  bool result = m_checkpoints.update_checkpoint(checkpoint);
  return result;
}
//------------------------------------------------------------------
bool Blockchain::get_checkpoint(uint64_t height, checkpoint_t &checkpoint) const
{
  std::unique_lock lock{*this};
  return m_checkpoints.get_checkpoint(height, checkpoint);
}
//------------------------------------------------------------------
void Blockchain::block_longhash_worker(uint64_t height, const epee::span<const block> &blocks, std::unordered_map<crypto::hash, crypto::hash> &map) const
{
  TIME_MEASURE_START(t);
  rx_slow_hash_allocate_state();

  for (const auto & block : blocks)
  {
    if (m_cancel)
       break;
    crypto::hash id = get_block_hash(block);
    crypto::hash pow = get_block_longhash(this, block, height++, 0);
    map.emplace(id, pow);
  }

  rx_slow_hash_free_state();
  TIME_MEASURE_FINISH(t);
}

//------------------------------------------------------------------
bool Blockchain::cleanup_handle_incoming_blocks(bool force_sync)
{
  bool success = false;

  MTRACE("Blockchain::" << __func__);
  TIME_MEASURE_START(t1);

  try
  {
    if(m_batch_success)
      m_db->batch_stop();
    else
      m_db->batch_abort();
    success = true;
  }
  catch (const std::exception& e)
  {
    MERROR("Exception in cleanup_handle_incoming_blocks: " << e.what());
  }

  if (success && m_sync_counter > 0)
  {
    if (force_sync)
    {
      if(m_db_sync_mode != db_nosync)
        store_blockchain();
      m_sync_counter = 0;
    }
    else if (m_db_sync_threshold && ((m_db_sync_on_blocks && m_sync_counter >= m_db_sync_threshold) || (!m_db_sync_on_blocks && m_bytes_to_sync >= m_db_sync_threshold)))
    {
      MDEBUG("Sync threshold met, syncing");
      if(m_db_sync_mode == db_async)
      {
        m_sync_counter = 0;
        m_bytes_to_sync = 0;
        m_async_service.get_executor().dispatch([this] { return store_blockchain(); }, std::allocator<void>{});
      }
      else if(m_db_sync_mode == db_sync)
      {
        store_blockchain();
      }
      else // db_nosync
      {
        // DO NOTHING, not required to call sync.
      }
    }
  }

  TIME_MEASURE_FINISH(t1);
  m_blocks_longhash_table.clear();
  m_scan_table.clear();
  m_blocks_txs_check.clear();

  // when we're well clear of the precomputed hashes, free the memory
  if (!m_blocks_hash_check.empty() && m_db->height() > m_blocks_hash_check.size() + 4096)
  {
    MINFO("Dumping block hashes, we're now 4k past " << m_blocks_hash_check.size());
    m_blocks_hash_check.clear();
    m_blocks_hash_check.shrink_to_fit();
  }

  unlock();
  m_tx_pool.unlock();

  update_blockchain_pruning();

  return success;
}

//------------------------------------------------------------------
void Blockchain::output_scan_worker(const uint64_t amount, const std::vector<uint64_t> &offsets, std::vector<output_data_t> &outputs) const
{
  try
  {
    m_db->get_output_key(epee::span<const uint64_t>(&amount, 1), offsets, outputs, true);
  }
  catch (const std::exception& e)
  {
    MERROR_VER("EXCEPTION: " << e.what());
  }
  catch (...)
  {

  }
}

uint64_t Blockchain::prevalidate_block_hashes(uint64_t height, const std::vector<crypto::hash> &hashes)
{
  // new: . . . . . X X X X X . . . . . .
  // pre: A A A A B B B B C C C C D D D D

  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // easy case: height >= hashes
  if (height >= m_blocks_hash_of_hashes.size() * HASH_OF_HASHES_STEP)
    return hashes.size();

  // if we're getting old blocks, we might have jettisoned the hashes already
  if (m_blocks_hash_check.empty())
    return hashes.size();

  // find hashes encompassing those block
  size_t first_index = height / HASH_OF_HASHES_STEP;
  size_t last_index = (height + hashes.size() - 1) / HASH_OF_HASHES_STEP;
  MDEBUG("Blocks " << height << " - " << (height + hashes.size() - 1) << " start at " << first_index << " and end at " << last_index);

  // case of not enough to calculate even a single hash
  if (first_index == last_index && hashes.size() < HASH_OF_HASHES_STEP && (height + hashes.size()) % HASH_OF_HASHES_STEP)
    return hashes.size();

  // build hashes vector to hash hashes together
  std::vector<crypto::hash> data;
  data.reserve(hashes.size() + HASH_OF_HASHES_STEP - 1); // may be a bit too much

  // we expect height to be either equal or a bit below db height
  bool disconnected = (height > m_db->height());
  size_t pop;
  if (disconnected && height % HASH_OF_HASHES_STEP)
  {
    ++first_index;
    pop = HASH_OF_HASHES_STEP - height % HASH_OF_HASHES_STEP;
  }
  else
  {
    // we might need some already in the chain for the first part of the first hash
    for (uint64_t h = first_index * HASH_OF_HASHES_STEP; h < height; ++h)
    {
      data.push_back(m_db->get_block_hash_from_height(h));
    }
    pop = 0;
  }

  // push the data to check
  for(const auto &h : hashes)
  {
    if (pop)
      --pop;
    else
      data.push_back(h);
  }

  // hash and check
  uint64_t usable = first_index * HASH_OF_HASHES_STEP - height; // may start negative, but unsigned under/overflow is not UB
  for (size_t n = first_index; n <= last_index; ++n)
  {
    if (n < m_blocks_hash_of_hashes.size())
    {
      // if the last index isn't fully filled, we can't tell if valid
      if (data.size() < (n - first_index) * HASH_OF_HASHES_STEP + HASH_OF_HASHES_STEP)
        break;

      crypto::hash hash;
      cn_fast_hash(data.data() + (n - first_index) * HASH_OF_HASHES_STEP, HASH_OF_HASHES_STEP * sizeof(crypto::hash), hash);
      bool valid = hash == m_blocks_hash_of_hashes[n];

      // add to the known hashes array
      if (!valid)
      {
        MDEBUG("invalid hash for blocks " << n * HASH_OF_HASHES_STEP << " - " << (n * HASH_OF_HASHES_STEP + HASH_OF_HASHES_STEP - 1));
        break;
      }

      size_t end = n * HASH_OF_HASHES_STEP + HASH_OF_HASHES_STEP;
      for (size_t i = n * HASH_OF_HASHES_STEP; i < end; ++i)
      {
        CHECK_AND_ASSERT_MES(m_blocks_hash_check[i] == crypto::null_hash || m_blocks_hash_check[i] == data[i - first_index * HASH_OF_HASHES_STEP],
            0, "Consistency failure in m_blocks_hash_check construction");
        m_blocks_hash_check[i] = data[i - first_index * HASH_OF_HASHES_STEP];
      }
      usable += HASH_OF_HASHES_STEP;
    }
    else
    {
      // if after the end of the precomputed blocks, accept anything
      usable += HASH_OF_HASHES_STEP;
      if (usable > hashes.size())
        usable = hashes.size();
    }
  }
  MDEBUG("usable: " << usable << " / " << hashes.size());
  CHECK_AND_ASSERT_MES(usable < std::numeric_limits<uint64_t>::max() / 2, 0, "usable is negative");
  return usable;
}
//------------------------------------------------------------------
// ND: Speedups:
// 1. Thread long_hash computations if possible (m_max_prepare_blocks_threads = nthreads, default = 4)
// 2. Group all amounts (from txs) and related absolute offsets and form a table of tx_prefix_hash
//    vs [k_image, output_keys] (m_scan_table). This is faster because it takes advantage of bulk queries
//    and is threaded if possible. The table (m_scan_table) will be used later when querying output
//    keys.
bool Blockchain::prepare_handle_incoming_blocks(const std::vector<block_complete_entry> &blocks_entry, std::vector<block> &blocks)
{
  MTRACE("Blockchain::" << __func__);
  TIME_MEASURE_START(prepare);
  uint64_t bytes = 0;
  size_t total_txs = 0;
  blocks.clear();

  // Order of locking must be:
  //  m_incoming_tx_lock (optional)
  //  m_tx_pool lock
  //  blockchain lock
  //
  //  Something which takes the blockchain lock may never take the txpool lock
  //  if it has not provably taken the txpool lock earlier
  //
  //  The txpool lock is now taken in prepare_handle_incoming_blocks
  //  and released in cleanup_handle_incoming_blocks. This avoids issues
  //  when something uses the pool, which now uses the blockchain and
  //  needs a batch, since a batch could otherwise be active while the
  //  txpool and blockchain locks were not held

  std::lock(m_tx_pool, *this);

  if(blocks_entry.size() == 0)
    return false;

  for (const auto &entry : blocks_entry)
  {
    bytes += entry.block.size();
    bytes += entry.checkpoint.size();
    for (const auto &tx_blob : entry.txs)
    {
      bytes += tx_blob.size();
    }
    total_txs += entry.txs.size();
  }
  m_bytes_to_sync += bytes;
  while (!m_db->batch_start(blocks_entry.size(), bytes)) {
    unlock();
    m_tx_pool.unlock();
    epee::misc_utils::sleep_no_w(100);
    std::lock(m_tx_pool, *this);
  }
  m_batch_success = true;

  const uint64_t height = m_db->height();
  if ((height + blocks_entry.size()) < m_blocks_hash_check.size())
    return true;

  bool blocks_exist = false;
  tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
  unsigned threads = tpool.get_max_concurrency();
  blocks.resize(blocks_entry.size());

  {
    // limit threads, default limit = 4
    if(threads > m_max_prepare_blocks_threads)
      threads = m_max_prepare_blocks_threads;

    unsigned int batches = blocks_entry.size() / threads;
    unsigned int extra = blocks_entry.size() % threads;
    MDEBUG("block_batches: " << batches);
    std::vector<std::unordered_map<crypto::hash, crypto::hash>> maps(threads);
    auto it = blocks_entry.begin();
    unsigned blockidx = 0;

    const crypto::hash tophash = m_db->top_block_hash();
    for (unsigned i = 0; i < threads; i++)
    {
      for (unsigned int j = 0; j < batches; j++, ++blockidx)
      {
        block &block = blocks[blockidx];
        crypto::hash block_hash;

        if (!parse_and_validate_block_from_blob(it->block, block, block_hash))
          return false;

        // check first block and skip all blocks if its not chained properly
        if (blockidx == 0)
        {
          if (block.prev_id != tophash)
          {
            MDEBUG("Skipping prepare blocks. New blocks don't belong to chain.");
            blocks.clear();
            return true;
          }
        }
        if (have_block(block_hash))
          blocks_exist = true;

        std::advance(it, 1);
      }
    }

    for (unsigned i = 0; i < extra && !blocks_exist; i++, blockidx++)
    {
      block &block = blocks[blockidx];
      crypto::hash block_hash;

      if (!parse_and_validate_block_from_blob(it->block, block, block_hash))
        return false;

      if (have_block(block_hash))
        blocks_exist = true;

      std::advance(it, 1);
    }

    if (!blocks_exist)
    {
      m_blocks_longhash_table.clear();
      uint64_t thread_height = height;
      tools::threadpool::waiter waiter(tpool);
      m_prepare_height = height;
      m_prepare_nblocks = blocks_entry.size();
      m_prepare_blocks = &blocks;
      for (unsigned int i = 0; i < threads; i++)
      {
        unsigned nblocks = batches;
        if (i < extra)
          ++nblocks;
        tpool.submit(&waiter, boost::bind(&Blockchain::block_longhash_worker, this, thread_height, epee::span<const block>(&blocks[thread_height - height], nblocks), std::ref(maps[i])), true);
        thread_height += nblocks;
      }

      if (!waiter.wait())
        return false;
      m_prepare_height = 0;

      if (m_cancel)
        return false;

      for (const auto & map : maps)
      {
        m_blocks_longhash_table.insert(map.begin(), map.end());
      }
    }
  }

  if (m_cancel)
    return false;

  if (blocks_exist)
  {
    MDEBUG("Skipping prepare blocks. Blocks exist.");
    return true;
  }

  m_fake_scan_time = 0;
  m_fake_pow_calc_time = 0;

  m_scan_table.clear();

  TIME_MEASURE_FINISH(prepare);
  m_fake_pow_calc_time = prepare / blocks_entry.size();

  if (blocks_entry.size() > 1 && threads > 1 && m_show_time_stats)
    MDEBUG("Prepare blocks took: " << prepare << " ms");

  TIME_MEASURE_START(scantable);

  // [input] stores all unique amounts found
  std::vector<uint64_t> amounts;
  // [input] stores all absolute_offsets for each amount
  std::map<uint64_t, std::vector<uint64_t>> offset_map;
  // [output] stores all output_data_t for each absolute_offset
  std::map<uint64_t, std::vector<output_data_t>> tx_map;
  std::vector<std::pair<cryptonote::transaction, crypto::hash>> txes(total_txs);

#define SCAN_TABLE_QUIT(m) \
        do { \
            MERROR_VER(m) ;\
            m_scan_table.clear(); \
            return false; \
        } while(0); \

  // generate sorted tables for all amounts and absolute offsets
  size_t tx_index = 0, block_index = 0;
  for (const auto &entry : blocks_entry)
  {
    if (m_cancel)
      return false;

    for (const auto &tx_blob : entry.txs)
    {
      if (tx_index >= txes.size())
        SCAN_TABLE_QUIT("tx_index is out of sync");
      transaction &tx = txes[tx_index].first;
      crypto::hash &tx_prefix_hash = txes[tx_index].second;
      ++tx_index;

      if (!parse_and_validate_tx_base_from_blob(tx_blob, tx))
        SCAN_TABLE_QUIT("Could not parse tx from incoming blocks.");
      cryptonote::get_transaction_prefix_hash(tx, tx_prefix_hash);

      auto its = m_scan_table.find(tx_prefix_hash);
      if (its != m_scan_table.end())
        SCAN_TABLE_QUIT("Duplicate tx found from incoming blocks.");

      m_scan_table.emplace(tx_prefix_hash, std::unordered_map<crypto::key_image, std::vector<output_data_t>>());
      its = m_scan_table.find(tx_prefix_hash);
      assert(its != m_scan_table.end());

      // get all amounts from tx.vin(s)
      for (const auto &txin : tx.vin)
      {
        const txin_to_key &in_to_key = boost::get<txin_to_key>(txin);

        // check for duplicate
        auto it = its->second.find(in_to_key.k_image);
        if (it != its->second.end())
          SCAN_TABLE_QUIT("Duplicate key_image found from incoming blocks.");

        amounts.push_back(in_to_key.amount);
      }

      // sort and remove duplicate amounts from amounts list
      std::sort(amounts.begin(), amounts.end());
      auto last = std::unique(amounts.begin(), amounts.end());
      amounts.erase(last, amounts.end());

      // add amount to the offset_map and tx_map
      for (const uint64_t &amount : amounts)
      {
        if (offset_map.find(amount) == offset_map.end())
          offset_map.emplace(amount, std::vector<uint64_t>());

        if (tx_map.find(amount) == tx_map.end())
          tx_map.emplace(amount, std::vector<output_data_t>());
      }

      // add new absolute_offsets to offset_map
      for (const auto &txin : tx.vin)
      {
        const txin_to_key &in_to_key = boost::get< txin_to_key >(txin);
        // no need to check for duplicate here.
        auto absolute_offsets = relative_output_offsets_to_absolute(in_to_key.key_offsets);
        for (const auto & offset : absolute_offsets)
          offset_map[in_to_key.amount].push_back(offset);

      }
    }
    ++block_index;
  }

  // sort and remove duplicate absolute_offsets in offset_map
  for (auto &offsets : offset_map)
  {
    std::sort(offsets.second.begin(), offsets.second.end());
    auto last = std::unique(offsets.second.begin(), offsets.second.end());
    offsets.second.erase(last, offsets.second.end());
  }

  // gather all the output keys
  threads = tpool.get_max_concurrency();
  if (!m_db->can_thread_bulk_indices())
    threads = 1;

  if (threads > 1 && amounts.size() > 1)
  {
    tools::threadpool::waiter waiter(tpool);

    for (size_t i = 0; i < amounts.size(); i++)
    {
      uint64_t amount = amounts[i];
      tpool.submit(&waiter, boost::bind(&Blockchain::output_scan_worker, this, amount, std::cref(offset_map[amount]), std::ref(tx_map[amount])), true);
    }
    if (!waiter.wait())
      return false;
  }
  else
  {
    for (size_t i = 0; i < amounts.size(); i++)
    {
      uint64_t amount = amounts[i];
      output_scan_worker(amount, offset_map[amount], tx_map[amount]);
    }
  }

  // now generate a table for each tx_prefix and k_image hashes
  tx_index = 0;
  for (const auto &entry : blocks_entry)
  {
    if (m_cancel)
      return false;

    for (const auto &tx_blob : entry.txs)
    {
      if (tx_index >= txes.size())
        SCAN_TABLE_QUIT("tx_index is out of sync");
      const transaction &tx = txes[tx_index].first;
      const crypto::hash &tx_prefix_hash = txes[tx_index].second;
      ++tx_index;

      auto its = m_scan_table.find(tx_prefix_hash);
      if (its == m_scan_table.end())
        SCAN_TABLE_QUIT("Tx not found on scan table from incoming blocks.");

      for (const auto &txin : tx.vin)
      {
        const txin_to_key &in_to_key = boost::get< txin_to_key >(txin);
        auto needed_offsets = relative_output_offsets_to_absolute(in_to_key.key_offsets);

        std::vector<output_data_t> outputs;
        for (const uint64_t & offset_needed : needed_offsets)
        {
          size_t pos = 0;
          bool found = false;

          for (const uint64_t &offset_found : offset_map[in_to_key.amount])
          {
            if (offset_needed == offset_found)
            {
              found = true;
              break;
            }

            ++pos;
          }

          if (found && pos < tx_map[in_to_key.amount].size())
            outputs.push_back(tx_map[in_to_key.amount].at(pos));
          else
            break;
        }

        its->second.emplace(in_to_key.k_image, outputs);
      }
    }
  }

  TIME_MEASURE_FINISH(scantable);
  if (total_txs > 0)
  {
    m_fake_scan_time = scantable / total_txs;
    if(m_show_time_stats)
      MDEBUG("Prepare scantable took: " << scantable << " ms");
  }

  return true;
}

void Blockchain::add_txpool_tx(const crypto::hash &txid, const cryptonote::blobdata &blob, const txpool_tx_meta_t &meta)
{
  m_db->add_txpool_tx(txid, blob, meta);
}

void Blockchain::update_txpool_tx(const crypto::hash &txid, const txpool_tx_meta_t &meta)
{
  m_db->update_txpool_tx(txid, meta);
}

void Blockchain::remove_txpool_tx(const crypto::hash &txid)
{
  m_db->remove_txpool_tx(txid);
}

uint64_t Blockchain::get_txpool_tx_count(bool include_unrelayed_txes) const
{
  return m_db->get_txpool_tx_count(include_unrelayed_txes);
}

bool Blockchain::get_txpool_tx_meta(const crypto::hash& txid, txpool_tx_meta_t &meta) const
{
  return m_db->get_txpool_tx_meta(txid, meta);
}

bool Blockchain::get_txpool_tx_blob(const crypto::hash& txid, cryptonote::blobdata &bd) const
{
  return m_db->get_txpool_tx_blob(txid, bd);
}

cryptonote::blobdata Blockchain::get_txpool_tx_blob(const crypto::hash& txid) const
{
  return m_db->get_txpool_tx_blob(txid);
}

bool Blockchain::for_all_txpool_txes(std::function<bool(const crypto::hash&, const txpool_tx_meta_t&, const cryptonote::blobdata*)> f, bool include_blob, bool include_unrelayed_txes) const
{
  return m_db->for_all_txpool_txes(f, include_blob, include_unrelayed_txes);
}

uint64_t Blockchain::get_immutable_height() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  checkpoint_t checkpoint;
  if (m_db->get_immutable_checkpoint(&checkpoint, get_current_blockchain_height()))
    return checkpoint.height;
  return 0;
}

void Blockchain::set_user_options(uint64_t maxthreads, bool sync_on_blocks, uint64_t sync_threshold, blockchain_db_sync_mode sync_mode, bool fast_sync)
{
  if (sync_mode == db_defaultsync)
  {
    m_db_default_sync = true;
    sync_mode = db_async;
  }
  m_db_sync_mode = sync_mode;
  m_fast_sync = fast_sync;
  m_db_sync_on_blocks = sync_on_blocks;
  m_db_sync_threshold = sync_threshold;
  m_max_prepare_blocks_threads = maxthreads;
}

void Blockchain::safesyncmode(const bool onoff)
{
  /* all of this is no-op'd if the user set a specific
   * --db-sync-mode at startup.
   */
  if (m_db_default_sync)
  {
    m_db->safesyncmode(onoff);
    m_db_sync_mode = onoff ? db_nosync : db_async;
  }
}

HardFork::State Blockchain::get_hard_fork_state() const
{
  return m_hardfork->get_state();
}

bool Blockchain::get_hard_fork_voting_info(uint8_t version, uint32_t &window, uint32_t &votes, uint32_t &threshold, uint64_t &earliest_height, uint8_t &voting) const
{
  return m_hardfork->get_voting_info(version, window, votes, threshold, earliest_height, voting);
}

uint64_t Blockchain::get_difficulty_target() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  uint8_t version = get_current_hard_fork_version();
  return get_current_diff_target(version);
}

std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t>> Blockchain::get_output_histogram(const std::vector<uint64_t> &amounts, bool unlocked, uint64_t recent_cutoff, uint64_t min_count) const
{
  return m_db->get_output_histogram(amounts, unlocked, recent_cutoff, min_count);
}

std::vector<std::pair<Blockchain::block_extended_info,std::vector<crypto::hash>>> Blockchain::get_alternative_chains() const
{
  std::vector<std::pair<Blockchain::block_extended_info,std::vector<crypto::hash>>> chains;

  blocks_ext_by_hash alt_blocks;
  alt_blocks.reserve(m_db->get_alt_block_count());
  m_db->for_all_alt_blocks([&alt_blocks](const crypto::hash &blkid, const cryptonote::alt_block_data_t &data, const cryptonote::blobdata *block_blob, const cryptonote::blobdata *checkpoint_blob)
  {
    if(!block_blob)
    {
      MERROR("No blob, but blobs were requested");
      return false;
    }

    checkpoint_t checkpoint = {};
    if (data.checkpointed && checkpoint_blob)
    {
      if (!t_serializable_object_from_blob(checkpoint, *checkpoint_blob))
        MERROR("Failed to parse checkpoint from blob");
    }

    cryptonote::block block;
    if (cryptonote::parse_and_validate_block_from_blob(*block_blob, block))
    {
      block_extended_info bei(data, std::move(block), data.checkpointed ? &checkpoint : nullptr);
      alt_blocks.insert(std::make_pair(cryptonote::get_block_hash(bei.bl), std::move(bei)));
    }
    else
      MERROR("Failed to parse block from blob");
    return true;
  }, true);

  for (const auto &i: alt_blocks)
  {
    const crypto::hash top = cryptonote::get_block_hash(i.second.bl);
    bool found = false;
    for (const auto &j: alt_blocks)
    {
      if (j.second.bl.prev_id == top)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      std::vector<crypto::hash> chain;
      auto h = i.second.bl.prev_id;
      chain.push_back(top);
      blocks_ext_by_hash::const_iterator prev;
      while ((prev = alt_blocks.find(h)) != alt_blocks.end())
      {
        chain.push_back(h);
        h = prev->second.bl.prev_id;
      }
      chains.push_back(std::make_pair(i.second, chain));
    }
  }
  return chains;
}

void Blockchain::cancel()
{
  m_cancel = true;
}

#if defined(PER_BLOCK_CHECKPOINT)
static const char expected_block_hashes_hash[] = "6eb355d11ea405e477d008fab48602139319291df76bf158e565f56bab486518";
void Blockchain::load_compiled_in_block_hashes(const GetCheckpointsCallback& get_checkpoints)
{
  if (get_checkpoints == nullptr || !m_fast_sync)
  {
    return;
  }
  const epee::span<const unsigned char> &checkpoints = get_checkpoints(m_nettype);
  if (!checkpoints.empty())
  {
    MINFO("Loading precomputed blocks (" << checkpoints.size() << " bytes)");
    if (m_nettype == MAINNET)
    {
      // first check hash
      crypto::hash hash;
      if (!tools::sha256sum(checkpoints.data(), checkpoints.size(), hash))
      {
        MERROR("Failed to hash precomputed blocks data");
        return;
      }
      MINFO("precomputed blocks hash: " << hash << ", expected " << expected_block_hashes_hash);
      cryptonote::blobdata expected_hash_data;
      if (!epee::string_tools::parse_hexstr_to_binbuff(std::string(expected_block_hashes_hash), expected_hash_data) || expected_hash_data.size() != sizeof(crypto::hash))
      {
        MERROR("Failed to parse expected block hashes hash");
        return;
      }
      const crypto::hash expected_hash = *reinterpret_cast<const crypto::hash*>(expected_hash_data.data());
      if (hash != expected_hash)
      {
        MERROR("Block hash data does not match expected hash");
        return;
      }
    }

    if (checkpoints.size() > 4)
    {
      const unsigned char *p = checkpoints.data();
      const uint32_t nblocks = *p | ((*(p+1))<<8) | ((*(p+2))<<16) | ((*(p+3))<<24);
      if (nblocks > (std::numeric_limits<uint32_t>::max() - 4) / sizeof(hash))
      {
        MERROR("Block hash data is too large");
        return;
      }
      const size_t size_needed = 4 + nblocks * sizeof(crypto::hash);
      if(checkpoints.size() != size_needed)
      {
        MERROR("Failed to load hashes - unexpected data size");
        return;
      }
      else if(nblocks > 0 && nblocks > (m_db->height() + HASH_OF_HASHES_STEP - 1) / HASH_OF_HASHES_STEP)
      {
        p += sizeof(uint32_t);
        m_blocks_hash_of_hashes.reserve(nblocks);
        for (uint32_t i = 0; i < nblocks; i++)
        {
          crypto::hash hash;
          memcpy(hash.data, p, sizeof(hash.data));
          p += sizeof(hash.data);
          m_blocks_hash_of_hashes.push_back(hash);
        }
        m_blocks_hash_check.resize(m_blocks_hash_of_hashes.size() * HASH_OF_HASHES_STEP, crypto::null_hash);
        MINFO(nblocks << " block hashes loaded");

        // FIXME: clear tx_pool because the process might have been
        // terminated and caused it to store txs kept by blocks.
        // The core will not call check_tx_inputs(..) for these
        // transactions in this case. Consequently, the sanity check
        // for tx hashes will fail in handle_block_to_main_chain(..)
        std::unique_lock lock{m_tx_pool};

        std::vector<transaction> txs;
        m_tx_pool.get_transactions(txs, true);

        size_t tx_weight;
        uint64_t fee;
        bool relayed, do_not_relay, double_spend_seen;
        transaction pool_tx;
        blobdata txblob;
        for(const transaction &tx : txs)
        {
          crypto::hash tx_hash = get_transaction_hash(tx);
          m_tx_pool.take_tx(tx_hash, pool_tx, txblob, tx_weight, fee, relayed, do_not_relay, double_spend_seen);
        }
      }
    }
  }
}
#endif

bool Blockchain::is_within_compiled_block_hash_area(uint64_t height) const
{
#if defined(PER_BLOCK_CHECKPOINT)
  return height < m_blocks_hash_of_hashes.size() * HASH_OF_HASHES_STEP;
#else
  return false;
#endif
}

bool Blockchain::for_all_key_images(std::function<bool(const crypto::key_image&)> f) const
{
  return m_db->for_all_key_images(f);
}

bool Blockchain::for_blocks_range(const uint64_t& h1, const uint64_t& h2, std::function<bool(uint64_t, const crypto::hash&, const block&)> f) const
{
  return m_db->for_blocks_range(h1, h2, f);
}

bool Blockchain::for_all_transactions(std::function<bool(const crypto::hash&, const cryptonote::transaction&)> f, bool pruned) const
{
  return m_db->for_all_transactions(f, pruned);
}

bool Blockchain::for_all_outputs(std::function<bool(uint64_t amount, const crypto::hash &tx_hash, uint64_t height, size_t tx_idx)> f) const
{
  return m_db->for_all_outputs(f);
}

bool Blockchain::for_all_outputs(uint64_t amount, std::function<bool(uint64_t height)> f) const
{
  return m_db->for_all_outputs(amount, f);
}

void Blockchain::invalidate_block_template_cache()
{
  MDEBUG("Invalidating block template cache");
  m_btc_valid = false;
}

void Blockchain::cache_block_template(const block &b, const cryptonote::account_public_address &address, const blobdata &nonce, const difficulty_type &diff, uint64_t height, uint64_t expected_reward, uint64_t seed_height, const crypto::hash &seed_hash, uint64_t pool_cookie)
{
  MDEBUG("Setting block template cache");
  m_btc = b;
  m_btc_address = address;
  m_btc_nonce = nonce;
  m_btc_difficulty = diff;
  m_btc_height = height;
  m_btc_expected_reward = expected_reward;
  m_btc_seed_hash = seed_hash;
  m_btc_seed_height = seed_height;
  m_btc_pool_cookie = pool_cookie;
  m_btc_valid = true;
}
