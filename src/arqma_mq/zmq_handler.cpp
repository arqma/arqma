// Copyright (c) 2018-2020, The Arqma Network
// Copyright (c)2020, Gary Rusher
// Copyright (c) 2017-2018, The Monero Project
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

#include "zmq_handler.h"

// likely included by daemon_handler.h's includes,
// but including here for clarity
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/blobdatatype.h"
#include "ringct/rctSigs.h"
#include "version.h"

namespace arqmaMQ
{

  void ZmqHandler::handle(const cryptonote::rpc::GetHeight::Request& req, cryptonote::rpc::GetHeight::Response& res)
  {
    res.height = m_core.get_current_blockchain_height();

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlocksFast::Request& req, cryptonote::rpc::GetBlocksFast::Response& res)
  {
    std::vector<std::pair<std::pair<cryptonote::blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, cryptonote::blobdata>>>> blocks;

    if(!m_core.find_blockchain_supplement(req.start_height, req.block_ids, blocks, res.current_height, res.start_height, req.prune, true, COMMAND_RPC_GET_BLOCKS_FAST_MAX_BLOCK_COUNT, COMMAND_RPC_GET_BLOCKS_FAST_MAX_TX_COUNT))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "core::find_blockchain_supplement() returned false";
      return;
    }

    res.blocks.resize(blocks.size());
    res.output_indices.resize(blocks.size());

    auto it = blocks.begin();

    uint64_t block_count = 0;
    while (it != blocks.end())
    {
      cryptonote::rpc::block_with_transactions& bwt = res.blocks[block_count];

      if (!parse_and_validate_block_from_blob(it->first.first, bwt.block))
      {
        res.blocks.clear();
        res.output_indices.clear();
        res.status = cryptonote::rpc::Message::STATUS_FAILED;
        res.error_details = "failed retrieving a requested block";
        return;
      }

      if (it->second.size() != bwt.block.tx_hashes.size())
      {
          res.blocks.clear();
          res.output_indices.clear();
          res.status = cryptonote::rpc::Message::STATUS_FAILED;
          res.error_details = "incorrect number of transactions retrieved for block";
          return;
      }

      cryptonote::rpc::block_output_indices& indices = res.output_indices[block_count];

      // miner tx output indices
      {
        cryptonote::rpc::tx_output_indices tx_indices;
        if (!m_core.get_tx_outputs_gindexs(get_transaction_hash(bwt.block.miner_tx), tx_indices))
        {
          res.status = cryptonote::rpc::Message::STATUS_FAILED;
          res.error_details = "core::get_tx_outputs_gindexs() returned false";
          return;
        }
        indices.push_back(std::move(tx_indices));
      }

      auto hash_it = bwt.block.tx_hashes.begin();
      bwt.transactions.reserve(it->second.size());
      for (const auto& blob : it->second)
      {
        bwt.transactions.emplace_back();
        if (!parse_and_validate_tx_from_blob(blob.second, bwt.transactions.back()))
        {
          res.blocks.clear();
          res.output_indices.clear();
          res.status = cryptonote::rpc::Message::STATUS_FAILED;
          res.error_details = "failed retrieving a requested transaction";
          return;
        }

        cryptonote::rpc::tx_output_indices tx_indices;
        if (!m_core.get_tx_outputs_gindexs(*hash_it, tx_indices))
        {
          res.status = cryptonote::rpc::Message::STATUS_FAILED;
          res.error_details = "core::get_tx_outputs_gindexs() returned false";
          return;
        }

        indices.push_back(std::move(tx_indices));
        ++hash_it;
      }

      it++;
      block_count++;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetHashesFast::Request& req, cryptonote::rpc::GetHashesFast::Response& res)
  {
    res.start_height = req.start_height;

    auto& chain = m_core.get_blockchain_storage();

    if (!chain.find_blockchain_supplement(req.known_hashes, res.hashes, NULL, res.start_height, res.current_height, false))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Blockchain::find_blockchain_supplement() returned false";
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetTransactions::Request& req, cryptonote::rpc::GetTransactions::Response& res)
  {
    std::vector<cryptonote::transaction> found_txs_vec;
    std::vector<crypto::hash> missed_vec;

    bool r = m_core.get_transactions(req.tx_hashes, found_txs_vec, missed_vec);

    // TODO: consider fixing core::get_transactions to not hide exceptions
    if (!r)
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "core::get_transactions() returned false (exception caught there)";
      return;
    }

    size_t num_found = found_txs_vec.size();

    std::vector<uint64_t> heights(num_found);
    std::vector<bool> in_pool(num_found, false);
    std::vector<crypto::hash> found_hashes(num_found);

    for (size_t i=0; i < num_found; i++)
    {
      found_hashes[i] = get_transaction_hash(found_txs_vec[i]);
      heights[i] = m_core.get_blockchain_storage().get_db().get_tx_block_height(found_hashes[i]);
    }

    // if any missing from blockchain, check in tx pool
    if (!missed_vec.empty())
    {
      std::vector<cryptonote::transaction> pool_txs;

      m_core.get_pool_transactions(pool_txs);

      for (const auto& tx : pool_txs)
      {
        crypto::hash h = get_transaction_hash(tx);

        auto itr = std::find(missed_vec.begin(), missed_vec.end(), h);

        if (itr != missed_vec.end())
        {
          found_hashes.push_back(h);
          found_txs_vec.push_back(tx);
          heights.push_back(std::numeric_limits<uint64_t>::max());
          in_pool.push_back(true);
          missed_vec.erase(itr);
        }
      }
    }

    for (size_t i=0; i < found_hashes.size(); i++)
    {
      cryptonote::rpc::transaction_info info;
      info.height = heights[i];
      info.in_pool = in_pool[i];
      info.transaction = std::move(found_txs_vec[i]);

      res.txs.emplace(found_hashes[i], std::move(info));
    }

    res.missed_hashes = std::move(missed_vec);
    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::KeyImagesSpent::Request& req, cryptonote::rpc::KeyImagesSpent::Response& res)
  {
    res.spent_status.resize(req.key_images.size(), cryptonote::rpc::KeyImagesSpent::STATUS::UNSPENT);

    std::vector<bool> chain_spent_status;
    std::vector<bool> pool_spent_status;

    m_core.are_key_images_spent(req.key_images, chain_spent_status);
    m_core.are_key_images_spent_in_pool(req.key_images, pool_spent_status);

    if ((chain_spent_status.size() != req.key_images.size()) || (pool_spent_status.size() != req.key_images.size()))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "tx_pool::have_key_images_as_spent() gave vectors of wrong size(s).";
      return;
    }

    for(size_t i=0; i < req.key_images.size(); i++)
    {
      if ( chain_spent_status[i] )
      {
        res.spent_status[i] = cryptonote::rpc::KeyImagesSpent::STATUS::SPENT_IN_BLOCKCHAIN;
      }
      else if ( pool_spent_status[i] )
      {
        res.spent_status[i] = cryptonote::rpc::KeyImagesSpent::STATUS::SPENT_IN_POOL;
      }
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetTxGlobalOutputIndices::Request& req, cryptonote::rpc::GetTxGlobalOutputIndices::Response& res)
  {
    if (!m_core.get_tx_outputs_gindexs(req.tx_hash, res.output_indices))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "core::get_tx_outputs_gindexs() returned false";
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;

  }

  void ZmqHandler::handle(const cryptonote::rpc::SendRawTx::Request& req, cryptonote::rpc::SendRawTx::Response& res)
  {
    handleTxBlob(cryptonote::tx_to_blob(req.tx), req.relay, res);
  }

	void ZmqHandler::handle(const cryptonote::rpc::SendRawTxHex::Request& req, cryptonote::rpc::SendRawTxHex::Response& res)
	{
	  std::string tx_blob;
	  if(!epee::string_tools::parse_hexstr_to_binbuff(req.tx_as_hex, tx_blob))
	  {
	    MERROR("[SendRawTxHex]: Failed to parse tx from hexbuff: " << req.tx_as_hex);
	    res.status = cryptonote::rpc::Message::STATUS_FAILED;
	    res.error_details = "Invalid hex";
	    return;
	  }
	  handleTxBlob(tx_blob, req.relay, res);
	}

	void ZmqHandler::handleTxBlob(const std::string& tx_blob, bool relay, cryptonote::rpc::SendRawTx::Response& res)
	{
	  if (!m_p2p.get_payload_object().is_synchronized())
	  {
	    res.status = cryptonote::rpc::Message::STATUS_FAILED;
	    res.error_details = "Not ready to accept transactions; try again later";
	    return;
	  }

    cryptonote::cryptonote_connection_context fake_context = AUTO_VAL_INIT(fake_context);
    cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);

    if(!m_core.handle_incoming_tx(tx_blob, tvc, false, false, !relay) || tvc.m_verifivation_failed)
    {
      if (tvc.m_verifivation_failed)
      {
        MERROR("[SendRawTx]: tx verification failed");
      }
      else
      {
        MERROR("[SendRawTx]: Failed to process tx");
      }
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "";

      if (tvc.m_low_mixin)
      {
        res.error_details = "mixin too low";
      }
      if (tvc.m_double_spend)
      {
        if (!res.error_details.empty()) res.error_details += " and ";
        res.error_details = "double spend";
      }
      if (tvc.m_invalid_input)
      {
        if (!res.error_details.empty()) res.error_details += " and ";
        res.error_details = "invalid input";
      }
      if (tvc.m_invalid_output)
      {
        if (!res.error_details.empty()) res.error_details += " and ";
        res.error_details = "invalid output";
      }
      if (tvc.m_too_big)
      {
        if (!res.error_details.empty()) res.error_details += " and ";
        res.error_details = "too big";
      }
      if (tvc.m_overspend)
      {
        if (!res.error_details.empty()) res.error_details += " and ";
        res.error_details = "overspend";
      }
      if (tvc.m_fee_too_low)
      {
        if (!res.error_details.empty()) res.error_details += " and ";
        res.error_details = "fee too low";
      }
      if (res.error_details.empty())
      {
        res.error_details = "an unknown issue was found with the transaction";
      }

      return;
    }

    if(!tvc.m_should_be_relayed || !relay)
    {
      MERROR("[SendRawTx]: tx accepted, but not relayed");
      res.error_details = "Not relayed";
      res.relayed = false;
      res.status = cryptonote::rpc::Message::STATUS_OK;

      return;
    }

    cryptonote::NOTIFY_NEW_TRANSACTIONS::request r;
    r.txs.push_back(tx_blob);
    m_core.get_protocol()->relay_transactions(r, fake_context);

    //TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes
    res.status = cryptonote::rpc::Message::STATUS_OK;
    res.relayed = true;

    return;
  }

  void ZmqHandler::handle(const cryptonote::rpc::StartMining::Request& req, cryptonote::rpc::StartMining::Response& res)
  {
    cryptonote::address_parse_info info;
    if(!get_account_address_from_str(info, m_core.get_nettype(), req.miner_address))
    {
      res.error_details = "Failed, wrong address";
      LOG_PRINT_L0(res.error_details);
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      return;
    }
    if (info.is_subaddress)
    {
      res.error_details = "Failed, mining to subaddress isn't supported yet";
      LOG_PRINT_L0(res.error_details);
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      return;
    }

    unsigned int concurrency_count = boost::thread::hardware_concurrency() * 4;

    // if we couldn't detect threads, set it to a ridiculously high number
    if(concurrency_count == 0)
    {
      concurrency_count = 257;
    }

    // if there are more threads requested than the hardware supports
    // then we fail and log that.
    if(req.threads_count > concurrency_count)
    {
      res.error_details = "Failed, too many threads relative to CPU cores.";
      LOG_PRINT_L0(res.error_details);
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      return;
    }

    boost::thread::attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);

    if(!m_core.get_miner().start(info.address, static_cast<size_t>(req.threads_count), attrs, req.do_background_mining, req.ignore_battery))
    {
      res.error_details = "Failed, mining not started";
      LOG_PRINT_L0(res.error_details);
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      return;
    }
    res.status = cryptonote::rpc::Message::STATUS_OK;
    res.error_details = "";

  }

  void ZmqHandler::handle(const cryptonote::rpc::GetInfo::Request& req, cryptonote::rpc::GetInfo::Response& res)
  {
    res.info.height = m_core.get_current_blockchain_height();

    res.info.target_height = m_core.get_target_blockchain_height();

    if (res.info.height > res.info.target_height)
    {
      res.info.target_height = res.info.height;
    }

    auto& chain = m_core.get_blockchain_storage();

    res.info.difficulty = chain.get_difficulty_for_next_block();

    res.info.target = chain.get_difficulty_target();

    res.info.tx_count = chain.get_total_transactions() - res.info.height; //without coinbase

    res.info.tx_pool_size = m_core.get_pool_transactions_count();

    res.info.alt_blocks_count = chain.get_alternative_blocks_count();

    uint64_t total_conn = m_p2p.get_public_connections_count();
    res.info.outgoing_connections_count = m_p2p.get_public_outgoing_connections_count();
    res.info.incoming_connections_count = total_conn - res.info.outgoing_connections_count;

    res.info.white_peerlist_size = m_p2p.get_public_white_peers_count();

    res.info.grey_peerlist_size = m_p2p.get_public_gray_peers_count();

    res.info.mainnet = m_core.get_nettype() == cryptonote::MAINNET;
    res.info.testnet = m_core.get_nettype() == cryptonote::TESTNET;
    res.info.stagenet = m_core.get_nettype() == cryptonote::STAGENET;
    res.info.cumulative_difficulty = m_core.get_blockchain_storage().get_db().get_block_cumulative_difficulty(res.info.height - 1);
    res.info.block_size_limit = res.info.block_weight_limit = m_core.get_blockchain_storage().get_current_cumulative_block_weight_limit();
    res.info.block_size_median = res.info.block_weight_median = m_core.get_blockchain_storage().get_current_cumulative_block_weight_median();
    res.info.start_time = (uint64_t)m_core.get_start_time();
    res.info.version = ARQMA_VERSION;
    res.info.syncing = m_p2p.get_payload_object().currently_busy_syncing();

    res.status = cryptonote::rpc::Message::STATUS_OK;
    res.error_details = "";
  }

  void ZmqHandler::handle(const cryptonote::rpc::StopMining::Request& req, cryptonote::rpc::StopMining::Response& res)
  {
    if(!m_core.get_miner().stop())
    {
      res.error_details = "Failed, mining not stopped";
      LOG_PRINT_L0(res.error_details);
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
    res.error_details = "";
  }

  void ZmqHandler::handle(const cryptonote::rpc::MiningStatus::Request& req, cryptonote::rpc::MiningStatus::Response& res)
  {
    const cryptonote::miner& lMiner = m_core.get_miner();
    res.active = lMiner.is_mining();
    res.is_background_mining_enabled = lMiner.get_is_background_mining_enabled();

    if ( lMiner.is_mining() ) {
      res.speed = lMiner.get_speed();
      res.threads_count = lMiner.get_threads_count();
      const cryptonote::account_public_address& lMiningAdr = lMiner.get_mining_address();
      res.address = get_account_address_as_str(m_core.get_nettype(), false, lMiningAdr);
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
    res.error_details = "";
  }

  void ZmqHandler::handle(const cryptonote::rpc::SaveBC::Request& req, cryptonote::rpc::SaveBC::Response& res)
  {
    if (!m_core.get_blockchain_storage().store_blockchain())
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Error storing the blockchain";
    }
    else
    {
      res.status = cryptonote::rpc::Message::STATUS_OK;
    }
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlockHash::Request& req, cryptonote::rpc::GetBlockHash::Response& res)
  {
    if (m_core.get_current_blockchain_height() <= req.height)
    {
      res.hash = crypto::null_hash;
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "height given is higher than current chain height";
      return;
    }

    res.hash = m_core.get_block_id_by_height(req.height);

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  uint64_t slow_memmem(const void* start_buff, size_t buflen,const void* pat,size_t patlen)
  {
    const void* buf = start_buff;
    const void* end=(const char*)buf+buflen;
    if (patlen > buflen || patlen == 0) return 0;
    while(buflen>0 && (buf=memchr(buf,((const char*)pat)[0],buflen-patlen+1)))
    {
      if(memcmp(buf,pat,patlen)==0)
        return (const char*)buf - (const char*)start_buff;
      buf=(const char*)buf+1;
      buflen = (const char*)end - (const char*)buf;
    }
    return 0;
  }

  bool ZmqHandler::check_core_ready()
  {
	if (m_core.get_current_blockchain_height() >= m_core.get_target_blockchain_height())
	{
	  return true;
	}
	return false;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlockTemplate::Request& req, cryptonote::rpc::GetBlockTemplate::Response& res)
  {
    if(!check_core_ready())
    {
      res.status  = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Core is busy";
      return;
    }

    if(req.reserve_size > 255)
    {
      res.status  = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details  = "Too big reserved size, maximum 255";
      return;
    }

    cryptonote::address_parse_info info;

    if(!req.wallet_address.size() || !cryptonote::get_account_address_from_str(info, nettype(), req.wallet_address))
    {
      res.status  = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Failed to parse wallet address";
      return;
    }
    if (info.is_subaddress)
    {
      res.status  = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Mining to subaddress is not supported yet";
      return;
    }

    cryptonote::block b;
    cryptonote::blobdata blob_reserve;
    blob_reserve.resize(req.reserve_size, 0);
    size_t reserved_offset;
    crypto::hash seed_hash, next_seed_hash;
    if(!get_block_template(info.address, NULL, blob_reserve, reserved_offset, res.difficulty, res.height, res.expected_reward, b, res.seed_height, seed_hash, next_seed_hash, res))
      return;
    res.reserved_offset = reserved_offset;
    cryptonote::blobdata block_blob = cryptonote::t_serializable_object_to_blob(b);
    cryptonote::blobdata hashing_blob = get_block_hashing_blob(b);
    res.prev_hash = string_tools::pod_to_hex(b.prev_id);
    res.blocktemplate_blob = string_tools::buff_to_hex_nodelimer(block_blob);
    res.blockhashing_blob =  string_tools::buff_to_hex_nodelimer(hashing_blob);
    res.seed_hash = string_tools::pod_to_hex(seed_hash);
    if(next_seed_hash != crypto::null_hash)
      res.next_seed_hash = string_tools::pod_to_hex(next_seed_hash);
    res.status = cryptonote::rpc::Message::STATUS_OK;
    return;
  }

  bool ZmqHandler::get_block_template(const cryptonote::account_public_address &address, const crypto::hash *prev_block, const cryptonote::blobdata &extra_nonce, size_t &reserved_offset, cryptonote::difficulty_type &difficulty, uint64_t &height, uint64_t &expected_reward, cryptonote::block &b, uint64_t &seed_height, crypto::hash &seed_hash, crypto::hash &next_seed_hash, cryptonote::rpc::GetBlockTemplate::Response& res)
  {
    b = boost::value_initialized<cryptonote::block>();
    if(!m_core.get_block_template(b, prev_block, address, difficulty, height, expected_reward, extra_nonce, seed_height, seed_hash))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Internal error: failed to create block template";
      LOG_ERROR("Failed to create block template");
      return false;
    }
    cryptonote::blobdata block_blob = t_serializable_object_to_blob(b);
    crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(b.miner_tx);
    if(tx_pub_key == crypto::null_pkey)
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Internal error: failed to create block template";
      LOG_ERROR("Failed to get tx pub key in coinbase extra");
      return false;
    }

    seed_hash = next_seed_hash = crypto::null_hash;
    if(b.major_version >= RX_BLOCK_VERSION)
    {
      uint64_t seed_height, next_height;
      crypto::rx_seedheights(height, &seed_height, &next_height);
      seed_hash = m_core.get_block_id_by_height(seed_height);
      if(next_height != seed_height)
      {
        next_seed_hash = m_core.get_block_id_by_height(next_height);
      }
    }

    if (extra_nonce.empty())
    {
      reserved_offset = 0;
      return true;
    }

    reserved_offset = slow_memmem((void*)block_blob.data(), block_blob.size(), &tx_pub_key, sizeof(tx_pub_key));
    if(!reserved_offset)
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Internal error: failed to create block template";
      LOG_ERROR("Failed to find tx pub key in blockblob");
      return false;
    }
    reserved_offset += sizeof(tx_pub_key) + 2; //2 bytes: tag for TX_EXTRA_NONCE(1 byte), counter in TX_EXTRA_NONCE(1 byte)
    if(reserved_offset + extra_nonce.size() > block_blob.size())
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Internal error: failed to create block template";
      LOG_ERROR("Failed to calculate offset for ");
      return false;
    }
    return true;
  }

  void ZmqHandler::handle(const cryptonote::rpc::SubmitBlock::Request& req, cryptonote::rpc::SubmitBlock::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetLastBlockHeader::Request& req, cryptonote::rpc::GetLastBlockHeader::Response& res)
  {
    const crypto::hash block_hash = m_core.get_tail_id();

    if (!getBlockHeaderByHash(block_hash, res.header))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Requested block does not exist";
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlockHeaderByHash::Request& req, cryptonote::rpc::GetBlockHeaderByHash::Response& res)
  {
    if (!getBlockHeaderByHash(req.hash, res.header))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Requested block does not exist";
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlockHeaderByHeight::Request& req, cryptonote::rpc::GetBlockHeaderByHeight::Response& res)
  {
    const crypto::hash block_hash = m_core.get_block_id_by_height(req.height);

    if (!getBlockHeaderByHash(block_hash, res.header))
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Requested block does not exist";
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlockHeadersByHeight::Request& req, cryptonote::rpc::GetBlockHeadersByHeight::Response& res)
  {
    res.headers.resize(req.heights.size());

    for (size_t i=0; i < req.heights.size(); i++)
    {
      const crypto::hash block_hash = m_core.get_block_id_by_height(req.heights[i]);

      if (!getBlockHeaderByHash(block_hash, res.headers[i]))
      {
        res.status = cryptonote::rpc::Message::STATUS_FAILED;
        res.error_details = "A requested block does not exist";
        return;
      }
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlock::Request& req, cryptonote::rpc::GetBlock::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetPeerList::Request& req, cryptonote::rpc::GetPeerList::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::SetLogHashRate::Request& req, cryptonote::rpc::SetLogHashRate::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::SetLogLevel::Request& req, cryptonote::rpc::SetLogLevel::Response& res)
  {
    if (req.level < 0 || req.level > 4)
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = "Error: log level not valid";
    }
    else
    {
      res.status = cryptonote::rpc::Message::STATUS_OK;
      mlog_set_log_level(req.level);
    }
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetTransactionPool::Request& req, cryptonote::rpc::GetTransactionPool::Response& res)
  {
    bool r = m_core.get_pool_for_rpc(res.transactions, res.key_images);

    if (!r) res.status = cryptonote::rpc::Message::STATUS_FAILED;
    else res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetConnections::Request& req, cryptonote::rpc::GetConnections::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBlockHeadersRange::Request& req, cryptonote::rpc::GetBlockHeadersRange::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::StopDaemon::Request& req, cryptonote::rpc::StopDaemon::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::StartSaveGraph::Request& req, cryptonote::rpc::StartSaveGraph::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::StopSaveGraph::Request& req, cryptonote::rpc::StopSaveGraph::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::HardForkInfo::Request& req, cryptonote::rpc::HardForkInfo::Response& res)
  {
    const cryptonote::Blockchain &blockchain = m_core.get_blockchain_storage();
    uint8_t version = req.version > 0 ? req.version : blockchain.get_ideal_hard_fork_version();
    res.info.version = blockchain.get_current_hard_fork_version();
    res.info.enabled = blockchain.get_hard_fork_voting_info(version, res.info.window, res.info.votes, res.info.threshold, res.info.earliest_height, res.info.voting);
    res.info.state = blockchain.get_hard_fork_state();
    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetBans::Request& req, cryptonote::rpc::GetBans::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::SetBans::Request& req, cryptonote::rpc::SetBans::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::FlushTransactionPool::Request& req, cryptonote::rpc::FlushTransactionPool::Response& res)
  {
    res.status = cryptonote::rpc::Message::STATUS_FAILED;
    res.error_details = "RPC method not yet implemented.";
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetOutputHistogram::Request& req, cryptonote::rpc::GetOutputHistogram::Response& res)
  {
    std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t> > histogram;
    try
    {
      histogram = m_core.get_blockchain_storage().get_output_histogram(req.amounts, req.unlocked, req.recent_cutoff);
    }
    catch (const std::exception &e)
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = e.what();
      return;
    }

    res.histogram.clear();
    res.histogram.reserve(histogram.size());
    for (const auto &i: histogram)
    {
      if (std::get<0>(i.second) >= req.min_count && (std::get<0>(i.second) <= req.max_count || req.max_count == 0))
        res.histogram.emplace_back(cryptonote::rpc::output_amount_count{i.first, std::get<0>(i.second), std::get<1>(i.second), std::get<2>(i.second)});
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetOutputKeys::Request& req, cryptonote::rpc::GetOutputKeys::Response& res)
  {
    try
    {
      for (const auto& i : req.outputs)
      {
        crypto::public_key key;
        rct::key mask;
        bool unlocked;
        m_core.get_blockchain_storage().get_output_key_mask_unlocked(i.amount, i.index, key, mask, unlocked);
        res.keys.emplace_back(cryptonote::rpc::output_key_mask_unlocked{key, mask, unlocked});
      }
    }
    catch (const std::exception& e)
    {
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = e.what();
      return;
    }

    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetRPCVersion::Request& req, cryptonote::rpc::GetRPCVersion::Response& res)
  {
    res.version = cryptonote::rpc::DAEMON_RPC_VERSION_ZMQ;
    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetFeeEstimate::Request& req, cryptonote::rpc::GetFeeEstimate::Response& res)
  {
    res.hard_fork_version = m_core.get_blockchain_storage().get_current_hard_fork_version();
    res.estimated_base_fee = m_core.get_blockchain_storage().get_dynamic_base_fee_estimate(req.num_grace_blocks);

    if (res.hard_fork_version < HF_VERSION_PER_BYTE_FEE)
    {
       res.size_scale = 1024; // per KiB fee
       res.fee_mask = 1;
    }
    else
    {
      res.size_scale = 1; // per byte fee
      res.fee_mask = cryptonote::Blockchain::get_fee_quantization_mask();
    }
    res.status = cryptonote::rpc::Message::STATUS_OK;
  }

  void ZmqHandler::handle(const cryptonote::rpc::GetOutputDistribution::Request& req, cryptonote::rpc::GetOutputDistribution::Response& res)
  {
    try
    {
      res.distributions.reserve(req.amounts.size());

      const uint64_t req_to_height = req.to_height ? req.to_height : (m_core.get_current_blockchain_height() - 1);
      for (std::uint64_t amount : req.amounts)
      {
        auto data = cryptonote::rpc::RpcHandler::get_output_distribution([this](uint64_t amount, uint64_t from, uint64_t to, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) { return m_core.get_output_distribution(amount, from, to, start_height, distribution, base); }, amount, req.from_height, req_to_height, req.cumulative);
        if (!data)
        {
          res.distributions.clear();
          res.status = cryptonote::rpc::Message::STATUS_FAILED;
          res.error_details = "Failed to get output distribution";
          return;
        }
        res.distributions.push_back(cryptonote::rpc::output_distribution{std::move(*data), amount, req.cumulative});
      }
      res.status = cryptonote::rpc::Message::STATUS_OK;
    }
    catch (const std::exception& e)
    {
      res.distributions.clear();
      res.status = cryptonote::rpc::Message::STATUS_FAILED;
      res.error_details = e.what();
    }
  }

  bool ZmqHandler::getBlockHeaderByHash(const crypto::hash& hash_in, cryptonote::rpc::BlockHeaderResponse& header)
  {
    cryptonote::block b;

    if (!m_core.get_block_by_hash(hash_in, b))
    {
      return false;
    }

    header.hash = hash_in;
    if (b.miner_tx.vin.size() != 1 || b.miner_tx.vin.front().type() != typeid(cryptonote::txin_gen))
    {
      return false;
    }
    header.height = boost::get<cryptonote::txin_gen>(b.miner_tx.vin.front()).height;

    header.major_version = b.major_version;
    header.minor_version = b.minor_version;
    header.timestamp = b.timestamp;
    header.nonce = b.nonce;
    header.prev_id = b.prev_id;

    header.depth = m_core.get_current_blockchain_height() - header.height - 1;

    header.reward = 0;
    for (const auto& out : b.miner_tx.vout)
    {
      header.reward += out.amount;
    }

    header.difficulty = m_core.get_blockchain_storage().block_difficulty(header.height);

    return true;
  }

  std::string ZmqHandler::handle(const std::string& request)
  {
    MDEBUG("Handling RPC request: " << request);

    cryptonote::rpc::Message* resp_message = NULL;

    try
    {
      cryptonote::rpc::FullMessage req_full(request, true);

      rapidjson::Value& req_json = req_full.getMessage();

      const std::string request_type = req_full.getRequestType();

      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetHeight, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetBlocksFast, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetHashesFast, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetTransactions, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::KeyImagesSpent, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetTxGlobalOutputIndices, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::SendRawTx, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::SendRawTxHex, req_json, resp_message, handle);
      REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetInfo, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::StartMining, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::StopMining, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::MiningStatus, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::SaveBC, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetBlockHash, req_json, resp_message, handle);
      REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetBlockTemplate, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetLastBlockHeader, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetBlockHeaderByHash, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetBlockHeaderByHeight, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetBlockHeadersByHeight, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetPeerList, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::SetLogLevel, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetTransactionPool, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::HardForkInfo, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetOutputHistogram, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetOutputKeys, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetRPCVersion, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetFeeEstimate, req_json, resp_message, handle);
      //REQ_RESP_TYPES_MACRO(request_type, cryptonote::rpc::GetOutputDistribution, req_json, resp_message, handle);

      if (resp_message == NULL)
      {
        return cryptonote::rpc::BAD_REQUEST(request_type, req_full.getID());
      }

      cryptonote::rpc::FullMessage resp_full = cryptonote::rpc::FullMessage::responseMessage(resp_message, req_full.getID());

      const std::string response = resp_full.getJson();
      delete resp_message;
      resp_message = NULL;

      MDEBUG("Returning RPC response: " << response);

      return response;
    }
    catch (const std::exception& e)
    {
      if (resp_message)
      {
        delete resp_message;
      }

      return cryptonote::rpc::BAD_JSON(e.what());
    }
  }

}  // namespace arqmaMQ
