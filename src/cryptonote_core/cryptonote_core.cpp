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

#include <boost/algorithm/string.hpp>
#include <boost/endian/conversion.hpp>

#include "string_tools.h"
using namespace epee;

#include <unordered_set>
#include <iomanip>

extern "C" {
#include <sodium.h>
#ifdef ENABLE_SYSTEMD
  #include <systemd/sd-daemon.h>
#endif
}

#include "cryptonote_core.h"
#include "common/util.h"
#include "common/updates.h"
#include "common/download.h"
#include "common/threadpool.h"
#include "common/command_line.h"
#include "daemon/command_line_args.h"
#include "warnings.h"
#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "misc_language.h"
#include "file_io_utils.h"
#include <csignal>
#include "checkpoints/checkpoints.h"
#include "ringct/rctTypes.h"
#include "blockchain_db/blockchain_db.h"
#include "ringct/rctSigs.h"
#include "common/notify.h"
#include "version.h"
#include "memwipe.h"
#include "common/i18n.h"
#include "net/local_ip.h"
#include <boost/filesystem.hpp>
#include "cryptonote_protocol/arqnet.h"

#include "config/ascii.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "cn"

DISABLE_VS_WARNINGS(4355)

#define MERROR_VER(x) MCERROR("verify", x)

#define BAD_SEMANTICS_TXES_MAX_SIZE 100

// basically at least how many bytes the block itself serializes to without the miner tx
#define BLOCK_SIZE_SANITY_LEEWAY 100

namespace cryptonote
{
  const command_line::arg_descriptor<bool, false> arg_testnet_on = {
    "testnet"
  , "Run on testnet. The wallet must be launched with --testnet flag."
  , false
  };
  const command_line::arg_descriptor<bool, false> arg_stagenet_on = {
    "stagenet"
  , "Run on stagenet. The wallet must be launched with --stagenet flag."
  , false
  };
  const command_line::arg_descriptor<bool> arg_regtest_on = {
    "regtest"
  , "Run in a regression testing mode."
  , false
  };
  const command_line::arg_descriptor<difficulty_type> arg_fixed_difficulty = {
    "fixed-difficulty"
  , "Fixed difficulty used for testing."
  , 0
  };
  const command_line::arg_descriptor<std::string, false, true, 2> arg_data_dir = {
    "data-dir"
  , "Specify data directory"
  , tools::get_default_data_dir()
  , {{ &arg_testnet_on, &arg_stagenet_on }}
  , [](std::array<bool, 2> testnet_stagenet, bool defaulted, std::string val)->std::string {
      if (testnet_stagenet[0])
        return (boost::filesystem::path(val) / "testnet").string();
      else if (testnet_stagenet[1])
        return (boost::filesystem::path(val) / "stagenet").string();
      return val;
    }
  };
  const command_line::arg_descriptor<bool> arg_offline = {
    "offline"
  , "Do not listen for peers, nor connect to any"
  };
  const command_line::arg_descriptor<size_t> arg_block_download_max_size = {
    "block-download-max-size"
  , "Set maximum size of block download queue in bytes (0 for default)"
  , 0
  };
  static const command_line::arg_descriptor<bool> arg_test_drop_download = {
    "test-drop-download"
  , "For net tests: in download, discard ALL blocks instead checking/saving them (very fast)"
  };
  static const command_line::arg_descriptor<uint64_t> arg_test_drop_download_height = {
    "test-drop-download-height"
  , "Like test-drop-download but discards only after around certain height"
  , 0
  };
  static const command_line::arg_descriptor<int> arg_test_dbg_lock_sleep = {
    "test-dbg-lock-sleep"
  , "Sleep time in ms, defaults to 0 (off), used to debug before/after locking mutex. Values 100 to 1000 are good for tests."
  , 0
  };
  static const command_line::arg_descriptor<uint64_t> arg_fast_block_sync = {
    "fast-block-sync"
  , "Sync up most of the way by using embedded, known block hashes."
  , 1
  };
  static const command_line::arg_descriptor<uint64_t> arg_prep_blocks_threads = {
    "prep-blocks-threads"
  , "Max number of threads to use when preparing block hashes in groups."
  , 8
  };
  static const command_line::arg_descriptor<uint64_t> arg_show_time_stats = {
    "show-time-stats"
  , "Show time-stats when processing blocks/txs and disk synchronization."
  , 0
  };
  static const command_line::arg_descriptor<size_t> arg_block_sync_size = {
    "block-sync-size"
  , "How many blocks to sync at once during chain synchronization (0 = adaptive)."
  , 0
  };
  static const command_line::arg_descriptor<std::string> arg_check_updates = {
    "check-updates"
  , "Check for new versions of arqma: [disabled|notify|download|update]"
  , "notify"
  };
  static const command_line::arg_descriptor<bool> arg_pad_transactions = {
    "pad-transactions"
  , "Pad relayed transactions to help defend against traffic volume analysis"
  , false
  };
  static const command_line::arg_descriptor<size_t> arg_max_txpool_weight = {
    "max-txpool-weight"
  , "Set maximum txpool weight in bytes."
  , DEFAULT_TXPOOL_MAX_WEIGHT
  };
  static const command_line::arg_descriptor<std::string> arg_block_notify = {
    "block-notify"
  , "Run a program for each new block, '%s' will be replaced by the block hash"
  , ""
  };
  static const command_line::arg_descriptor<bool> arg_prune_blockchain = {
    "prune-blockchain"
  , "Prune Blockchain"
  , false
  };
  static const command_line::arg_descriptor<std::string> arg_reorg_notify = {
    "reorg-notify"
  , "Run a program for each reorg, '%s' will be replaced by the split height, "
    "'%h' will be replaced by the new blockchain height, and '%n' will be "
    "replaced by the number of new blocks in the new chain"
  , ""
  };
  static const command_line::arg_descriptor<bool> arg_keep_alt_blocks = {
    "keep-alt-blocks"
  , "Keep Alternative Blocks on Restart"
  , false
  };
  static const command_line::arg_descriptor<bool> arg_service_node = {
    "service-node"
  , "Run as a Arqma Network Service Node" //, options 'sn-ip' and 'ss-port' must be set"
  };
  static const command_line::arg_descriptor<std::string> arg_public_ip = {
    "sn-ip"
  , "Public IP address on which this service node's services (such as the Arqma "
    "storage server) are accessible. This IP address will be advertised to the "
    "network via the service node uptime proofs. Required if operating as a "
    "service node."
  };
  static const command_line::arg_descriptor<uint16_t> arg_sn_bind_port = {
    "ss-port"
  , "The port on which this service node's storage server is accessible. A listening "
    "storage server is required for service nodes."
  , 0};
  static const command_line::arg_descriptor<uint16_t, false, true, 2> arg_arqnet_port = {
    "arqnet-port"
  , "The port on which this service node listen to direct connections from other "
    "service nodes for quorum messages. Port must be publicly reachable on the "
    "'--sn-ip' address and binds to the p2p IP address. Only applies when "
    "running as a service node."
  , config::ANET_DEFAULT_PORT
  , {{ &cryptonote::arg_testnet_on, &cryptonote::arg_stagenet_on }}
  , [](std::array<bool, 2> testnet_stagenet, bool defaulted, uint16_t val) -> uint16_t {
    return defaulted && testnet_stagenet[0] ? config::testnet::ANET_DEFAULT_PORT :
           defaulted && testnet_stagenet[1] ? config::stagenet::ANET_DEFAULT_PORT :
           val;
    }
  };
  static const command_line::arg_descriptor<uint64_t> arg_store_quorum_history = {
    "store-quorum-history"
  , "Store the Service Node Quorum history for the last N blocks. "
    "Specify the number of blocks or 1 to store the entire history."
  , 0};
  //-----------------------------------------------------------------------------------------------
  [[noreturn]] static void need_core_init()
  {
    throw std::logic_error("Internal error: arqnet::init_core_callbacks() should have been called");
  }
  void *(*arqnet_new)(core &, const std::string &bind);
  void (*arqnet_delete)(void *&self);
  void (*arqnet_relay_obligation_votes)(void *self, const std::vector<service_nodes::quorum_vote_t> &);
  static bool init_core_callback_stubs()
  {
    arqnet_new = [](core &, const std::string &) -> void * { need_core_init(); };
    arqnet_delete = [](void *&) { need_core_init(); };
    arqnet_relay_obligation_votes = [](void *, const std::vector<service_nodes::quorum_vote_t> &) { need_core_init(); };
    return false;
  }
  bool init_core_callback_complete = init_core_callback_stubs();
  //-----------------------------------------------------------------------------------------------
  core::core(i_cryptonote_protocol* pprotocol):
              m_mempool(m_blockchain_storage),
              m_service_node_list(m_blockchain_storage),
              m_blockchain_storage(m_mempool, m_service_node_list),
              m_quorum_cop(*this),
              m_miner(this, [this](const cryptonote::block &b, uint64_t height, const crypto::hash *seed_hash, unsigned int threads, crypto::hash &hash) {
                return cryptonote::get_block_longhash(&m_blockchain_storage, b, hash, height, seed_hash, threads);
              }),
              m_miner_address{},
              m_starter_message_showed(false),
              m_target_blockchain_height(0),
              m_checkpoints_path(""),
              m_last_json_checkpoints_update(0),
              m_update_download(0),
              m_nettype(UNDEFINED),
              m_update_available(false),
              m_last_storage_server_ping(0),
              /*m_last_arqnet_ping(0),*/
              m_pad_transactions(false)
  {
    m_checkpoints_updating.clear();
    set_cryptonote_protocol(pprotocol);
  }
  void core::set_cryptonote_protocol(i_cryptonote_protocol* pprotocol)
  {
    if(pprotocol)
      m_pprotocol = pprotocol;
    else
      m_pprotocol = &m_protocol_stub;
  }
  //-----------------------------------------------------------------------------------
  bool core::update_checkpoints_from_json_file()
  {
    if (m_checkpoints_updating.test_and_set()) return true;

    bool res = true;
    if (time(NULL) - m_last_json_checkpoints_update >= 600)
    {
      res = m_blockchain_storage.update_checkpoints_from_json_file(m_checkpoints_path);
      m_last_json_checkpoints_update = time(NULL);
    }

    m_checkpoints_updating.clear();

    // if anything fishy happened getting new checkpoints, bring down the house
    if (!res)
    {
      graceful_exit();
    }
    return res;
  }
  //-----------------------------------------------------------------------------------
  void core::stop()
  {
    m_miner.stop();
    m_blockchain_storage.cancel();

    tools::download_async_handle handle;
    {
      std::lock_guard lock{m_update_mutex};
      handle = m_update_download;
      m_update_download = 0;
    }
    if (handle)
      tools::download_cancel(handle);
  }
  //-----------------------------------------------------------------------------------
  void core::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_data_dir);

    command_line::add_arg(desc, arg_test_drop_download);
    command_line::add_arg(desc, arg_test_drop_download_height);

    command_line::add_arg(desc, arg_testnet_on);
    command_line::add_arg(desc, arg_stagenet_on);
    command_line::add_arg(desc, arg_regtest_on);
    command_line::add_arg(desc, arg_fixed_difficulty);
    command_line::add_arg(desc, arg_prep_blocks_threads);
    command_line::add_arg(desc, arg_fast_block_sync);
    command_line::add_arg(desc, arg_show_time_stats);
    command_line::add_arg(desc, arg_block_sync_size);
    command_line::add_arg(desc, arg_check_updates);
    command_line::add_arg(desc, arg_test_dbg_lock_sleep);
    command_line::add_arg(desc, arg_offline);
    command_line::add_arg(desc, arg_block_download_max_size);
    command_line::add_arg(desc, arg_max_txpool_weight);
    command_line::add_arg(desc, arg_pad_transactions);
    command_line::add_arg(desc, arg_block_notify);
#if 0
    command_line::add_arg(desc, arg_prune_blockchain);
#endif
    command_line::add_arg(desc, arg_reorg_notify);
    command_line::add_arg(desc, arg_keep_alt_blocks);
    command_line::add_arg(desc, arg_service_node);
    command_line::add_arg(desc, arg_public_ip);
    command_line::add_arg(desc, arg_sn_bind_port);
    command_line::add_arg(desc, arg_arqnet_port);
    command_line::add_arg(desc, arg_store_quorum_history);

    miner::init_options(desc);
    BlockchainDB::init_options(desc);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_command_line(const boost::program_options::variables_map& vm)
  {
    if (m_nettype != FAKECHAIN)
    {
      const bool testnet = command_line::get_arg(vm, arg_testnet_on);
      const bool stagenet = command_line::get_arg(vm, arg_stagenet_on);
      m_nettype = testnet ? TESTNET : stagenet ? STAGENET : MAINNET;
    }

    m_config_folder = command_line::get_arg(vm, arg_data_dir);

    test_drop_download_height(command_line::get_arg(vm, arg_test_drop_download_height));
    m_pad_transactions = get_arg(vm, arg_pad_transactions);
    m_offline = get_arg(vm, arg_offline);

    if (command_line::get_arg(vm, arg_test_drop_download) == true)
      test_drop_download();

    bool service_node = command_line::get_arg(vm, arg_service_node);

    if (service_node) {
      m_service_node_keys = std::make_unique<service_node_keys>();

      m_storage_port = command_line::get_arg(vm, arg_sn_bind_port);
      m_arqnet_port = command_line::get_arg(vm, arg_arqnet_port);

      bool storage_ok = true;
      if (m_storage_port == 0) {
        MERROR("Please specify the port on which the storage server is listening with: '--" << arg_sn_bind_port.name << " <port>'");
        storage_ok = false;
      }

      if (m_arqnet_port == 0) {
        MERROR("Arq-Net port cannot be 0; Please specify a valid port to listen on with: '--" << arg_arqnet_port.name << " <port>'");
        storage_ok = false;
      }

      const std::string pub_ip = command_line::get_arg(vm, arg_public_ip);
      if (pub_ip.size())
      {
        if (!epee::string_tools::get_ip_int32_from_string(m_sn_public_ip, pub_ip)) {
          MERROR("Unable to parse IPv4 public address from: " << pub_ip);
          storage_ok = false;
        }

        if (!epee::net_utils::is_ip_public(m_sn_public_ip))
        {
          MERROR("Address given for public-ip is not public: " << epee::string_tools::get_ip_string_from_int32(m_sn_public_ip));
          storage_ok = false;
        }
      }
      else
      {
        MERROR("Please specify an IPv4 public address which the service node & storage server is accessible from with: '--" << arg_public_ip.name << " <ip address>'");
        storage_ok = false;
      }

      if (!storage_ok)
      {
        MERROR("IMPORTANT: All service node operators are now required to run arqma storage\nserver and provide the public ip and ports on which it can be accessed on the internet.");
        return false;
      }

      MGINFO("Storage server endpoint is set to: " << (epee::net_utils::ipv4_network_address{ m_sn_public_ip, m_storage_port }).str());
    }

    epee::debug::g_test_dbg_lock_sleep() = command_line::get_arg(vm, arg_test_dbg_lock_sleep);

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_current_blockchain_height() const
  {
    return m_blockchain_storage.get_current_blockchain_height();
  }
  //-----------------------------------------------------------------------------------------------
  void core::get_blockchain_top(uint64_t& height, crypto::hash& top_id) const
  {
    top_id = m_blockchain_storage.get_tail_id(height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks, std::vector<cryptonote::blobdata>& txs) const
  {
    return m_blockchain_storage.get_blocks(start_offset, count, blocks, txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks) const
  {
    return m_blockchain_storage.get_blocks(start_offset, count, blocks);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_blocks(uint64_t start_offset, size_t count, std::vector<block>& blocks) const
  {
    std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> bs;
    if (!m_blockchain_storage.get_blocks(start_offset, count, bs))
      return false;
    for (const auto &b: bs)
      blocks.push_back(b.second);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_transactions(const std::vector<crypto::hash>& txs_ids, std::vector<cryptonote::blobdata>& txs, std::vector<crypto::hash>& missed_txs) const
  {
    return m_blockchain_storage.get_transactions_blobs(txs_ids, txs, missed_txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_split_transactions_blobs(const std::vector<crypto::hash>& txs_ids, std::vector<std::tuple<crypto::hash, cryptonote::blobdata, crypto::hash, cryptonote::blobdata> >& txs, std::vector<crypto::hash>& missed_txs) const
  {
    return m_blockchain_storage.get_split_transactions_blobs(txs_ids, txs, missed_txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_transactions(const std::vector<crypto::hash>& txs_ids, std::vector<transaction>& txs, std::vector<crypto::hash>& missed_txs) const
  {
    return m_blockchain_storage.get_transactions(txs_ids, txs, missed_txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_alternative_blocks(std::vector<block>& blocks) const
  {
    return m_blockchain_storage.get_alternative_blocks(blocks);
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_alternative_blocks_count() const
  {
    return m_blockchain_storage.get_alternative_blocks_count();
  }
  //-----------------------------------------------------------------------------------------------
#ifdef ENABLE_SYSTEMD
  static std::string time_ago_str(time_t now, time_t then)
  {
    if (then >= now)
      return "now"s;
    if (then == 0)
      return "never"s;
    int seconds = now - then;
    if (seconds >= 60)
      return std::to_string(seconds / 60) + "m" + std::to_string(seconds % 60) + "s";
    return std::to_string(seconds % 60) + "s";
  }

  static std::string get_systemd_status_string(const core &c)
  {
    std::string s;
    s.reserve(128);
    s += "Height: ";
    s += std::to_string(c.get_blockchain_storage().get_current_blockchain_height());
    s += ", SN: ";
    auto keys = c.get_service_node_keys();
    if (!keys)
      s += "no";
    else
    {
      auto &snl = c.get_service_node_list();
      auto states = snl.get_service_node_list_state({ keys->pub });
      if (states.empty())
        s += "not registered";
      else
      {
        auto &info = *states[0].info;
        if (!info.is_fully_funded())
          s += "awaiting contr.";
        else if (info.is_active())
          s += "active";
        else if (info.is_decommissioned())
          s += "decomm.";

        uint64_t last_proof = 0;
        snl.access_proof(keys->pub, [&](auto &proof) { last_proof = proof.timestamp; });
        s += ", proof: ";
        time_t now = std::time(nullptr);
        s += time_ago_str(now, last_proof);
        s += ", storage: ";
        s += time_ago_str(now, c.m_last_storage_server_ping);
      }
    }
    return s;
  }
#endif
  //-----------------------------------------------------------------------------------------------
  bool core::init(const boost::program_options::variables_map& vm, const cryptonote::test_options *test_options, const GetCheckpointsCallback& get_checkpoints/* = nullptr */)
  {
    start_time = std::time(nullptr);

    const bool regtest = command_line::get_arg(vm, arg_regtest_on);
    if (test_options != NULL || regtest)
    {
      m_nettype = FAKECHAIN;
    }
    bool r = handle_command_line(vm);
    CHECK_AND_ASSERT_MES(r, false, "Failed to handle command line");

    std::string db_sync_mode = command_line::get_arg(vm, cryptonote::arg_db_sync_mode);
    bool db_salvage = command_line::get_arg(vm, cryptonote::arg_db_salvage) != 0;
    bool fast_sync = command_line::get_arg(vm, arg_fast_block_sync) != 0;
    uint64_t blocks_threads = command_line::get_arg(vm, arg_prep_blocks_threads);
    std::string check_updates_string = command_line::get_arg(vm, arg_check_updates);
    size_t max_txpool_weight = command_line::get_arg(vm, arg_max_txpool_weight);
    bool const prune_blockchain = false; /*command_line::get_arg(vm, arg_prune_blockchain);*/
    bool keep_alt_blocks = command_line::get_arg(vm, arg_keep_alt_blocks);

    if(m_service_node_keys)
    {
      r = init_service_node_keys();
      CHECK_AND_ASSERT_MES(r, false, "Failed to create or load service node key");
      m_service_node_list.set_my_service_node_keys(m_service_node_keys.get());
    }

    boost::filesystem::path folder(m_config_folder);
    if (m_nettype == FAKECHAIN)
      folder /= "fake";

    // make sure the data directory exists, and try to lock it
    CHECK_AND_ASSERT_MES (boost::filesystem::exists(folder) || boost::filesystem::create_directories(folder), false,
      std::string("Failed to create directory ").append(folder.string()).c_str());

    // check for blockchain.bin
    try
    {
      const boost::filesystem::path old_files = folder;
      if (boost::filesystem::exists(old_files / "blockchain.bin"))
      {
        MWARNING("Found old-style blockchain.bin in " << old_files.string());
        MWARNING("ArQmA now uses a new format. You can either remove blockchain.bin to start syncing");
        MWARNING("the blockchain anew, or use arqma-blockchain-export and arqma-blockchain-import to");
        MWARNING("convert your existing blockchain.bin to the new format. See README.md for instructions.");
        return false;
      }
    }
    // folder might not be a directory, etc, etc
    catch (...) { }

    std::unique_ptr<BlockchainDB> db(new_db());
    if (db == NULL)
    {
      LOG_ERROR("Failed to initialize a database");
      return false;
    }

    folder /= db->get_db_name();
    MGINFO("Loading blockchain from folder " << folder.string() << " ...");

    const std::string filename = folder.string();
    // default to fast:async:1 if overridden
    blockchain_db_sync_mode sync_mode = db_defaultsync;
    bool sync_on_blocks = true;
    uint64_t sync_threshold = 1;

    if (m_nettype == FAKECHAIN)
    {
      // reset the db by removing the database file before opening it
      if (!db->remove_data_file(filename))
      {
        MERROR("Failed to remove data file in " << filename);
        return false;
      }
    }

    try
    {
      uint64_t db_flags = 0;

      std::vector<std::string> options;
      boost::trim(db_sync_mode);
      boost::split(options, db_sync_mode, boost::is_any_of(" :"));
      const bool db_sync_mode_is_default = command_line::is_arg_defaulted(vm, cryptonote::arg_db_sync_mode);

      for(const auto &option : options)
        MDEBUG("option: " << option);

      // default to fast:async:1
      uint64_t DEFAULT_FLAGS = DBF_FAST;

      if(options.size() == 0)
      {
        // default to fast:async:1
        db_flags = DEFAULT_FLAGS;
      }

      bool safemode = false;
      if(options.size() >= 1)
      {
        if(options[0] == "safe")
        {
          safemode = true;
          db_flags = DBF_SAFE;
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_nosync;
        }
        else if(options[0] == "fast")
        {
          db_flags = DBF_FAST;
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_async;
        }
        else if(options[0] == "fastest")
        {
          db_flags = DBF_FASTEST;
          sync_threshold = 1000; // default to fastest:async:1000
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_async;
        }
        else
          db_flags = DEFAULT_FLAGS;
      }

      if(options.size() >= 2 && !safemode)
      {
        if(options[1] == "sync")
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_sync;
        else if(options[1] == "async")
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_async;
      }

      if(options.size() >= 3 && !safemode)
      {
        char *endptr;
        uint64_t threshold = strtoull(options[2].c_str(), &endptr, 0);
        if (*endptr == '\0' || !strcmp(endptr, "blocks"))
        {
          sync_on_blocks = true;
          sync_threshold = threshold;
        }
        else if (!strcmp(endptr, "bytes"))
        {
          sync_on_blocks = false;
          sync_threshold = threshold;
        }
        else
        {
          LOG_ERROR("Invalid db sync mode: " << options[2]);
          return false;
        }
      }

      if (db_salvage)
        db_flags |= DBF_SALVAGE;

      db->open(filename, m_nettype, db_flags);
      if(!db->m_open)
        return false;
    }
    catch (const DB_ERROR& e)
    {
      LOG_ERROR("Error opening database: " << e.what());
      return false;
    }

    m_blockchain_storage.set_user_options(blocks_threads, sync_on_blocks, sync_threshold, sync_mode, fast_sync);

    try
    {
      if(!command_line::is_arg_defaulted(vm, arg_block_notify))
        m_blockchain_storage.set_block_notify(std::shared_ptr<tools::Notify>(new tools::Notify(command_line::get_arg(vm, arg_block_notify).c_str())));
    }
    catch (const std::exception& e)
    {
      MERROR("Failed to parse block notify spec");
    }

    try
    {
      if (!command_line::is_arg_defaulted(vm, arg_reorg_notify))
      m_blockchain_storage.set_reorg_notify(std::shared_ptr<tools::Notify>(new tools::Notify(command_line::get_arg(vm, arg_reorg_notify).c_str())));
    }
    catch (const std::exception& e)
    {
      MERROR("Failed to parse reorg notify spec");
    }

    const std::vector<std::pair<uint8_t, uint64_t>> regtest_hard_forks = {std::make_pair(cryptonote::network_version_count - 1, 1)};
    const cryptonote::test_options regtest_test_options = {
      regtest_hard_forks
    };

    // SN
    {
      m_service_node_list.set_quorum_history_storage(command_line::get_arg(vm, arg_store_quorum_history));

      m_blockchain_storage.hook_block_added(m_service_node_list);
      m_blockchain_storage.hook_blockchain_detached(m_service_node_list);
      m_blockchain_storage.hook_init(m_service_node_list);
      m_blockchain_storage.hook_validate_miner_tx(m_service_node_list);
      m_blockchain_storage.hook_alt_block_added(m_service_node_list);

      m_blockchain_storage.hook_init(m_quorum_cop);
      m_blockchain_storage.hook_block_added(m_quorum_cop);
      m_blockchain_storage.hook_blockchain_detached(m_quorum_cop);
    }

    // Checkpoints
    {
      auto data_dir = boost::filesystem::path(m_config_folder);
      boost::filesystem::path json(JSON_HASH_FILE_NAME);
      boost::filesystem::path checkpoint_json_hashfile_fullpath = data_dir / json;
      m_checkpoints_path = checkpoint_json_hashfile_fullpath.string();
    }

    const difficulty_type fixed_difficulty = command_line::get_arg(vm, arg_fixed_difficulty);
    r = m_blockchain_storage.init(db.release(), m_nettype, m_offline, regtest ? &regtest_test_options : test_options, fixed_difficulty, get_checkpoints);

    r = m_mempool.init(max_txpool_weight);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize memory pool");

    // now that we have a valid m_blockchain_storage, we can clean out any
    // transactions in the pool that do not conform to the current fork
    m_mempool.validate(m_blockchain_storage.get_current_hard_fork_version());

    bool show_time_stats = command_line::get_arg(vm, arg_show_time_stats) != 0;
    m_blockchain_storage.set_show_time_stats(show_time_stats);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize blockchain storage");

    block_sync_size = command_line::get_arg(vm, arg_block_sync_size);
    if(block_sync_size > BLOCKS_SYNCHRONIZING_MAX_COUNT)
      MERROR("Error --dblock_sync_size cannot be greater than " << BLOCKS_SYNCHRONIZING_MAX_COUNT);

    MGINFO("Loading checkpoints");

    CHECK_AND_ASSERT_MES(update_checkpoints_from_json_file(), false, "One or more checkpoints loaded from json conflicted with existing checkpoints.");

   //DNS versions checking
    if (check_updates_string == "disabled")
      check_updates_level = UPDATES_DISABLED;
    else if (check_updates_string == "notify")
      check_updates_level = UPDATES_NOTIFY;
    else if (check_updates_string == "download")
      check_updates_level = UPDATES_DOWNLOAD;
    else if (check_updates_string == "update")
      check_updates_level = UPDATES_UPDATE;
    else {
      MERROR("Invalid argument to --dns-versions-check: " << check_updates_string);
      return false;
    }

    r = m_miner.init(vm, m_nettype);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize miner instance");

    if(!keep_alt_blocks && !m_blockchain_storage.get_db().is_read_only())
      m_blockchain_storage.get_db().drop_alt_blocks();

    if (prune_blockchain)
    {
      // display a message if the blockchain is not pruned yet
      if (m_blockchain_storage.get_current_blockchain_height() > 1 && !m_blockchain_storage.get_blockchain_pruning_seed())
        MGINFO("Pruning blockchain...");
      CHECK_AND_ASSERT_MES(m_blockchain_storage.prune_blockchain(), false, "Failed to prune blockchain");
    }

    if (m_service_node_keys)
    {
      std::lock_guard<std::mutex> lock{m_arqnet_init_mutex};
      std::string listen_ip = vm["p2p-bind-ip"].as<std::string>();
      if (listen_ip.empty())
        listen_ip = "0.0.0.0";
      std::string arqnet_listen = "tcp://" + listen_ip + ":" + std::to_string(m_arqnet_port);
      m_arqnet_obj = arqnet_new(*this, arqnet_listen);
    }

#ifdef ENABLE_SYSTEMD
    sd_notify(0, ("READY=1\nSTATUS=" + get_systemd_status_string(*this)).c_str());
#endif

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  template <typename Privkey, typename Pubkey, typename GetPubkey, typename GeneratePair>
  bool init_key(const std::string &keypath, Privkey &privkey, Pubkey &pubkey, GetPubkey get_pubkey, GeneratePair generate_pair)
  {
    if (epee::file_io_utils::is_file_exist(keypath))
    {
      std::string keystr;
      bool r = epee::file_io_utils::load_file_to_string(keypath, keystr);
      memcpy(&unwrap(unwrap(privkey)), keystr.data(), sizeof(privkey));
      memwipe(&keystr[0], keystr.size());
      CHECK_AND_ASSERT_MES(r, false, "failed to load service node key from " + keypath);
      CHECK_AND_ASSERT_MES(keystr.size() == sizeof(privkey), false, "service node key file " + keypath + " has an invalid size");

      r = get_pubkey(privkey, pubkey);
      CHECK_AND_ASSERT_MES(r, false, "failed to generate pubkey from secret key");
    }
    else
    {
      generate_pair(privkey, pubkey);

      std::string keystr(reinterpret_cast<const char *>(&privkey), sizeof(privkey));
      bool r = epee::file_io_utils::save_string_to_file(keypath, keystr);
      memwipe(&keystr[0], keystr.size());
      CHECK_AND_ASSERT_MES(r, false, "failed to save service node key to " + keypath);

      using namespace boost::filesystem;
      permissions(keypath, owner_read);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::init_service_node_keys()
  {
    auto &keys = *m_service_node_keys;

    if (!init_key(m_config_folder + "/key", keys.key, keys.pub,
          crypto::secret_key_to_public_key,
          [](crypto::secret_key &key, crypto::public_key &pubkey) {
            cryptonote::keypair keypair = keypair::generate(hw::get_device("default"));
            key = keypair.sec;
            pubkey = keypair.pub;
          })
        )
      return false;

    MGINFO_BLUE("Arqma Service Node primary pubkey is " << epee::string_tools::pod_to_hex(keys.pub));

    static_assert(sizeof(crypto::ed25519_public_key) == crypto_sign_ed25519_PUBLICKEYBYTES &&
                  sizeof(crypto::ed25519_secret_key) == crypto_sign_ed25519_SECRETKEYBYTES &&
                  sizeof(crypto::ed25519_signature) == crypto_sign_BYTES &&
                  sizeof(crypto::x25519_public_key) == crypto_scalarmult_curve25519_BYTES &&
                  sizeof(crypto::x25519_secret_key) == crypto_scalarmult_curve25519_BYTES,
                  "Invalid ed25519/x25519 sizes");

    if (!init_key(m_config_folder + "/key_ed25519", keys.key_ed25519, keys.pub_ed25519,
          [](crypto::ed25519_secret_key &sk, crypto::ed25519_public_key &pk) { crypto_sign_ed25519_sk_to_pk(pk.data, sk.data); return true; },
          [](crypto::ed25519_secret_key &sk, crypto::ed25519_public_key &pk) { crypto_sign_ed25519_keypair(pk.data, sk.data); })
       )
      return false;

    MGINFO_BLUE("Arqma Service Node ed25519 pubkey is " << epee::string_tools::pod_to_hex(keys.pub_ed25519));

    int rc = crypto_sign_ed25519_pk_to_curve25519(keys.pub_x25519.data, keys.pub_ed25519.data);
    CHECK_AND_ASSERT_MES(rc == 0, false, "failed to convert ed25519 pubkey to x25519");
    crypto_sign_ed25519_sk_to_curve25519(keys.key_x25519.data, keys.key_ed25519.data);

    MGINFO_BLUE("Arqma Service Node x25519 pubkey is " << epee::string_tools::pod_to_hex(keys.pub_x25519));

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::set_genesis_block(const block& b)
  {
    return m_blockchain_storage.reset_and_set_genesis_block(b);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::deinit()
  {
#ifdef ENABLE_SYSTEMD
    sd_notify(0, "STOPPING=1\nSTATUS=Shutting down");
#endif
    if (m_arqnet_obj)
      arqnet_delete(m_arqnet_obj);
    m_service_node_list.store();
    m_miner.stop();
    m_mempool.deinit();
    m_blockchain_storage.deinit();
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::test_drop_download()
  {
    m_test_drop_download = false;
  }
  //-----------------------------------------------------------------------------------------------
  void core::test_drop_download_height(uint64_t height)
  {
    m_test_drop_download_height = height;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_test_drop_download() const
  {
    return m_test_drop_download;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_test_drop_download_height() const
  {
    if (m_test_drop_download_height == 0)
      return true;

    if (get_blockchain_storage().get_current_blockchain_height() <= m_test_drop_download_height)
      return true;

    return false;
  }
  //-----------------------------------------------------------------------------------------------
  void core::parse_incoming_tx_pre(tx_verification_batch_info &tx_info)
  {
    uint8_t hf = m_blockchain_storage.get_current_hard_fork_version();
    uint64_t max_tx_size;

    if(hf < 13)
    {
      max_tx_size = get_max_tx_size() * 4;
    }
    else
    {
      max_tx_size = get_max_tx_size();
    }

    if(tx_info.blob->size() > max_tx_size)
    {
      LOG_PRINT_L1("WRONG TRANSACTION BLOB, too big size " << tx_info.blob->size() << ", rejected");
      tx_info.tvc.m_verification_failed = true;
      tx_info.tvc.m_too_big = true;
      return;
    }

    tx_info.parsed = parse_tx_from_blob(tx_info.tx, tx_info.tx_hash, *tx_info.blob);
    if(!tx_info.parsed)
    {
      LOG_PRINT_L1("WRONG TRANSACTION BLOB, Failed to parse, rejected");
      tx_info.tvc.m_verification_failed = true;
      return;
    }
    //std::cout << "!"<< tx.vin.size() << std::endl;

    std::lock_guard lock{bad_semantics_txes_lock};
    for (int idx = 0; idx < 2; ++idx)
    {
      if (bad_semantics_txes[idx].find(tx_info.tx_hash) != bad_semantics_txes[idx].end())
      {
        LOG_PRINT_L1("Transaction already seen with bad semantics, rejected");
        tx_info.tvc.m_verification_failed = true;
        return;
      }
    }
    tx_info.result = true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::set_semantics_failed(const crypto::hash &tx_hash)
  {
    LOG_PRINT_L1("WRONG TRANSACTION BLOB, Failed to check tx " << tx_hash << " semantic, rejected");
    bad_semantics_txes_lock.lock();
    bad_semantics_txes[0].insert(tx_hash);
    if (bad_semantics_txes[0].size() >= BAD_SEMANTICS_TXES_MAX_SIZE)
    {
      std::swap(bad_semantics_txes[0], bad_semantics_txes[1]);
      bad_semantics_txes[0].clear();
    }
    bad_semantics_txes_lock.unlock();
  }
  //-----------------------------------------------------------------------------------------------
  static bool is_canonical_bulletproof_layout(const std::vector<rct::Bulletproof> &proofs)
  {
    if (proofs.size() != 1)
      return false;
    const size_t sz = proofs[0].V.size();
    if (sz == 0 || sz > BULLETPROOF_MAX_OUTPUTS)
      return false;
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::parse_incoming_tx_accumulated_batch(std::vector<tx_verification_batch_info> &tx_info, bool kept_by_block)
  {
    if(kept_by_block && get_blockchain_storage().is_within_compiled_block_hash_area())
    {
      MTRACE("Skipping semantics check for tx kept by block in embedded hash area");
      return;
    }

    std::vector<const rct::rctSig*> rvv;
    for(size_t n = 0; n < tx_info.size(); ++n)
    {
      if (!tx_info[n].result || tx_info[n].already_have)
        continue;

      if(!check_tx_semantic(tx_info[n].tx, kept_by_block))
      {
        set_semantics_failed(tx_info[n].tx_hash);
        tx_info[n].tvc.m_verification_failed = true;
        tx_info[n].result = false;
        continue;
      }

      if(!tx_info[n].tx.is_transfer() || tx_info[n].tx.version < txversion::v2)
        continue;
      const rct::rctSig &rv = tx_info[n].tx.rct_signatures;
      switch(rv.type) {
        case rct::RCTTypeNull:
          // coinbase should not come here, so we reject for all other types
          MERROR_VER("Unexpected Null rctSig type");
          set_semantics_failed(tx_info[n].tx_hash);
          tx_info[n].tvc.m_verification_failed = true;
          tx_info[n].result = false;
          break;
        case rct::RCTTypeSimpleBulletproof:
          if(!rct::verRctSemanticsSimple_old(rv))
          {
            MERROR_VER("rct signature semantics check failed");
            set_semantics_failed(tx_info[n].tx_hash);
            tx_info[n].tvc.m_verification_failed = true;
            tx_info[n].result = false;
            break;
          }
          break;
        case rct::RCTTypeSimple:
          if(!rct::verRctSemanticsSimple(rv))
          {
            MERROR_VER("rct signature semantics check failed");
            set_semantics_failed(tx_info[n].tx_hash);
            tx_info[n].tvc.m_verification_failed = true;
            tx_info[n].result = false;
            break;
          }
          break;
        case rct::RCTTypeFull:
        case rct::RCTTypeFullBulletproof:
          if(!rct::verRct(rv, true))
          {
            MERROR_VER("rct signature semantics check failed");
            set_semantics_failed(tx_info[n].tx_hash);
            tx_info[n].tvc.m_verification_failed = true;
            tx_info[n].result = false;
            break;
          }
          break;
        case rct::RCTTypeBulletproof:
        case rct::RCTTypeBulletproof2:
          if(!is_canonical_bulletproof_layout(rv.p.bulletproofs))
          {
            MERROR_VER("Bulletproof does not have canonical form");
            set_semantics_failed(tx_info[n].tx_hash);
            tx_info[n].tvc.m_verification_failed = true;
            tx_info[n].result = false;
            break;
          }
          rvv.push_back(&rv); // delayed batch verification
          break;
        default:
          MERROR_VER("Unknown rct type: " << rv.type);
          set_semantics_failed(tx_info[n].tx_hash);
          tx_info[n].tvc.m_verification_failed = true;
          tx_info[n].result = false;
          break;
      }
    }
    if(!rvv.empty() && !rct::verRctSemanticsSimple(rvv))
    {
      LOG_PRINT_L1("One transaction among this group has bad semantics, verifying one at a time");
      const bool assumed_bad = rvv.size() == 1; // if there's only one tx, it must be the bad one
      for(size_t n = 0; n < tx_info.size(); ++n)
      {
        if(!tx_info[n].result || tx_info[n].already_have)
          continue;
        if(tx_info[n].tx.rct_signatures.type != rct::RCTTypeBulletproof && tx_info[n].tx.rct_signatures.type != rct::RCTTypeBulletproof2)
          continue;
        if(assumed_bad || !rct::verRctSemanticsSimple(tx_info[n].tx.rct_signatures))
        {
          set_semantics_failed(tx_info[n].tx_hash);
          tx_info[n].tvc.m_verification_failed = true;
          tx_info[n].result = false;
        }
      }
    }
  }
  //-----------------------------------------------------------------------------------------------
  std::vector<core::tx_verification_batch_info> core::parse_incoming_txs(const std::vector<blobdata>& tx_blobs, const tx_pool_options &opts)
  {
    std::vector<tx_verification_batch_info> tx_info(tx_blobs.size());

    tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
    tools::threadpool::waiter waiter(tpool);
    for (size_t i = 0; i < tx_blobs.size(); i++)
    {
      tx_info[i].blob = &tx_blobs[i];
      tpool.submit(&waiter, [this, &info = tx_info[i]] {
        try
        {
          parse_incoming_tx_pre(info);
        }
        catch (const std::exception& e)
        {
          MERROR_VER("Exception in handle_incoming_tx_pre: " << e.what());
          info.tvc.m_verification_failed = true;
        }
      });
    }
    waiter.wait();

    for (auto &info : tx_info)
    {
      if (!info.result)
        continue;

      if (m_mempool.have_tx(info.tx_hash))
      {
        LOG_PRINT_L2("tx " << info.tx_hash << " already have transaction in tx_pool");
        info.already_have = true;
      }
      else if(m_blockchain_storage.have_tx(info.tx_hash))
      {
        LOG_PRINT_L2("tx " << info.tx_hash << " already have transaction in blockchain");
        info.already_have = true;
      }
    }

    parse_incoming_tx_accumulated_batch(tx_info, opts.kept_by_block);

    return tx_info;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_parsed_txs(std::vector<tx_verification_batch_info> &parsed_txs, const tx_pool_options &opts)
  {
    uint8_t hard_fork_version = m_blockchain_storage.get_current_hard_fork_version();
    bool ok = true;
    tx_pool_options tx_opts;
    for (size_t i = 0; i < parsed_txs.size(); i++)
    {
      auto &info = parsed_txs[i];
      if (!info.result)
      {
        ok = false;
        continue;
      }
      if (opts.kept_by_block)
        get_blockchain_storage().on_new_tx_from_block(info.tx);
      if (info.already_have)
        continue;

      const size_t weight = get_transaction_weight(info.tx, info.blob->size());
      const tx_pool_options *local_opts = &opts;
      if (m_mempool.add_tx(info.tx, info.tx_hash, *info.blob, weight, info.tvc, *local_opts, hard_fork_version))
        MDEBUG("tx added: " << info.tx_hash);
      else
      {
        ok = false;
        if (info.tvc.m_verification_failed)
          MERROR_VER("Transaction verification failed: " << info.tx_hash);
        else if (info.tvc.m_verification_impossible)
          MERROR_VER("Transaction verification impossible: " << info.tx_hash);
      }
    }
    return ok;
  }
  //-----------------------------------------------------------------------------------------------
  std::vector<core::tx_verification_batch_info> core::handle_incoming_txs(const std::vector<blobdata>& tx_blobs, const tx_pool_options &opts)
  {
    auto lock = incoming_tx_lock();
    auto parsed = parse_incoming_txs(tx_blobs, opts);
    handle_parsed_txs(parsed, opts);
    return parsed;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_tx(const blobdata& tx_blob, tx_verification_context& tvc, const tx_pool_options &opts)
  {
    const std::vector<cryptonote::blobdata> tx_blobs{{tx_blob}};
    auto parsed = handle_incoming_txs(tx_blobs, opts);
    parsed[0].blob = &tx_blob;
    tvc = parsed[0].tvc;
    return parsed[0].result && (parsed[0].already_have || tvc.m_added_to_pool);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_semantic(const transaction& tx, bool keeped_by_block) const
  {
    if(tx.is_transfer())
    {
      if(tx.vin.empty())
      {
        MERROR_VER("Transaction with empty inputs, rejected for tx id: " << get_transaction_hash(tx));
        return false;
      }
    }
    else
    {
      if(tx.vin.size() != 0)
      {
        MERROR_VER("Transaction type: " << tx.tx_type << " must have 0 inputs, received: " << tx.vin.size() << ", rejected for tx id = " << get_transaction_hash(tx));
        return false;
      }
    }

    if(!check_inputs_types_supported(tx))
    {
      MERROR_VER("unsupported input types for tx id= " << get_transaction_hash(tx));
      return false;
    }

    if(!check_outs_valid(tx))
    {
      MERROR_VER("tx with invalid outputs, rejected for tx id= " << get_transaction_hash(tx));
      return false;
    }
    if(tx.version >= txversion::v2)
    {
      if(tx.rct_signatures.outPk.size() != tx.vout.size())
      {
        MERROR_VER("tx with mismatched vout/outPk count, rejected for tx id= " << get_transaction_hash(tx));
        return false;
      }
    }

    if(!check_money_overflow(tx))
    {
      MERROR_VER("tx has money overflow, rejected for tx id= " << get_transaction_hash(tx));
      return false;
    }

    if(tx.version == txversion::v1)
    {
      uint64_t amount_in = 0;
      get_inputs_money_amount(tx, amount_in);
      uint64_t amount_out = get_outs_money_amount(tx);

      if(amount_in <= amount_out)
      {
        MERROR_VER("tx with wrong amounts: ins " << amount_in << ", outs " << amount_out << ", rejected for tx id= " << get_transaction_hash(tx));
        return false;
      }
    }

    if(!keeped_by_block && get_transaction_weight(tx) >= m_blockchain_storage.get_current_cumulative_block_weight_limit() - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE)
    {
      MERROR_VER("tx is too large " << get_transaction_weight(tx) << ", expected not bigger than " << m_blockchain_storage.get_current_cumulative_block_weight_limit() - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE);
      return false;
    }

    if(!check_tx_inputs_keyimages_diff(tx))
    {
      MERROR_VER("tx uses a single key image more than once");
      return false;
    }

    if(!check_tx_inputs_ring_members_diff(tx))
    {
      MERROR_VER("tx uses duplicate ring members");
      return false;
    }

    if(!check_tx_inputs_keyimages_domain(tx))
    {
      MERROR_VER("tx uses key image not in the valid domain");
      return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::is_key_image_spent(const crypto::key_image &key_image) const
  {
    return m_blockchain_storage.have_tx_keyimg_as_spent(key_image);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::are_key_images_spent(const std::vector<crypto::key_image>& key_im, std::vector<bool> &spent) const
  {
    spent.clear();
    for(auto& ki: key_im)
    {
      spent.push_back(m_blockchain_storage.have_tx_keyimg_as_spent(ki));
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_block_sync_size(uint64_t height) const
  {
    static const uint64_t quick_height = m_nettype == MAINNET ? config::blockchain_settings::sync_height : m_nettype == STAGENET ? 0 : 0;
    size_t res = 0;
    if(block_sync_size > 0)
      res = block_sync_size;
    else if(height >= quick_height)
      res = config::sync::NORMAL_SYNC;
    else
      res = config::sync::FAST_SYNC;

    static size_t max_block_size = 0;
    if(max_block_size == 0)
    {
      const char *env = getenv("SEEDHASH_EPOCH_BLOCKS");
      if (env)
      {
        int n = atoi(env);
        if (n <= 0)
          n = config::sync::MAX_SYNC;
        size_t p = 1;
        while (p < (size_t)n)
          p <<= 1;
        max_block_size = p;
      }
      else
        max_block_size = config::sync::MAX_SYNC;
    }
    if (res > max_block_size)
    {
      static bool warned = false;
      if(!warned)
      {
        MWARNING("Clamping block sync size to " << max_block_size);
        warned = true;
      }
      res = max_block_size;
    }
    return res;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::are_key_images_spent_in_pool(const std::vector<crypto::key_image>& key_im, std::vector<bool> &spent) const
  {
    spent.clear();

    return m_mempool.check_for_key_images(key_im, spent);
  }
  //-----------------------------------------------------------------------------------------------
  std::pair<uint64_t, uint64_t> core::get_coinbase_tx_sum(const uint64_t start_offset, const size_t count)
  {
    uint64_t emission_amount = 0;
    uint64_t total_fee_amount = 0;
    if (count)
    {
      const uint64_t end = start_offset + count - 1;
      m_blockchain_storage.for_blocks_range(start_offset, end,
        [this, &emission_amount, &total_fee_amount](uint64_t, const crypto::hash& hash, const block& b){
      std::vector<transaction> txs;
      std::vector<crypto::hash> missed_txs;
      uint64_t coinbase_amount = get_outs_money_amount(b.miner_tx);
      this->get_transactions(b.tx_hashes, txs, missed_txs);
      uint64_t tx_fee_amount = 0;
      for(const auto& tx: txs)
      {
        tx_fee_amount += get_tx_fee(tx);
      }

      emission_amount += coinbase_amount - tx_fee_amount;
      total_fee_amount += tx_fee_amount;
      return true;
      });
      // Remove Burned Premine Amount from coinbase emission
      if (start_offset <= 1 && 1 <= end)
      {
        emission_amount -= config::blockchain_settings::PREMINE_BURN;
      }
    }

    return std::pair<uint64_t, uint64_t>(emission_amount, total_fee_amount);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_inputs_keyimages_diff(const transaction& tx) const
  {
    std::unordered_set<crypto::key_image> ki;
    for(const auto& in: tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      if(!ki.insert(tokey_in.k_image).second)
        return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_inputs_ring_members_diff(const transaction& tx) const
  {
    for(const auto& in: tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      for (size_t n = 1; n < tokey_in.key_offsets.size(); ++n)
        if (tokey_in.key_offsets[n] == 0)
          return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_inputs_keyimages_domain(const transaction& tx) const
  {
    std::unordered_set<crypto::key_image> ki;
    for(const auto& in: tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      if (!(rct::scalarmultKey(rct::ki2rct(tokey_in.k_image), rct::curveOrder()) == rct::identity()))
        return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_blockchain_total_transactions() const
  {
    return m_blockchain_storage.get_total_transactions();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::relay_txpool_transactions()
  {
    // we attempt to relay txes that should be relayed, but were not
    std::vector<std::pair<crypto::hash, cryptonote::blobdata>> txs;
    if (m_mempool.get_relayable_transactions(txs) && !txs.empty())
    {
      cryptonote_connection_context fake_context;
      tx_verification_context tvc{};
      NOTIFY_NEW_TRANSACTIONS::request r;
      for (auto it = txs.begin(); it != txs.end(); ++it)
      {
        r.txs.push_back(it->second);
      }
      get_protocol()->relay_transactions(r, fake_context);
      m_mempool.set_relayed(txs);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::submit_uptime_proof()
  {
    if (!m_service_node_keys)
      return true;

    NOTIFY_UPTIME_PROOF::request req = m_service_node_list.generate_uptime_proof(*m_service_node_keys, m_sn_public_ip, m_storage_port, m_arqnet_port);

    cryptonote_connection_context fake_context{};
    bool relayed = get_protocol()->relay_uptime_proof(req, fake_context);
    if (relayed)
      MGINFO("Submitted uptime-proof for service node (yours): " << m_service_node_keys->pub);

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_uptime_proof(const NOTIFY_UPTIME_PROOF::request &proof, bool &my_uptime_proof_confirmation)
  {
    return m_service_node_list.handle_uptime_proof(proof, my_uptime_proof_confirmation);
  }
  //-----------------------------------------------------------------------------------------------
  crypto::hash core::on_transaction_relayed(const cryptonote::blobdata& tx_blob)
  {
    std::vector<std::pair<crypto::hash, cryptonote::blobdata>> txs;
    cryptonote::transaction tx;
    crypto::hash tx_hash;
    if (!parse_and_validate_tx_from_blob(tx_blob, tx, tx_hash))
    {
      LOG_ERROR("Failed to parse relayed transaction");
      return crypto::null_hash;
    }
    txs.push_back(std::make_pair(tx_hash, std::move(tx_blob)));
    m_mempool.set_relayed(txs);
    return tx_hash;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::relay_service_node_votes()
  {
    auto height = get_current_blockchain_height();
    auto hard_fork_version = get_hard_fork_version(height);

    auto quorum_votes = m_quorum_cop.get_relayable_votes(height, hard_fork_version, true);
    auto p2p_votes = m_quorum_cop.get_relayable_votes(height, hard_fork_version, false);
    if (!quorum_votes.empty() && m_arqnet_obj && m_service_node_keys)
      arqnet_relay_obligation_votes(m_arqnet_obj, quorum_votes);

    if (!p2p_votes.empty())
    {
      NOTIFY_NEW_SERVICE_NODE_VOTE::request req{};
      req.votes = std::move(p2p_votes);
      cryptonote_connection_context fake_context{};
      get_protocol()->relay_service_node_votes(req, fake_context);
    }

    return true;
  }
  void core::set_service_node_votes_relayed(const std::vector<service_nodes::quorum_vote_t> &votes)
  {
    m_quorum_cop.set_votes_relayed(votes);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_block_template(block& b, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
  {
    return m_blockchain_storage.create_block_template(b, adr, diffic, height, expected_reward, ex_nonce, seed_height, seed_hash);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_block_template(block& b, const crypto::hash *prev_block, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
  {
    return m_blockchain_storage.create_block_template(b, prev_block, adr, diffic, height, expected_reward, ex_nonce, seed_height, seed_hash);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) const
  {
    return m_blockchain_storage.find_blockchain_supplement(qblock_ids, resp);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::find_blockchain_supplement(const uint64_t req_start_block, const std::list<crypto::hash>& qblock_ids, std::vector<std::pair<std::pair<cryptonote::blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, cryptonote::blobdata> > > >& blocks, uint64_t& total_height, uint64_t& start_height, bool pruned, bool get_miner_tx_hash, size_t max_block_count, size_t max_tx_count) const
  {
    return m_blockchain_storage.find_blockchain_supplement(req_start_block, qblock_ids, blocks, total_height, start_height, pruned, get_miner_tx_hash, max_block_count, max_tx_count);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_outs(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res) const
  {
    return m_blockchain_storage.get_outs(req, res);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_output_distribution(uint64_t amount, uint64_t from_height, uint64_t to_height, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) const
  {
    return m_blockchain_storage.get_output_distribution(amount, from_height, to_height, start_height, distribution, base);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_output_blacklist(std::vector<uint64_t> &blacklist) const
  {
    return m_blockchain_storage.get_output_blacklist(blacklist);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) const
  {
    return m_blockchain_storage.get_tx_outputs_gindexs(tx_id, indexs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_tx_outputs_gindexs(const crypto::hash& tx_id, size_t n_txes, std::vector<std::vector<uint64_t>>& indexs) const
  {
    return m_blockchain_storage.get_tx_outputs_gindexs(tx_id, n_txes, indexs);
  }
  //-----------------------------------------------------------------------------------------------
  void core::pause_mine()
  {
    m_miner.pause();
  }
  //-----------------------------------------------------------------------------------------------
  void core::resume_mine()
  {
    m_miner.resume();
  }
  //-----------------------------------------------------------------------------------------------
  block_complete_entry get_block_complete_entry(block& b, tx_memory_pool &pool)
  {
    block_complete_entry bce;
    bce.block = cryptonote::block_to_blob(b);
    for (const auto &tx_hash: b.tx_hashes)
    {
      cryptonote::blobdata txblob;
      CHECK_AND_ASSERT_THROW_MES(pool.get_transaction(tx_hash, txblob), "Transaction not found in pool");
      bce.txs.push_back(txblob);
    }
    return bce;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_block_found(block& b, block_verification_context &bvc)
  {
    bvc = {};
    std::vector<block_complete_entry> blocks;
    m_miner.pause();
    {
      ARQMA_DEFER { m_miner.resume(); };
      try
      {
        blocks.push_back(get_block_complete_entry(b, m_mempool));
      }
      catch (const std::exception &e)
      {
        return false;
      }
      std::vector<block> pblocks;
      if (!prepare_handle_incoming_blocks(blocks, pblocks))
      {
        MERROR("Block found, but failed to prepare to add");
        return false;
      }
      add_new_block(b, bvc, nullptr);
      cleanup_handle_incoming_blocks(true);
      m_miner.on_block_chain_update();
    }

    CHECK_AND_ASSERT_MES(!bvc.m_verification_failed, false, "mined block failed verification");
    if(bvc.m_added_to_main_chain)
    {
      std::vector<crypto::hash> missed_txs;
      std::vector<cryptonote::blobdata> txs;
      m_blockchain_storage.get_transactions_blobs(b.tx_hashes, txs, missed_txs);
      if(missed_txs.size() &&  m_blockchain_storage.get_block_id_by_height(get_block_height(b)) != get_block_hash(b))
      {
        LOG_PRINT_L1("Block found but, seems that reorganize just happened after that, do not relay this block");
        return true;
      }
      CHECK_AND_ASSERT_MES(txs.size() == b.tx_hashes.size() && !missed_txs.size(), false, "can't find some transactions in found block:" << get_block_hash(b) << " txs.size()=" << txs.size()
        << ", b.tx_hashes.size()=" << b.tx_hashes.size() << ", missed_txs.size()" << missed_txs.size());

      cryptonote_connection_context exclude_context{};
      NOTIFY_NEW_FLUFFY_BLOCK::request arg{};
      arg.current_blockchain_height = m_blockchain_storage.get_current_blockchain_height();
      arg.b = blocks[0];

      m_pprotocol->relay_block(arg, exclude_context);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::on_synchronized()
  {
    m_miner.on_synchronized();
  }
  //-----------------------------------------------------------------------------------------------
  void core::safesyncmode(const bool onoff)
  {
    m_blockchain_storage.safesyncmode(onoff);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::add_new_block(const block& b, block_verification_context& bvc, checkpoint_t const *checkpoint)
  {
    bool result = m_blockchain_storage.add_new_block(b, bvc, checkpoint);
    if (result)
    {
      relay_service_node_votes(); // NOTE: not working while syncing.
    }
    return result;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::prepare_handle_incoming_blocks(const std::vector<block_complete_entry> &blocks_entry, std::vector<block> &blocks)
  {
    m_incoming_tx_lock.lock();
    if(!m_blockchain_storage.prepare_handle_incoming_blocks(blocks_entry, blocks))
    {
      cleanup_handle_incoming_blocks(false);
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::cleanup_handle_incoming_blocks(bool force_sync)
  {
    bool success = false;
    try {
      success = m_blockchain_storage.cleanup_handle_incoming_blocks(force_sync);
    }
    catch (...) {}
    m_incoming_tx_lock.unlock();
    return success;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_block(const blobdata& block_blob, const block *b, block_verification_context& bvc, checkpoint_t *checkpoint, bool update_miner_blocktemplate)
  {
    TRY_ENTRY();

    bvc = {};

    if(!check_incoming_block_size(block_blob))
    {
      bvc.m_verification_failed = true;
      return false;
    }

    CHECK_AND_ASSERT_MES(update_checkpoints_from_json_file(), false, "One or more checkpoints loaded from json conflicted with existing checkpoints.");

    block lb;
    if(!b)
    {
      crypto::hash block_hash;
      if(!parse_and_validate_block_from_blob(block_blob, lb, block_hash))
      {
        LOG_PRINT_L1("Failed to parse and validate new block");
        bvc.m_verification_failed = true;
        return false;
      }
      b = &lb;
    }

    add_new_block(*b, bvc, checkpoint);
    if(update_miner_blocktemplate && bvc.m_added_to_main_chain)
      m_miner.on_block_chain_update();
    return true;

    CATCH_ENTRY_L0("core::handle_incoming_block()", false);
  }
  //-----------------------------------------------------------------------------------------------
  // Used by the RPC server to check the size of an incoming
  // block_blob
  bool core::check_incoming_block_size(const blobdata& block_blob) const
  {
    // note: we assume block weight is always >= block blob size, so we check incoming
    // blob size against the block weight limit, which acts as a sanity check without
    // having to parse/weigh first; in fact, since the block blob is the block header
    // plus the tx hashes, the weight will typically be much larger than the blob size
    if(block_blob.size() > config::blockchain_settings::MAXIMUM_BLOCK_SIZE_LIMIT)
    {
      LOG_PRINT_L1("WRONG BLOCK BLOB, sanity check failed on size " << block_blob.size() << ", rejected");
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  crypto::hash core::get_tail_id() const
  {
    return m_blockchain_storage.get_tail_id();
  }
  //-----------------------------------------------------------------------------------------------
  difficulty_type core::get_block_cumulative_difficulty(uint64_t height) const
  {
    return m_blockchain_storage.get_db().get_block_cumulative_difficulty(height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::have_block(const crypto::hash& id) const
  {
    return m_blockchain_storage.have_block(id);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::parse_tx_from_blob(transaction& tx, crypto::hash& tx_hash, const blobdata& blob) const
  {
    return parse_and_validate_tx_from_blob(blob, tx, tx_hash);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp, cryptonote_connection_context& context)
  {
    return m_blockchain_storage.handle_get_objects(arg, rsp);
  }
  //-----------------------------------------------------------------------------------------------
  crypto::hash core::get_block_id_by_height(uint64_t height) const
  {
    return m_blockchain_storage.get_block_id_by_height(height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_block_by_hash(const crypto::hash &h, block &blk, bool *orphan) const
  {
    return m_blockchain_storage.get_block_by_hash(h, blk, orphan);
  }
  //-----------------------------------------------------------------------------------------------
  static bool check_external_ping(time_t last_ping, time_t lifetime, const char *what)
  {
    const auto elapsed = std::time(nullptr) - last_ping;
    if (elapsed > lifetime)
    {
      MWARNING("Have not heard from " << what << " " << (!last_ping ? "since starting" :
               "for more than " + tools::get_human_readable_timespan(std::chrono::seconds(elapsed))));
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::reset_proof_interval()
  {
    m_check_uptime_proof_interval.reset();
  }

  void core::do_uptime_proof_call()
  {
    std::vector<service_nodes::service_node_pubkey_info> const states = get_service_node_list_state({ m_service_node_keys->pub });
    // wait one block before starting uptime proofs.
    if(!states.empty() && (states[0].info->registration_height + 1) < get_current_blockchain_height())
    {
      m_check_uptime_proof_interval.do_call([this]()
      {
        uint64_t next_proof_time = 0;
        m_service_node_list.access_proof(m_service_node_keys->pub, [&](auto &proof) { next_proof_time = proof.timestamp; });
        next_proof_time += UPTIME_PROOF_FREQUENCY_IN_SECONDS + UPTIME_PROOF_TIMER_SECONDS/2;

        if ((uint64_t) std::time(nullptr) < next_proof_time)
          return;

        if (!check_external_ping(m_last_storage_server_ping, STORAGE_SERVER_PING_LIFETIME, "the storage server"))
        {
          MGINFO_RED("Failed to submit uptime proof: have not heard from the storage server recently.\n Make sure that it is running!. It is required to run alongside with the Arqma daemon");
          return;
        }

/*
        uint8_t hard_fork_version = get_blockchain_storage().get_current_hard_fork_version();
        if (!check_external_ping(m_last_arqnet_ping, ARQNET_PING_LIFETIME, "Arq-Net"))
        {
          if (hard_fork_version >= cryptonote::network_version_17)
          {
            MGINFO_RED("Failed to submit uptime proof: have not heard from the Arq-Net recently.\n Make sure that it is running!. It is required to run alongside with the Arqma daemon");
            return;
          }
          else
          {
            MGINFO_RED("Have not heard from the Arq-Net recently.\n Make sure that it is running!. It is required to run alongside with the Arqma daemon after Hard Fork 17");
          }
        }
*/
        submit_uptime_proof();
      });
    }
    else
    {
      // reset the interval so that we're ready when we register.
      m_check_uptime_proof_interval.reset();
    }
  }
  //-----------------------------------------------------------------------------------------------
  // messages moved to ascii.h
  bool core::on_idle()
  {
    if(!m_starter_message_showed)
    {
      MGINFO_YELLOW(ENDL << ascii_arqma_logo << ENDL);
      MGINFO_CYAN(ENDL << ascii_arqma_info << ENDL);

      if(m_offline)
        MGINFO_GREEN(ENDL << main_message_true << ENDL);
      else
        MGINFO_GREEN(ENDL << main_message_false << ENDL);

      m_starter_message_showed = true;
    }

    m_txpool_auto_relayer.do_call([this] { return relay_txpool_transactions(); });
    m_service_node_vote_relayer.do_call([this] { return relay_service_node_votes(); });
    m_check_disk_space_interval.do_call([this] { return check_disk_space(); });
    m_blockchain_pruning_interval.do_call([this] { return update_blockchain_pruning(); });
    m_sn_proof_cleanup_interval.do_call([&snl=m_service_node_list] { snl.cleanup_proofs(); return true; });

    time_t const lifetime = time(nullptr) - get_start_time();
    if(m_service_node_keys && lifetime > UPTIME_PROOF_INITIAL_DELAY_SECONDS)
    {
      do_uptime_proof_call();
    }

    m_miner.on_idle();
    m_mempool.on_idle();

#ifdef ENABLE_SYSTEMD
    m_systemd_notify_interval.do_call([this] { sd_notify(0, ("WATCHDOG=1\nSTATUS=" + get_systemd_status_string(*this)).c_str()); });
#endif

    return true;
  }

  //-----------------------------------------------------------------------------------------------
  uint8_t core::get_ideal_hard_fork_version() const
  {
    return get_blockchain_storage().get_ideal_hard_fork_version();
  }
  //-----------------------------------------------------------------------------------------------
  uint8_t core::get_ideal_hard_fork_version(uint64_t height) const
  {
    return get_blockchain_storage().get_ideal_hard_fork_version(height);
  }
  //-----------------------------------------------------------------------------------------------
  uint8_t core::get_hard_fork_version(uint64_t height) const
  {
    return get_blockchain_storage().get_hard_fork_version(height);
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_earliest_ideal_height_for_version(uint8_t version) const
  {
    return get_blockchain_storage().get_earliest_ideal_height_for_version(version);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_updates()
  {
    static const char software[] = "arqma";
#ifdef BUILD_TAG
    static const char buildtag[] = BOOST_PP_STRINGIZE(BUILD_TAG);
//    static const char subdir[] = "cli"; // because it can never be simple
#else
    static const char buildtag[] = "source";
//    static const char subdir[] = "source"; // because it can never be simple
#endif

    if (m_offline)
      return true;

    if (check_updates_level == UPDATES_DISABLED)
      return true;

    std::string version, hash;
    MCDEBUG("updates", "Checking for a new " << software << " version for " << buildtag);
    if (!tools::check_updates(software, buildtag, version, hash))
      return false;

    if (tools::vercmp(version.c_str(), ARQMA_VERSION_STR) <= 0)
    {
      m_update_available = false;
      return true;
    }

    std::string url = tools::get_update_url(software, buildtag, version, true);
    MCLOG_CYAN(el::Level::Info, "global", "Version " << version << " of " << software << " for " << buildtag << " is available: " << url << ", SHA256 hash " << hash);
    m_update_available = true;

    if (check_updates_level == UPDATES_NOTIFY)
      return true;

    url = tools::get_update_url(software, buildtag, version, false);
    std::string filename;
    const char *slash = strrchr(url.c_str(), '/');
    if (slash)
      filename = slash + 1;
    else
      filename = std::string(software) + "-update-" + version;
    boost::filesystem::path path(epee::string_tools::get_current_module_folder());
    path /= filename;

    std::unique_lock lock{m_update_mutex};

    if (m_update_download != 0)
    {
      MCDEBUG("updates", "Already downloading update");
      return true;
    }

    crypto::hash file_hash;
    if (!tools::sha256sum(path.string(), file_hash) || (hash != epee::string_tools::pod_to_hex(file_hash)))
    {
      MCDEBUG("updates", "We don't have that file already, downloading");
      const std::string tmppath = path.string() + ".tmp";
      if (epee::file_io_utils::is_file_exist(tmppath))
      {
        MCDEBUG("updates", "We have part of the file already, resuming download");
      }
      m_last_update_length = 0;
      m_update_download = tools::download_async(tmppath, url, [this, hash, path](const std::string &tmppath, const std::string &uri, bool success) {
        bool remove = false, good = true;
        if (success)
        {
          crypto::hash file_hash;
          if (!tools::sha256sum(tmppath, file_hash))
          {
            MCERROR("updates", "Failed to hash " << tmppath);
            remove = true;
            good = false;
          }
          else if (hash != epee::string_tools::pod_to_hex(file_hash))
          {
            MCERROR("updates", "Download from " << uri << " does not match the expected hash");
            remove = true;
            good = false;
          }
        }
        else
        {
          MCERROR("updates", "Failed to download " << uri);
          good = false;
        }
        std::unique_lock lock{m_update_mutex};
        m_update_download = 0;
        if (success && !remove)
        {
          std::error_code e = tools::replace_file(tmppath, path.string());
          if (e)
          {
            MCERROR("updates", "Failed to rename downloaded file");
            good = false;
          }
        }
        else if (remove)
        {
          if (!boost::filesystem::remove(tmppath))
          {
            MCERROR("updates", "Failed to remove invalid downloaded file");
            good = false;
          }
        }
        if (good)
          MCLOG_CYAN(el::Level::Info, "updates", "New version downloaded to " << path.string());
      }, [this](const std::string &path, const std::string &uri, size_t length, ssize_t content_length) {
        if (length >= m_last_update_length + 1024 * 1024 * 10)
        {
          m_last_update_length = length;
          MCDEBUG("updates", "Downloaded " << length << "/" << (content_length ? std::to_string(content_length) : "unknown"));
        }
        return true;
      });
    }
    else
    {
      MCDEBUG("updates", "We already have " << path << " with expected hash");
    }

    lock.unlock();

    if (check_updates_level == UPDATES_DOWNLOAD)
      return true;

    MCERROR("updates", "Download/update not implemented yet");
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_disk_space()
  {
    uint64_t free_space = get_free_space();
    if (free_space < 1ull * 1024 * 1024 * 1024) // 1 GB
    {
      const el::Level level = el::Level::Warning;
      MCLOG_RED(level, "global", "Free space is below 1 GB on " << m_config_folder);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::set_storage_server_peer_reachable(crypto::public_key const &pubkey, bool value)
  {
    return m_service_node_list.set_storage_server_peer_reachable(pubkey, value);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::update_blockchain_pruning()
  {
    return m_blockchain_storage.update_blockchain_pruning();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_blockchain_pruning()
  {
    return m_blockchain_storage.check_blockchain_pruning();
  }
  //-----------------------------------------------------------------------------------------------
  void core::set_target_blockchain_height(uint64_t target_blockchain_height)
  {
    m_target_blockchain_height = target_blockchain_height;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_target_blockchain_height() const
  {
    return m_target_blockchain_height;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::prevalidate_block_hashes(uint64_t height, const std::vector<crypto::hash> &hashes)
  {
    return get_blockchain_storage().prevalidate_block_hashes(height, hashes);
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_free_space() const
  {
    boost::filesystem::path path(m_config_folder);
    boost::filesystem::space_info si = boost::filesystem::space(path);
    return si.available;
  }
  //-----------------------------------------------------------------------------------------------
  uint32_t core::get_blockchain_pruning_seed() const
  {
    return get_blockchain_storage().get_blockchain_pruning_seed();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::prune_blockchain(uint32_t pruning_seed)
  {
    return get_blockchain_storage().prune_blockchain(pruning_seed);
  }
  //-----------------------------------------------------------------------------------------------
  std::shared_ptr<const service_nodes::quorum> core::get_quorum(service_nodes::quorum_type type, uint64_t height, bool include_old, std::vector<std::shared_ptr<const service_nodes::quorum>> *alt_states) const
  {
    return m_service_node_list.get_quorum(type, height, include_old, alt_states);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::is_service_node(const crypto::public_key& pubkey, bool require_active) const
  {
    return m_service_node_list.is_service_node(pubkey, require_active);
  }
  //-----------------------------------------------------------------------------------------------
  const std::vector<service_nodes::key_image_blacklist_entry> &core::get_service_node_blacklisted_key_images() const
  {
    return m_service_node_list.get_blacklisted_key_images();
  }
  //-----------------------------------------------------------------------------------------------
  std::vector<service_nodes::service_node_pubkey_info> core::get_service_node_list_state(const std::vector<crypto::public_key> &service_node_pubkeys) const
  {
    return m_service_node_list.get_service_node_list_state(service_node_pubkeys);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::add_service_node_vote(const service_nodes::quorum_vote_t& vote, vote_verification_context &vvc)
  {
    return m_quorum_cop.handle_vote(vote, vvc);
  }
  //-----------------------------------------------------------------------------------------------
  const core::service_node_keys* core::get_service_node_keys() const
  {
    return m_service_node_keys.get();
  }
  //-----------------------------------------------------------------------------------------------
  std::time_t core::get_start_time() const
  {
    return start_time;
  }
  //-----------------------------------------------------------------------------------------------
  void core::graceful_exit()
  {
    raise(SIGTERM);
  }
}
