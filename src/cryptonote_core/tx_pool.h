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

#pragma once
#include "include_base_utils.h"

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <boost/serialization/version.hpp>

#include "string_tools.h"
#include "syncobj.h"
#include "common/periodic_task.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/verification_context.h"
#include "blockchain_db/blockchain_db.h"
#include "crypto/hash.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "rpc/message_data_structs.h"

namespace cryptonote
{
  class Blockchain;
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  using namespace std::literals;

  //! tuple of <deregister, transaction fee, receive time> for organization
  typedef std::pair<std::tuple<bool, double, std::time_t>, crypto::hash> tx_by_fee_and_receive_time_entry;

  class txCompare
  {
  public:
    bool operator()(const tx_by_fee_and_receive_time_entry& a, const tx_by_fee_and_receive_time_entry& b) const
    {
      // Sort order:         non-standard txes,     fee                    arrival time          hash
      return std::make_tuple(!std::get<0>(a.first), -std::get<1>(a.first), std::get<2>(a.first), a.second)
           < std::make_tuple(!std::get<0>(b.first), -std::get<1>(b.first), std::get<2>(b.first), b.second);
    }
  };

  //! container for sorting transactions by fee per unit size
  typedef std::set<tx_by_fee_and_receive_time_entry, txCompare> sorted_tx_container;

  struct tx_pool_options
  {
    bool kept_by_block = false;
    bool relayed = false;
    bool do_not_relay = false;

    static tx_pool_options from_block() { tx_pool_options o; o.kept_by_block = true; o.relayed = true; return o; }
    static tx_pool_options from_peer() { tx_pool_options o; o.relayed = true; return o; }
    static tx_pool_options new_tx(bool do_not_relay = false) { tx_pool_options o; o.do_not_relay = do_not_relay; return o; }
  };

  /**
   * @brief Transaction pool, handles transactions which are not part of a block
   *
   * This class handles all transactions which have been received, but not as
   * part of a block.
   *
   * This handling includes:
   *   storing the transactions
   *   organizing the transactions by fee per weight unit
   *   taking/giving transactions to and from various other components
   *   saving the transactions to disk on shutdown
   *   helping create a new block template by choosing transactions for it
   *
   */
  class tx_memory_pool
  {
  public:
    /**
     * @brief Constructor
     *
     * @param bchs a Blockchain class instance, for getting chain info
     */
    tx_memory_pool(Blockchain& bchs);

    tx_memory_pool(const tx_memory_pool &) = delete;
    tx_memory_pool &operator=(const tx_memory_pool &) = delete;


    /**
     * @copydoc add_tx(transaction&, tx_verification_context&, const tx_pool_options &, uint8_t)
     *
     * @param id the transaction's hash
     * @param tx_weight the transaction's weight
     */
    bool add_tx(transaction &tx, const crypto::hash &id, const cryptonote::blobdata &blob, size_t tx_weight, tx_verification_context& tvc, const tx_pool_options &opts, uint8_t hard_fork_version);

    /**
     * @brief add a transaction to the transaction pool
     *
     * Most likely the transaction will come from the network, but it is
     * also possible for transactions to come from popped blocks during
     * a reorg, or from local clients creating a transaction and
     * submitting it to the network
     *
     * @param tx the transaction to be added
     * @param tvc return-by-reference status about the transaction verification
     * @param kept_by_block has this transaction been in a block?
     * @param relayed was this transaction from the network or a local client?
     * @param do_not_relay to avoid relaying the transaction to the network
     * @param version the version used to create the transaction
     *
     * @return true if the transaction passes validations, otherwise false
     */
    bool add_tx(transaction &tx, tx_verification_context& tvc, const tx_pool_options &opts, uint8_t hard_fork_version);

    /**
     * @brief takes a transaction with the given hash from the pool
     *
     * @param id the hash of the transaction
     * @param tx return-by-reference the transaction taken
     * @param txblob return-by-reference the transaction as a blob
     * @param tx_weight return-by-reference the transaction's weight
     * @param fee the transaction fee
     * @param relayed return-by-reference was transaction relayed to us by the network?
     * @param do_not_relay return-by-reference is transaction not to be relayed to the network?
     * @param double_spend_seen return-by-reference was a double spend seen for that transaction?
     * @param pruned return-by-reference is the tx pruned
     *
     * @return true unless the transaction cannot be found in the pool
     */
    bool take_tx(const crypto::hash &id, transaction &tx, cryptonote::blobdata &txblob, size_t& tx_weight, uint64_t& fee, bool &relayed, bool &do_not_relay, bool &double_spend_seen);

    /**
     * @brief checks if the pool has a transaction with the given hash
     *
     * @param id the hash to look for
     *
     * @return true if the transaction is in the pool, otherwise false
     */
    bool have_tx(const crypto::hash &id) const;

    std::vector<uint8_t> have_txs(const std::vector<crypto::hash> &hashes) const;

    /**
     * @brief action to take when notified of a block added to the blockchain
     *
     * @param new_block_height the height of the blockchain after the change
     * @param top_block_id the hash of the new top block
     *
     * @return true
     */
    bool on_blockchain_inc(block const &blk);

    /**
     * @brief action to take when notified of a block removed from the blockchain
     *
     * @param new_block_height the height of the blockchain after the change
     * @param top_block_id the hash of the new top block
     *
     * @return true
     */
    bool on_blockchain_dec();

    /**
     * @brief action to take periodically
     *
     * Currently checks transaction pool for stale ("stuck") transactions
     */
    void on_idle();

    /**
     * @brief locks the transaction pool
     */
    void lock() const { m_transactions_lock.lock(); }

    /**
     * @brief unlocks the transaction pool
     */
    void unlock() const { m_transactions_lock.unlock(); }

    bool try_lock() const { return m_transactions_lock.try_lock(); }

    // load/store operations

    /**
     * @brief loads pool state (if any) from disk, and initializes pool
     *
     * @param max_txpool_weight the max weight in bytes
     *
     * @return true
     */
    bool init(size_t max_txpool_weight = 0);

    /**
     * @brief attempts to save the transaction pool state to disk
     *
     * Currently fails (returns false) if the data directory from init()
     * does not exist and cannot be created, but returns true even if
     * saving to disk is unsuccessful.
     *
     * @return true in most cases (see above)
     */
    bool deinit();

    /**
     * @brief Chooses transactions for a block to include
     *
     * @param bl return-by-reference the block to fill in with transactions
     * @param median_weight the current median block weight
     * @param already_generated_coins the current total number of coins "minted"
     * @param total_weight return-by-reference the total weight of the new block
     * @param fee return-by-reference the total of fees from the included transactions
     * @param expected_reward return-by-reference the total reward awarded to the miner finding this block, including transaction fees
     * @param version hard fork version to use for consensus rules
     *
     * @return true
     */
    bool fill_block_template(block &bl, size_t median_weight, uint64_t already_generated_coins, size_t &total_weight, uint64_t &fee, uint64_t &expected_reward, uint8_t version, uint64_t height);

    /**
     * @brief get a list of all transactions in the pool
     *
     * @param txs return-by-reference the list of transactions
     * @param include_unrelayed_txes include unrelayed txes in the result
     *
     */
    void get_transactions(std::vector<transaction>& txs, bool include_unrelayed_txes = true) const;

    /**
     * @brief get a list of all transaction hashes in the pool
     *
     * @param txs return-by-reference the list of transactions
     * @param include_unrelayed_txes include unrelayed txes in the result
     *
     */
    void get_transaction_hashes(std::vector<crypto::hash>& txs, bool include_unrelayed_txes = true) const;

    /**
     * @brief get (weight, fee, receive time) for all transaction in the pool
     *
     * @param txs return-by-reference that data
     * @param include_unrelayed_txes include unrelayed txes in the result
     *
     */
    void get_transaction_backlog(std::vector<tx_backlog_entry>& backlog, bool include_unrelayed_txes = true) const;

    /**
     * @brief get a summary statistics of all transaction hashes in the pool
     *
     * @param stats return-by-reference the pool statistics
     * @param include_unrelayed_txes include unrelayed txes in the result
     *
     */
    void get_transaction_stats(struct txpool_stats& stats, bool include_unrelayed_txes = true) const;

    /**
     * @brief get information about all transactions and key images in the pool
     *
     * see documentation on tx_info and spent_key_image_info for more details
     *
     * @param tx_infos return-by-reference the transactions' information
     * @param key_image_infos return-by-reference the spent key images' information
     * @param include_sensitive_data include unrelayed txes and fields that are sensitive to the node privacy
     *
     * @return true
     */
    bool get_transactions_and_spent_keys_info(std::vector<tx_info>& tx_infos, std::vector<spent_key_image_info>& key_image_infos, bool include_sensitive_data = true) const;

    /**
     * @brief get information about all transactions and key images in the pool
     *
     * see documentation on tx_in_pool and key_images_with_tx_hashes for more details
     *
     * @param tx_infos [out] the transactions' information
     * @param key_image_infos [out] the spent key images' information
     *
     * @return true
     */
    bool get_pool_for_rpc(std::vector<cryptonote::rpc::tx_in_pool>& tx_infos, cryptonote::rpc::key_images_with_tx_hashes& key_image_infos) const;

    /**
     * @brief check for presence of key images in the pool
     *
     * @param key_images [in] vector of key images to check
     * @param spent [out] vector of bool to return
     *
     * @return true
     */
    bool check_for_key_images(const std::vector<crypto::key_image>& key_images, std::vector<bool> spent) const;

    /**
     * @brief get a specific transaction from the pool
     *
     * @param h the hash of the transaction to get
     * @param tx return-by-reference the transaction blob requested
     *
     * @return true if the transaction is found, otherwise false
     */
    bool get_transaction(const crypto::hash& h, cryptonote::blobdata& txblob) const;

    int find_transactions(const std::vector<crypto::hash> &tx_hashes, std::vector<cryptonote::blobdata> &txblobs) const;

    /**
     * @brief get a list of all relayable transactions and their hashes
     *
     * "relayable" in this case means:
     *   nonzero fee
     *   hasn't been relayed too recently
     *   isn't old enough that relaying it is considered harmful
     * Note a transaction can be "relayable" even if do_not_relay is true
     *
     * @param txs return-by-reference the transactions and their hashes
     *
     * @return true
     */
    bool get_relayable_transactions(std::vector<std::pair<crypto::hash, cryptonote::blobdata>>& txs) const;

    int set_relayable(const std::vector<crypto::hash> &tx_hashes);

    /**
     * @brief tell the pool that certain transactions were just relayed
     *
     * @param txs the list of transactions (and their hashes)
     */
    void set_relayed(const std::vector<std::pair<crypto::hash, cryptonote::blobdata>>& txs);

    /**
     * @brief get the total number of transactions in the pool
     *
     * @return the number of transactions in the pool
     */
    size_t get_transactions_count(bool include_unrelayed_txes = true) const;

    /**
     * @brief remove transactions from the pool which are no longer valid
     *
     * With new versions of the currency, what conditions render a transaction
     * invalid may change.  This function clears those which were received
     * before a version change and no longer conform to requirements.
     *
     * @param version the version the transactions must conform to
     *
     * @return the number of transactions removed
     */
    size_t validate(uint8_t version);

    /**
      * @brief return the cookie
      *
      * @return the cookie
      */
    uint64_t cookie() const { return m_cookie; }


    /**
     * @brief get the cumulative txpool weight in bytes
     *
     * @return the cumulative txpool weight in bytes
     */
    size_t get_txpool_weight() const;

    /**
     * @brief set the max cumulative txpool weight in bytes
     *
     * @param bytes the max cumulative txpool weight in bytes
     */
    void set_txpool_max_weight(size_t bytes);

  private:

    /**
     * @brief insert key images into m_spent_key_images
     *
     * @return true on success, false on error
     */
    bool insert_key_images(const transaction_prefix &tx, const crypto::hash &txid, bool kept_by_block);

    /**
     * @brief remove old transactions from the pool
     *
     * After a certain time, it is assumed that a transaction which has not
     * yet been mined will likely not be mined.  These transactions are removed
     * from the pool to avoid buildup.
     *
     * @return true
     */
    bool remove_stuck_transactions();

    /**
     * @brief check if a transaction in the pool has a given spent key image
     *
     * @param key_im the spent key image to look for
     *
     * @return true if the spent key image is present, otherwise false
     */
    bool have_tx_keyimg_as_spent(const crypto::key_image& key_im) const;

    /**
     * @brief check if TX that does not have a key_image component has any duplicates in the pool
     * @return true if it already exists
     *
     */
    bool have_duplicated_non_standard_tx(transaction const &tx, uint8_t hard_fork_version) const;

    /**
     * @brief check if any spent key image in a transaction is in the pool
     *
     * Checks if any of the spent key images in a given transaction are present
     * in any of the transactions in the transaction pool.
     *
     * @note see tx_pool::have_tx_keyimg_as_spent
     *
     * @param tx the transaction to check spent key images of
     *
     * @return true if any spent key images are present in the pool, otherwise false
     */
    bool have_tx_keyimges_as_spent(const transaction& tx) const;

    /**
     * @brief forget a transaction's spent key images
     *
     * Spent key images are stored separately from transactions for
     * convenience/speed, so this is part of the process of removing
     * a transaction from the pool.
     *
     * @param tx the transaction
     * @param txid the transaction's hash
     *
     * @return false if any key images to be removed cannot be found, otherwise true
     */
    bool remove_transaction_keyimages(const transaction_prefix& tx, const crypto::hash &txid);

    /**
     * @brief check if any of a transaction's spent key images are present in a given set
     *
     * @param kic the set of key images to check against
     * @param tx the transaction to check
     *
     * @return true if any key images present in the set, otherwise false
     */
    static bool have_key_images(const std::unordered_set<crypto::key_image>& kic, const transaction_prefix& tx);

    /**
     * @brief append the key images from a transaction to the given set
     *
     * @param kic the set of key images to append to
     * @param tx the transaction
     *
     * @return false if any append fails, otherwise true
     */
    static bool append_key_images(std::unordered_set<crypto::key_image>& kic, const transaction_prefix& tx);

    /**
     * @brief check if a transaction is a valid candidate for inclusion in a block
     *
     * @param txd the transaction to check (and info about it)
     * @param txid the txid of the transaction to check
     * @param txblob the transaction blob to check
     * @param tx the parsed transaction, if successful
     *
     * @return true if the transaction is good to go, otherwise false
     */
    bool is_transaction_ready_to_go(txpool_tx_meta_t& txd, const crypto::hash &txid, const cryptonote::blobdata &txblob, transaction&tx) const;

    /**
     * @brief mark all transactions double spending the one passed
     */
    void mark_double_spend(const transaction &tx);

    bool remove_tx(const crypto::hash &txid, const txpool_tx_meta_t *meta = nullptr, const sorted_tx_container::iterator *stc_it = nullptr);

    /**
     * @brief prune lowest fee/byte txes till we're not above bytes
     *
     * if bytes is 0, use m_txpool_max_weight
     */
    void prune(const crypto::hash &skip);

    //TODO: confirm the below comments and investigate whether or not this
    //      is the desired behavior
    //! map key images to transactions which spent them
    /*! this seems odd, but it seems that multiple transactions can exist
     *  in the pool which both have the same spent key.  This would happen
     *  in the event of a reorg where someone creates a new/different
     *  transaction on the assumption that the original will not be in a
     *  block again.
     */
    typedef std::unordered_map<crypto::key_image, std::unordered_set<crypto::hash>> key_images_container;

    mutable std::recursive_mutex m_transactions_lock;  //!< mutex for the pool

    //! container for spent key images from the transactions in the pool
    key_images_container m_spent_key_images;

    //TODO: this time should be a named constant somewhere, not hard-coded
    //! interval on which to check for stale/"stuck" transactions
    tools::periodic_task m_remove_stuck_tx_interval{30s};

    //TODO: look into doing this better
    //!< container for transactions organized by fee per size and receive time
    sorted_tx_container m_txs_by_fee_and_receive_time;

    std::atomic<uint64_t> m_cookie; //!< incremented at each change

    /**
     * @brief get an iterator to a transaction in the sorted container
     *
     * @param id the hash of the transaction to look for
     *
     * @return an iterator, possibly to the end of the container if not found
     */
    sorted_tx_container::iterator find_tx_in_sorted_container(const crypto::hash& id) const;

    //! cache/call Blockchain::check_tx_inputs results
    bool check_tx_inputs(const std::function<cryptonote::transaction&(void)> &get_tx, const crypto::hash &txid, uint64_t &max_used_block_height, crypto::hash &max_used_block_id, tx_verification_context &tvc, bool kept_by_block = false) const;

    //! transactions which are unlikely to be included in blocks
    /*! These transactions are kept in RAM in case they *are* included
     *  in a block eventually, but this container is not saved to disk.
     */
    std::unordered_set<crypto::hash> m_timed_out_transactions;

    Blockchain& m_blockchain;  //!< reference to the Blockchain object

    size_t m_txpool_max_weight;
    size_t m_txpool_weight;

    mutable std::unordered_map<crypto::hash, std::tuple<bool, tx_verification_context, uint64_t, crypto::hash>> m_input_cache;

    std::unordered_map<crypto::hash, transaction> m_parsed_tx_cache;
  };
}