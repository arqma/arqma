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

#include "string_tools.h"

#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/verification_context.h"
#include "cryptonote_basic/difficulty.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_core/service_node_voting.h"
#include "rpc/rpc_handler.h"
#include "common/varint.h"
#include "common/perf_timer.h"
#include "checkpoints/checkpoints.h"

#include "cryptonote_core/service_node_quorum_cop.h"
#include "cryptonote_core/service_node_list.h"

namespace
{
  template<typename T>
  std::string compress_integer_array(const std::vector<T> &v)
  {
    std::string s;
    s.resize(v.size() * (sizeof(T) * 8 / 7 + 1));
    char *ptr = (char*)s.data();
    for (const T &t: v)
      tools::write_varint(ptr, t);
    s.resize(ptr - s.data());
    return s;
  }

  template<typename T>
  std::vector<T> decompress_integer_array(const std::string &s)
  {
    std::vector<T> v;
    v.reserve(s.size());
    int read = 0;
    const std::string::const_iterator end = s.end();
    for (std::string::const_iterator i = s.begin(); i != end; std::advance(i, read))
    {
      T t;
      read = tools::read_varint(std::string::const_iterator(i), s.end(), t);
      CHECK_AND_ASSERT_THROW_MES(read > 0 && read <= 256, "Error decompressing data");
      v.push_back(t);
    }
    return v;
  }
}

namespace cryptonote
{
  //-----------------------------------------------
#define CORE_RPC_STATUS_OK   "OK"
#define CORE_RPC_STATUS_BUSY   "BUSY"
#define CORE_RPC_STATUS_NOT_MINING "NOT MINING"
constexpr char const CORE_RPC_STATUS_TX_LONG_POLL_TIMED_OUT[] = "Long polling client timed out before txpool had an update";
constexpr char const CORE_RPC_STATUS_TX_LONG_POLL_MAX_CONNECTIONS[] = "Daemon maxed out long polling connections";

// When making *any* change here, bump minor
// If the change is incompatible, then bump major and set minor to 0
// This ensures CORE_RPC_VERSION always increases, that every change
// has its own version, and that clients can just test major to see
// whether they can talk to a given daemon without having to know in
// advance which version they will stop working with
// Don't go over 32767 for any of these
#define CORE_RPC_VERSION_MAJOR 4
#define CORE_RPC_VERSION_MINOR 3
#define MAKE_CORE_RPC_VERSION(major,minor) (((major)<<16)|(minor))
#define CORE_RPC_VERSION MAKE_CORE_RPC_VERSION(CORE_RPC_VERSION_MAJOR, CORE_RPC_VERSION_MINOR)

  struct COMMAND_RPC_GET_HEIGHT
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t height;
      std::string status;
      bool untrusted;
      std::string hash;
      uint64_t immutable_height;
      std::string immutable_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(immutable_height)
        KV_SERIALIZE(immutable_hash)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_BLOCKS_FAST
  {
    struct request_t
    {
      std::list<crypto::hash> block_ids; //*first 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */
      uint64_t    start_height;
      bool        prune;
      bool        no_miner_tx;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(prune)
        KV_SERIALIZE_OPT(no_miner_tx, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct tx_output_indices
    {
      std::vector<uint64_t> indices;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(indices)
      END_KV_SERIALIZE_MAP()
    };

    struct block_output_indices
    {
      std::vector<tx_output_indices> indices;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(indices)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<block_complete_entry> blocks;
      uint64_t    start_height;
      uint64_t    current_height;
      std::string status;
      std::vector<block_output_indices> output_indices;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(current_height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(output_indices)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_BLOCKS_BY_HEIGHT
  {
    struct request_t
    {
      std::vector<uint64_t> heights;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(heights)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<block_complete_entry> blocks;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_ALT_BLOCKS_HASHES
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<std::string> blks_hashes;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blks_hashes)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_HASHES_FAST
  {
    struct request_t
    {
      std::list<crypto::hash> block_ids; //*first 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */
      uint64_t    start_height;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
        KV_SERIALIZE(start_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<crypto::hash> m_block_ids;
      uint64_t    start_height;
      uint64_t    current_height;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(m_block_ids)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(current_height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  struct COMMAND_RPC_GET_RANDOM_OUTS
  {
      struct request_t
      {
        std::vector<std::string> amounts;
        uint32_t count;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(amounts)
          KV_SERIALIZE(count)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<request_t> request;


      struct output {
        std::string public_key;
        uint64_t global_index;
        std::string rct; // 64+64+64 characters long (<rct commit> + <encrypted mask> + <rct amount>)

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(public_key)
          KV_SERIALIZE(global_index)
          KV_SERIALIZE(rct)
        END_KV_SERIALIZE_MAP()
      };

      struct amount_out {
        uint64_t amount;
        std::vector<output> outputs;
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(amount)
          KV_SERIALIZE(outputs)
        END_KV_SERIALIZE_MAP()

      };

      struct response_t
      {
        std::vector<amount_out> amount_outs;
        std::string Error;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(amount_outs)
          KV_SERIALIZE(Error)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct COMMAND_RPC_SUBMIT_RAW_TX
  {
      struct request_t
      {
        std::string address;
        std::string view_key;
        std::string tx;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(address)
          KV_SERIALIZE(view_key)
          KV_SERIALIZE(tx)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<request_t> request;


      struct response_t
      {
        std::string status;
        std::string error;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(status)
          KV_SERIALIZE(error)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_TRANSACTIONS
  {
    struct request_t
    {
      std::vector<std::string> txs_hashes;
      bool decode_as_json;
      bool prune;
      bool split;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs_hashes)
        KV_SERIALIZE(decode_as_json)
        KV_SERIALIZE_OPT(prune, false)
        KV_SERIALIZE_OPT(split, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct entry
    {
      std::string tx_hash;
      std::string as_hex;
      std::string pruned_as_hex;
      std::string prunable_as_hex;
      std::string prunable_hash;
      std::string as_json;
      bool in_pool;
      bool double_spend_seen;
      uint64_t block_height;
      uint64_t block_timestamp;
      std::vector<uint64_t> output_indices;
      bool relayed;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tx_hash)
        KV_SERIALIZE(as_hex)
        KV_SERIALIZE(pruned_as_hex)
        KV_SERIALIZE(prunable_as_hex)
        KV_SERIALIZE(prunable_hash)
        KV_SERIALIZE(as_json)
        KV_SERIALIZE(in_pool)
        KV_SERIALIZE(double_spend_seen)
        KV_SERIALIZE(block_height)
        KV_SERIALIZE(block_timestamp)
        KV_SERIALIZE(output_indices)
        KV_SERIALIZE(relayed)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      // older compatibility stuff
      std::vector<std::string> txs_as_hex;  //transactions blobs as hex (old compat)
      std::vector<std::string> txs_as_json; //transactions decoded as json (old compat)

      // in both old and new
      std::vector<std::string> missed_tx;   //not found transactions

      // new style
      std::vector<entry> txs;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs_as_hex)
        KV_SERIALIZE(txs_as_json)
        KV_SERIALIZE(txs)
        KV_SERIALIZE(missed_tx)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  struct COMMAND_RPC_IS_KEY_IMAGE_SPENT
  {
    enum STATUS {
      UNSPENT = 0,
      SPENT_IN_BLOCKCHAIN = 1,
      SPENT_IN_POOL = 2,
    };

    struct request_t
    {
      std::vector<std::string> key_images;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(key_images)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::vector<int> spent_status;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(spent_status)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  struct COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES
  {
    struct request_t
    {
      crypto::hash txid;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(txid)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::vector<uint64_t> o_indexes;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(o_indexes)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct get_outputs_out
  {
    uint64_t amount;
    uint64_t index;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(amount)
      KV_SERIALIZE(index)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_OUTPUTS_BIN
  {
    struct request_t
    {
      std::vector<get_outputs_out> outputs;
      bool get_txid;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outputs)
        KV_SERIALIZE_OPT(get_txid, true)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct outkey
    {
      crypto::public_key key;
      rct::key mask;
      bool unlocked;
      uint64_t height;
      crypto::hash txid;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(key)
        KV_SERIALIZE_VAL_POD_AS_BLOB(mask)
        KV_SERIALIZE(unlocked)
        KV_SERIALIZE(height)
        KV_SERIALIZE_VAL_POD_AS_BLOB(txid)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<outkey> outs;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outs)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_OUTPUTS
  {
    struct request_t
    {
      std::vector<get_outputs_out> outputs;
      bool get_txid;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outputs)
        KV_SERIALIZE(get_txid)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct outkey
    {
      std::string key;
      std::string mask;
      bool unlocked;
      uint64_t height;
      std::string txid;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(key)
        KV_SERIALIZE(mask)
        KV_SERIALIZE(unlocked)
        KV_SERIALIZE(height)
        KV_SERIALIZE(txid)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<outkey> outs;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outs)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct COMMAND_RPC_SEND_RAW_TX
  {
    struct request_t
    {
      std::string tx_as_hex;
      bool do_not_relay;
      bool do_sanity_checks;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tx_as_hex)
        KV_SERIALIZE_OPT(do_not_relay, false)
        KV_SERIALIZE_OPT(do_sanity_checks, true)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;
      std::string reason;
      bool not_relayed;
      bool sanity_check_failed;
      bool untrusted;
      tx_verification_context tvc;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(reason)
        KV_SERIALIZE(not_relayed)
        KV_SERIALIZE(sanity_check_failed)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(tvc)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct COMMAND_RPC_START_MINING
  {
    struct request_t
    {
      std::string miner_address;
      uint64_t    threads_count;
      bool        do_background_mining;
      bool        ignore_battery;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(miner_address)
        KV_SERIALIZE(threads_count)
        KV_SERIALIZE(do_background_mining)
        KV_SERIALIZE(ignore_battery)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_INFO
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint64_t height;
      uint64_t target_height;
      uint64_t immutable_height;
      uint64_t difficulty;
      uint64_t target;
      uint64_t tx_count;
      uint64_t tx_pool_size;
      uint64_t alt_blocks_count;
      uint64_t outgoing_connections_count;
      uint64_t incoming_connections_count;
      uint64_t rpc_connections_count;
      uint64_t white_peerlist_size;
      uint64_t grey_peerlist_size;
      bool mainnet;
      bool testnet;
      bool stagenet;
      std::string nettype;
      std::string top_block_hash;
      std::string immutable_block_hash;
      uint64_t cumulative_difficulty;
      uint64_t block_size_limit;
      uint64_t block_weight_limit;
      uint64_t block_size_median;
      uint64_t block_weight_median;
      uint64_t start_time;
      uint64_t last_storage_server_ping;
      uint64_t last_arqnet_ping;
      uint64_t free_space;
      bool offline;
      bool untrusted;
      std::string bootstrap_daemon_address;
      uint64_t height_without_bootstrap;
      bool was_bootstrap_ever_used;
      uint64_t database_size;
      bool update_available;
      std::string version;
      bool syncing;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(target_height)
        KV_SERIALIZE(immutable_height)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(target)
        KV_SERIALIZE(tx_count)
        KV_SERIALIZE(tx_pool_size)
        KV_SERIALIZE(alt_blocks_count)
        KV_SERIALIZE(outgoing_connections_count)
        KV_SERIALIZE(incoming_connections_count)
        KV_SERIALIZE(rpc_connections_count)
        KV_SERIALIZE(white_peerlist_size)
        KV_SERIALIZE(grey_peerlist_size)
        KV_SERIALIZE(mainnet)
        KV_SERIALIZE(testnet)
        KV_SERIALIZE(stagenet)
        KV_SERIALIZE(nettype)
        KV_SERIALIZE(top_block_hash)
        KV_SERIALIZE(immutable_block_hash)
        KV_SERIALIZE(cumulative_difficulty)
        KV_SERIALIZE(block_size_limit)
        KV_SERIALIZE_OPT(block_weight_limit, (uint64_t)0)
        KV_SERIALIZE(block_size_median)
        KV_SERIALIZE_OPT(block_weight_median, (uint64_t)0)
        KV_SERIALIZE(start_time)
        KV_SERIALIZE(last_storage_server_ping)
        KV_SERIALIZE(last_arqnet_ping)
        KV_SERIALIZE(free_space)
        KV_SERIALIZE(offline)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(bootstrap_daemon_address)
        KV_SERIALIZE(height_without_bootstrap)
        KV_SERIALIZE(was_bootstrap_ever_used)
        KV_SERIALIZE(database_size)
        KV_SERIALIZE(update_available)
        KV_SERIALIZE(version)
        KV_SERIALIZE(syncing)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };


  //-----------------------------------------------
  struct COMMAND_RPC_GET_NET_STATS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;
      uint64_t start_time;
      uint64_t total_packets_in;
      uint64_t total_bytes_in;
      uint64_t total_packets_out;
      uint64_t total_bytes_out;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(start_time)
        KV_SERIALIZE(total_packets_in)
        KV_SERIALIZE(total_bytes_in)
        KV_SERIALIZE(total_packets_out)
        KV_SERIALIZE(total_bytes_out)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  struct COMMAND_RPC_STOP_MINING
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  struct COMMAND_RPC_MINING_STATUS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;
      bool active;
      uint64_t speed;
      uint32_t threads_count;
      std::string address;
      bool is_background_mining_enabled;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(active)
        KV_SERIALIZE(speed)
        KV_SERIALIZE(threads_count)
        KV_SERIALIZE(address)
        KV_SERIALIZE(is_background_mining_enabled)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  struct COMMAND_RPC_SAVE_BC
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //
  struct COMMAND_RPC_GETBLOCKCOUNT
  {
    typedef std::list<std::string> request;

    struct response_t
    {
      uint64_t count;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(count)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GETBLOCKHASH
  {
    typedef std::vector<uint64_t> request;

    typedef std::string response;
  };


  struct COMMAND_RPC_GETBLOCKTEMPLATE
  {
    struct request_t
    {
      uint64_t reserve_size;       //max 255 bytes
      std::string wallet_address;
      std::string prev_block;
      std::string extra_nonce;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(reserve_size)
        KV_SERIALIZE(wallet_address)
        KV_SERIALIZE(prev_block)
        KV_SERIALIZE(extra_nonce)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t difficulty;
      uint64_t height;
      uint64_t reserved_offset;
      uint64_t expected_reward;
      std::string prev_hash;
      uint64_t seed_height;
      std::string seed_hash;
      std::string next_seed_hash;
      blobdata blocktemplate_blob;
      blobdata blockhashing_blob;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(height)
        KV_SERIALIZE(reserved_offset)
        KV_SERIALIZE(expected_reward)
        KV_SERIALIZE(prev_hash)
        KV_SERIALIZE(seed_height)
        KV_SERIALIZE(blocktemplate_blob)
        KV_SERIALIZE(blockhashing_blob)
        KV_SERIALIZE(seed_hash)
        KV_SERIALIZE(next_seed_hash)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SUBMITBLOCK
  {
    typedef std::vector<std::string> request;

    struct response_t
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GENERATEBLOCKS
  {
    struct request_t
    {
      uint64_t amount_of_blocks;
      std::string wallet_address;
      std::string prev_block;
      uint32_t starting_nonce;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount_of_blocks)
        KV_SERIALIZE(wallet_address)
        KV_SERIALIZE(prev_block)
        KV_SERIALIZE_OPT(starting_nonce, (uint32_t)0)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t height;
      std::vector<std::string> blocks;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct block_header_response
  {
      uint8_t major_version;
      uint8_t minor_version;
      uint64_t timestamp;
      std::string prev_hash;
      uint32_t nonce;
      bool orphan_status;
      uint64_t height;
      uint64_t depth;
      std::string hash;
      difficulty_type difficulty;
      difficulty_type cumulative_difficulty;
      uint64_t reward;
      uint64_t miner_reward;
      uint64_t miner_reward_unlock_block;
      uint64_t block_size;
      uint64_t block_weight;
      uint64_t num_txes;
      std::string pow_hash;
      uint64_t long_term_weight;
      std::string miner_tx_hash;
      std::string service_node_winner;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(major_version)
        KV_SERIALIZE(minor_version)
        KV_SERIALIZE(timestamp)
        KV_SERIALIZE(prev_hash)
        KV_SERIALIZE(nonce)
        KV_SERIALIZE(orphan_status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(depth)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(cumulative_difficulty)
        KV_SERIALIZE(reward)
        KV_SERIALIZE(miner_reward)
        KV_SERIALIZE(miner_reward_unlock_block)
        KV_SERIALIZE(block_size)
        KV_SERIALIZE_OPT(block_weight, (uint64_t)0)
        KV_SERIALIZE(num_txes)
        KV_SERIALIZE(pow_hash)
        KV_SERIALIZE_OPT(long_term_weight, (uint64_t)0)
        KV_SERIALIZE(miner_tx_hash)
        KV_SERIALIZE(service_node_winner)
      END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_LAST_BLOCK_HEADER
  {
    struct request_t
    {
      bool fill_pow_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      block_header_response block_header;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;

  };

  struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH
  {
    struct request_t
    {
      std::string hash;
      std::vector<std::string> hashes;
      bool fill_pow_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(hash)
        KV_SERIALIZE(hashes)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      block_header_response block_header;
      std::vector<block_header_response> block_headers;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(block_headers)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT
  {
    struct request_t
    {
      uint64_t height;
      bool fill_pow_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      block_header_response block_header;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_BLOCK
  {
    struct request_t
    {
      std::string hash;
      uint64_t height;
      bool fill_pow_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(hash)
        KV_SERIALIZE(height)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      block_header_response block_header;
      std::string miner_tx_hash;
      std::vector<std::string> tx_hashes;
      std::string blob;
      std::string json;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(miner_tx_hash)
        KV_SERIALIZE(tx_hashes)
        KV_SERIALIZE(status)
        KV_SERIALIZE(blob)
        KV_SERIALIZE(json)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct peer {
    uint64_t id;
    std::string host;
    uint32_t ip;
    uint16_t port;
    uint16_t rpc_port;
    uint64_t last_seen;
    uint32_t pruning_seed;

    peer() = default;

    peer(uint64_t id, const std::string &host, uint64_t last_seen, uint32_t pruning_seed, uint16_t rpc_port)
      : id(id), host(host), ip(0), port(0), rpc_port(rpc_port), last_seen(last_seen), pruning_seed(pruning_seed)
    {}
    peer(uint64_t id, const std::string &host, uint16_t port, uint64_t last_seen, uint32_t pruning_seed, uint16_t rpc_port)
      : id(id), host(host), ip(0), port(port), rpc_port(rpc_port), last_seen(last_seen), pruning_seed(pruning_seed)
    {}
    peer(uint64_t id, uint32_t ip, uint16_t port, uint64_t last_seen, uint32_t pruning_seed, uint16_t rpc_port)
      : id(id), host(epee::string_tools::get_ip_string_from_int32(ip)), ip(ip), port(port), rpc_port(rpc_port), last_seen(last_seen), pruning_seed(pruning_seed)
    {}

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(id)
      KV_SERIALIZE(host)
      KV_SERIALIZE(ip)
      KV_SERIALIZE(port)
      KV_SERIALIZE_OPT(rpc_port, (uint16_t)0)
      KV_SERIALIZE(last_seen)
      KV_SERIALIZE_OPT(pruning_seed, (uint32_t)0)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_PEER_LIST
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<peer> white_list;
      std::vector<peer> gray_list;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(white_list)
        KV_SERIALIZE(gray_list)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct public_node
  {
    std::string host;
    uint64_t last_seen;
    uint16_t rpc_port;

    public_node() = delete;

    public_node(const peer &peer)
      : host(peer.host), last_seen(peer.last_seen), rpc_port(peer.rpc_port)
    {}

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(host)
      KV_SERIALIZE(last_seen)
      KV_SERIALIZE(rpc_port)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_PUBLIC_NODES
  {
    struct request_t
    {
      bool gray;
      bool white;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(gray, false)
        KV_SERIALIZE_OPT(white, true)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<public_node> gray;
      std::vector<public_node> white;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(gray)
        KV_SERIALIZE(white)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SET_LOG_HASH_RATE
  {
    struct request_t
    {
      bool visible;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(visible)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SET_LOG_LEVEL
  {
    struct request_t
    {
      int8_t level;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(level)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SET_LOG_CATEGORIES
  {
    struct request_t
    {
      std::string categories;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(categories)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::string categories;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(categories)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct tx_info
  {
    std::string id_hash;
    std::string tx_json; // TODO - expose this data directly
    uint64_t blob_size;
    uint64_t weight;
    uint64_t fee;
    std::string max_used_block_id_hash;
    uint64_t max_used_block_height;
    bool kept_by_block;
    uint64_t last_failed_height;
    std::string last_failed_id_hash;
    uint64_t receive_time;
    bool relayed;
    uint64_t last_relayed_time;
    bool do_not_relay;
    bool double_spend_seen;
    std::string tx_blob;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(id_hash)
      KV_SERIALIZE(tx_json)
      KV_SERIALIZE(blob_size)
      KV_SERIALIZE_OPT(weight, (uint64_t)0)
      KV_SERIALIZE(fee)
      KV_SERIALIZE(max_used_block_id_hash)
      KV_SERIALIZE(max_used_block_height)
      KV_SERIALIZE(kept_by_block)
      KV_SERIALIZE(last_failed_height)
      KV_SERIALIZE(last_failed_id_hash)
      KV_SERIALIZE(receive_time)
      KV_SERIALIZE(relayed)
      KV_SERIALIZE(last_relayed_time)
      KV_SERIALIZE(do_not_relay)
      KV_SERIALIZE(double_spend_seen)
      KV_SERIALIZE(tx_blob)
    END_KV_SERIALIZE_MAP()
  };

  struct spent_key_image_info
  {
    std::string id_hash;
    std::vector<std::string> txs_hashes;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(id_hash)
      KV_SERIALIZE(txs_hashes)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_TRANSACTION_POOL
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<tx_info> transactions;
      std::vector<spent_key_image_info> spent_key_images;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(transactions)
        KV_SERIALIZE(spent_key_images)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN
  {
    struct request_t
    {
      bool long_poll;
      crypto::hash tx_pool_checksum;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(long_poll, false)
        KV_SERIALIZE_VAL_POD_AS_BLOB_OPT(tx_pool_checksum, crypto::hash{})
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<crypto::hash> tx_hashes;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(tx_hashes)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_TRANSACTION_POOL_HASHES
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<std::string> tx_hashes;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(tx_hashes)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct tx_backlog_entry
  {
    uint64_t weight;
    uint64_t fee;
    uint64_t time_in_pool;
  };

  struct COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<tx_backlog_entry> backlog;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(backlog)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct txpool_histo
  {
    uint32_t txs;
    uint64_t bytes;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(txs)
      KV_SERIALIZE(bytes)
    END_KV_SERIALIZE_MAP()
  };

  struct txpool_stats
  {
    uint64_t bytes_total;
    uint32_t bytes_min;
    uint32_t bytes_max;
    uint32_t bytes_med;
    uint64_t fee_total;
    uint64_t oldest;
    uint32_t txs_total;
    uint32_t num_failing;
    uint32_t num_10m;
    uint32_t num_not_relayed;
    uint64_t histo_98pc;
    std::vector<txpool_histo> histo;
    uint32_t num_double_spends;

    txpool_stats(): bytes_total(0), bytes_min(0), bytes_max(0), bytes_med(0), fee_total(0), oldest(0), txs_total(0), num_failing(0), num_10m(0), num_not_relayed(0), histo_98pc(0), num_double_spends(0) {}

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(bytes_total)
      KV_SERIALIZE(bytes_min)
      KV_SERIALIZE(bytes_max)
      KV_SERIALIZE(bytes_med)
      KV_SERIALIZE(fee_total)
      KV_SERIALIZE(oldest)
      KV_SERIALIZE(txs_total)
      KV_SERIALIZE(num_failing)
      KV_SERIALIZE(num_10m)
      KV_SERIALIZE(num_not_relayed)
      KV_SERIALIZE(histo_98pc)
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(histo)
      KV_SERIALIZE(num_double_spends)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_TRANSACTION_POOL_STATS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      txpool_stats pool_stats;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(pool_stats)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_CONNECTIONS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::list<connection_info> connections;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(connections)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

struct COMMAND_RPC_GET_BLOCKS_RANGE
  {
    struct blocks_transaction_entry
    {
      std::string tx_hash;
      std::string as_json;

      blocks_transaction_entry() = default;

      blocks_transaction_entry(const std::string &tx_hash, const std::string &as_json)
        : tx_hash(tx_hash), as_json(as_json)
      {}

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tx_hash)
        KV_SERIALIZE(as_json)
      END_KV_SERIALIZE_MAP()
    };

    struct blocks_entry {
      block_header_response block_header;
      std::vector<blocks_transaction_entry> txs;

      blocks_entry() = default;

      blocks_entry(block_header_response block_header, std::vector<blocks_transaction_entry>& txs)
        : block_header(block_header), txs(txs)
      {}

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(txs)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      uint64_t start_height;
      uint64_t end_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(end_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<blocks_entry> blocks;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_BLOCK_HEADERS_RANGE
  {
    struct request_t
    {
      uint64_t start_height;
      uint64_t end_height;
      bool fill_pow_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(end_height)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<block_header_response> headers;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(headers)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_STOP_DAEMON
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_FAST_EXIT
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_LIMIT
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint64_t limit_up;
      uint64_t limit_down;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(limit_up)
        KV_SERIALIZE(limit_down)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SET_LIMIT
  {
    struct request_t
    {
      int64_t limit_down;  // all limits (for get and set) are kB/s
      int64_t limit_up;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(limit_down)
        KV_SERIALIZE(limit_up)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      int64_t limit_up;
      int64_t limit_down;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(limit_up)
        KV_SERIALIZE(limit_down)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_OUT_PEERS
  {
    struct request_t
    {
      bool set;
      uint32_t out_peers;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(set, true)
        KV_SERIALIZE(out_peers)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint32_t out_peers;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(out_peers)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_IN_PEERS
  {
    struct request_t
    {
      bool set;
      uint32_t in_peers;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(set, true)
        KV_SERIALIZE(in_peers)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint32_t in_peers;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(in_peers)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_HARD_FORK_INFO
  {
    struct request_t
    {
      uint8_t version;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint8_t version;
      bool enabled;
      uint32_t window;
      uint32_t votes;
      uint32_t threshold;
      uint8_t voting;
      uint32_t state;
      uint64_t earliest_height;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version)
        KV_SERIALIZE(enabled)
        KV_SERIALIZE(window)
        KV_SERIALIZE(votes)
        KV_SERIALIZE(threshold)
        KV_SERIALIZE(voting)
        KV_SERIALIZE(state)
        KV_SERIALIZE(earliest_height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GETBANS
  {
    struct ban
    {
      std::string host;
      uint32_t ip;
      uint32_t seconds;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(host)
        KV_SERIALIZE(ip)
        KV_SERIALIZE(seconds)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::vector<ban> bans;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(bans)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SETBANS
  {
    struct ban
    {
      std::string host;
      uint32_t ip;
      bool ban;
      uint32_t seconds;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(host)
        KV_SERIALIZE(ip)
        KV_SERIALIZE(ban)
        KV_SERIALIZE(seconds)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      std::vector<ban> bans;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(bans)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_FLUSH_TRANSACTION_POOL
  {
    struct request_t
    {
      std::vector<std::string> txids;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txids)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_OUTPUT_HISTOGRAM
  {
    struct request_t
    {
      std::vector<uint64_t> amounts;
      uint64_t min_count;
      uint64_t max_count;
      bool unlocked;
      uint64_t recent_cutoff;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amounts)
        KV_SERIALIZE(min_count)
        KV_SERIALIZE(max_count)
        KV_SERIALIZE(unlocked)
        KV_SERIALIZE(recent_cutoff)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct entry
    {
      uint64_t amount;
      uint64_t total_instances;
      uint64_t unlocked_instances;
      uint64_t recent_instances;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount)
        KV_SERIALIZE(total_instances)
        KV_SERIALIZE(unlocked_instances)
        KV_SERIALIZE(recent_instances)
      END_KV_SERIALIZE_MAP()

      entry(uint64_t amount, uint64_t total_instances, uint64_t unlocked_instances, uint64_t recent_instances):
          amount(amount), total_instances(total_instances), unlocked_instances(unlocked_instances), recent_instances(recent_instances) {}
      entry() {}
    };

    struct response_t
    {
      std::string status;
      std::vector<entry> histogram;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(histogram)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_VERSION
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint32_t version;
      bool release;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(version)
        KV_SERIALIZE(release)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_COINBASE_TX_SUM
  {
    struct request_t
    {
      uint64_t height;
      uint64_t count;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(count)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint64_t emission_amount;
      uint64_t fee_amount;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(emission_amount)
        KV_SERIALIZE(fee_amount)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_BASE_FEE_ESTIMATE
  {
    struct request_t
    {
      uint64_t grace_blocks;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(grace_blocks)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint64_t fee;
      uint64_t quantization_mask;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(fee)
        KV_SERIALIZE_OPT(quantization_mask, (uint64_t)1)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_ALTERNATE_CHAINS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct chain_info
    {
      std::string block_hash;
      uint64_t height;
      uint64_t length;
      uint64_t difficulty;
      std::vector<std::string> block_hashes;
      std::string main_chain_parent_block;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_hash)
        KV_SERIALIZE(height)
        KV_SERIALIZE(length)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(block_hashes)
        KV_SERIALIZE(main_chain_parent_block)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::string status;
      std::vector<chain_info> chains;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(chains)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_UPDATE
  {
    struct request_t
    {
      std::string command;
      std::string path;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(command)
        KV_SERIALIZE(path)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      bool update;
      std::string version;
      std::string user_uri;
      std::string auto_uri;
      std::string hash;
      std::string path;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(update)
        KV_SERIALIZE(version)
        KV_SERIALIZE(user_uri)
        KV_SERIALIZE(auto_uri)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(path)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_RELAY_TX
  {
    struct request_t
    {
      std::vector<std::string> txids;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txids)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_SYNC_INFO
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct peer
    {
      connection_info info;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(info)
      END_KV_SERIALIZE_MAP()
    };

    struct span
    {
      uint64_t start_block_height;
      uint64_t nblocks;
      std::string connection_id;
      uint32_t rate;
      uint32_t speed;
      uint64_t size;
      std::string remote_address;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_block_height)
        KV_SERIALIZE(nblocks)
        KV_SERIALIZE(connection_id)
        KV_SERIALIZE(rate)
        KV_SERIALIZE(speed)
        KV_SERIALIZE(size)
        KV_SERIALIZE(remote_address)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::string status;
      uint64_t height;
      uint64_t target_height;
      uint32_t next_needed_pruning_seed;
      std::list<peer> peers;
      std::list<span> spans;
      std::string overview;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(target_height)
        KV_SERIALIZE(next_needed_pruning_seed)
        KV_SERIALIZE(peers)
        KV_SERIALIZE(spans)
        KV_SERIALIZE(overview)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_OUTPUT_DISTRIBUTION
  {
    struct request_t
    {
      std::vector<uint64_t> amounts;
      uint64_t from_height;
      uint64_t to_height;
      bool cumulative;
      bool binary;
      bool compress;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amounts)
        KV_SERIALIZE_OPT(from_height, (uint64_t)0)
        KV_SERIALIZE_OPT(to_height, (uint64_t)0)
        KV_SERIALIZE_OPT(cumulative, false)
        KV_SERIALIZE_OPT(binary, true)
        KV_SERIALIZE_OPT(compress, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct distribution
    {
      rpc::output_distribution_data data;
      uint64_t amount;
      std::string compressed_data;
      bool binary;
      bool compress;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount)
        KV_SERIALIZE_N(data.start_height, "start_height")
        KV_SERIALIZE(binary)
        KV_SERIALIZE(compress)
        if(this_ref.binary)
        {
          if(is_store)
          {
            if(this_ref.compress)
            {
              const_cast<std::string&>(this_ref.compressed_data) = compress_integer_array(this_ref.data.distribution);
              KV_SERIALIZE(compressed_data)
            }
            else
              KV_SERIALIZE_CONTAINER_POD_AS_BLOB_N(data.distribution, "distribution")
          }
          else
          {
            if(this_ref.compress)
            {
              KV_SERIALIZE(compressed_data)
              const_cast<std::vector<uint64_t>&>(this_ref.data.distribution) = decompress_integer_array<uint64_t>(this_ref.compressed_data);
            }
            else
              KV_SERIALIZE_CONTAINER_POD_AS_BLOB_N(data.distribution, "distribution")
          }
        }
        else
          KV_SERIALIZE_N(data.distribution, "distribution")
        KV_SERIALIZE_N(data.base, "base")
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::string status;
      std::vector<distribution> distributions;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(distributions)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_POP_BLOCKS
  {
    struct request_t
    {
      uint64_t nblocks;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(nblocks)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint64_t height;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_PRUNE_BLOCKCHAIN
  {
    struct request_t
    {
      bool check;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(check, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      bool pruned;
      uint32_t pruning_seed;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(pruned)
        KV_SERIALIZE(pruning_seed)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_QUORUM_STATE
  {
    static constexpr uint64_t HEIGHT_SENTINEL_VALUE = UINT64_MAX;
    static constexpr uint8_t ALL_QUORUMS_SENTINEL_VALUE = 255;
    struct request_t
    {
      uint64_t start_height;
      uint64_t end_height;
      uint8_t quorum_type;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(start_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(end_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(quorum_type, ALL_QUORUMS_SENTINEL_VALUE)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct quorum_t
    {
      std::vector<std::string> validators;
      std::vector<std::string> workers;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(validators)
        KV_SERIALIZE(workers)
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE()
        FIELD(validators)
        FIELD(workers)
      END_SERIALIZE()
    };

    struct quorum_for_height
    {
      uint64_t height;
      uint8_t quorum_type;
      quorum_t quorum;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(quorum_type)
        KV_SERIALIZE(quorum)
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE()
        FIELD(height)
        FIELD(quorum_type)
        FIELD(quorum)
      END_SERIALIZE()
    };

    struct response_t
    {
      std::string status;
      std::vector<quorum_for_height> quorums;
      bool untrusted;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(quorums)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW
  {
    struct request_t
    {
      std::vector<std::string> args;
      bool make_friendly;
      uint64_t staking_requirement;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(args)
        KV_SERIALIZE(make_friendly)
        KV_SERIALIZE(staking_requirement)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::string registration_cmd;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(registration_cmd)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD
  {
    struct contribution_t
    {
      std::string address;
      uint64_t amount;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(address)
        KV_SERIALIZE(amount)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      std::string operator_cut;
      std::vector<contribution_t> contributions;
      uint64_t staking_requirement;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(operator_cut)
        KV_SERIALIZE(contributions)
        KV_SERIALIZE(staking_requirement)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      std::string registration_cmd;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(registration_cmd)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_SERVICE_NODE_KEY
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string service_node_pubkey;
      std::string service_node_ed25519_pubkey;
      std::string service_node_x25519_pubkey;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_pubkey)
        KV_SERIALIZE(service_node_ed25519_pubkey)
        KV_SERIALIZE(service_node_x25519_pubkey)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string service_node_privkey;
      std::string service_node_ed25519_privkey;
      std::string service_node_x25519_privkey;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_privkey)
        KV_SERIALIZE(service_node_ed25519_privkey)
        KV_SERIALIZE(service_node_x25519_privkey)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST
  {
    struct request_t
    {
      uint64_t max_height;
      uint64_t seed;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(max_height)
        KV_SERIALIZE(seed)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      uint64_t res_height;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(res_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct service_node_contribution
  {
    std::string key_image;
    std::string key_image_pub_key;
    uint64_t amount;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(key_image)
      KV_SERIALIZE(key_image_pub_key)
      KV_SERIALIZE(amount)
    END_KV_SERIALIZE_MAP()
  };

  struct service_node_contributor
  {
    uint64_t amount;
    uint64_t reserved;
    std::string address;
    std::vector<service_node_contribution> locked_contributions;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(amount)
      KV_SERIALIZE(reserved)
      KV_SERIALIZE(address)
      KV_SERIALIZE(locked_contributions)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_GET_SERVICE_NODES
  {
    struct request_t
    {
      std::vector<std::string> service_node_pubkeys;
      bool include_json;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_pubkeys)
        KV_SERIALIZE(include_json);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      struct entry
      {
        std::string service_node_pubkey;
        uint64_t registration_height;
        uint64_t requested_unlock_height;
        uint64_t last_reward_block_height;
        uint32_t last_reward_transaction_index;
        bool active;
        bool funded;
        uint64_t state_height;
        uint32_t decommission_count;
        int64_t earned_downtime_blocks;
        std::array<uint16_t, 3> service_node_version;
        std::vector<service_node_contributor> contributors;
        uint64_t total_contributed;
        uint64_t total_reserved;
        uint64_t staking_requirement;
        uint64_t portions_for_operator;
        uint64_t swarm_id;
        std::string operator_address;
        std::string public_ip;
        uint16_t storage_port;
        uint16_t arqnet_port;
        std::string pubkey_ed25519;
        std::string pubkey_x25519;

        uint64_t last_uptime_proof;
        bool storage_server_reachable;
        uint64_t storage_server_reachable_timestamp;
        uint16_t version_major;
        uint16_t version_minor;
        uint16_t version_patch;
        std::vector<service_nodes::checkpoint_vote_record> votes;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(service_node_pubkey)
          KV_SERIALIZE(registration_height)
          KV_SERIALIZE(requested_unlock_height)
          KV_SERIALIZE(last_reward_block_height)
          KV_SERIALIZE(last_reward_transaction_index)
          KV_SERIALIZE(active)
          KV_SERIALIZE(funded)
          KV_SERIALIZE(state_height)
          KV_SERIALIZE(decommission_count)
          KV_SERIALIZE(earned_downtime_blocks)
          KV_SERIALIZE(service_node_version)
          KV_SERIALIZE(contributors)
          KV_SERIALIZE(total_contributed)
          KV_SERIALIZE(total_reserved)
          KV_SERIALIZE(staking_requirement)
          KV_SERIALIZE(portions_for_operator)
          KV_SERIALIZE(swarm_id)
          KV_SERIALIZE(operator_address)
          KV_SERIALIZE(public_ip)
          KV_SERIALIZE(storage_port)
          KV_SERIALIZE(arqnet_port)
          KV_SERIALIZE(pubkey_ed25519)
          KV_SERIALIZE(pubkey_x25519)
          KV_SERIALIZE(last_uptime_proof)
          KV_SERIALIZE(storage_server_reachable)
          KV_SERIALIZE(storage_server_reachable_timestamp)
          KV_SERIALIZE(version_major)
          KV_SERIALIZE(version_minor)
          KV_SERIALIZE(version_patch)
          KV_SERIALIZE(votes)
        END_KV_SERIALIZE_MAP()
      };

      std::vector<entry> service_node_states;
      uint64_t height;
      std::string block_hash;
      std::string status;
      std::string as_json;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_states)
        KV_SERIALIZE(height)
        KV_SERIALIZE(block_hash)
        KV_SERIALIZE(status)
        KV_SERIALIZE(as_json)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  #define KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(var) \
  if (this_ref.requested_fields.var || !this_ref.requested_fields.explicitly_set) KV_SERIALIZE(var)

  struct COMMAND_RPC_GET_N_SERVICE_NODES
  {
    // Boolean values indicat whether corresponding fields should be included in the response
    struct requested_fields_t
    {
      bool explicitly_set = false; // internal use only

      bool service_node_pubkey;
      bool registration_height;
      bool requested_unlock_height;
      bool last_reward_block_height;
      bool last_reward_transaction_index;
      bool active;
      bool funded;
      bool state_height;
      bool decommission_count;
      bool earned_downtime_blocks;
      bool service_node_version;
      bool contributors;
      bool total_contributed;
      bool total_reserved;
      bool staking_requirement;
      bool portions_for_operator;
      bool swarm_id;
      bool operator_address;
      bool public_ip;
      bool storage_port;
      bool arqnet_port;
      bool pubkey_ed25519;
      bool pubkey_x25519;
      bool last_uptime_proof;
      bool storage_server_reachable;
      bool storage_server_reachable_timestamp;
      bool version_major;
      bool version_minor;
      bool version_patch;
      bool votes;
      bool block_hash;
      bool height;
      bool target_height;
      bool hardfork;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT2(service_node_pubkey, false)
        KV_SERIALIZE_OPT2(registration_height, false)
        KV_SERIALIZE_OPT2(requested_unlock_height, false)
        KV_SERIALIZE_OPT2(last_reward_block_height, false)
        KV_SERIALIZE_OPT2(last_reward_transaction_index, false)
        KV_SERIALIZE_OPT2(active, false)
        KV_SERIALIZE_OPT2(funded, false)
        KV_SERIALIZE_OPT2(state_height, false)
        KV_SERIALIZE_OPT2(decommission_count, false)
        KV_SERIALIZE_OPT2(earned_downtime_blocks, false)
        KV_SERIALIZE_OPT2(service_node_version, false)
        KV_SERIALIZE_OPT2(contributors, false)
        KV_SERIALIZE_OPT2(total_contributed, false)
        KV_SERIALIZE_OPT2(total_reserved, false)
        KV_SERIALIZE_OPT2(staking_requirement, false)
        KV_SERIALIZE_OPT2(portions_for_operator, false)
        KV_SERIALIZE_OPT2(swarm_id, false)
        KV_SERIALIZE_OPT2(operator_address, false)
        KV_SERIALIZE_OPT2(public_ip, false)
        KV_SERIALIZE_OPT2(storage_port, false)
        KV_SERIALIZE_OPT2(arqnet_port, false)
        KV_SERIALIZE_OPT2(pubkey_ed25519, false)
        KV_SERIALIZE_OPT2(pubkey_x25519, false)
        KV_SERIALIZE_OPT2(last_uptime_proof, false)
        KV_SERIALIZE_OPT2(storage_server_reachable, false)
        KV_SERIALIZE_OPT2(storage_server_reachable_timestamp, false)
        KV_SERIALIZE_OPT2(version_major, false)
        KV_SERIALIZE_OPT2(version_minor, false)
        KV_SERIALIZE_OPT2(version_patch, false)
        KV_SERIALIZE_OPT2(votes, false)
        KV_SERIALIZE_OPT2(block_hash, false)
        KV_SERIALIZE_OPT2(height, false)
        KV_SERIALIZE_OPT2(target_height, false)
        KV_SERIALIZE_OPT2(hardfork, false)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      uint32_t limit;
      bool active_only;
      requested_fields_t fields;
      std::string poll_block_hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(limit)
        KV_SERIALIZE(active_only)
        KV_SERIALIZE(fields)
        KV_SERIALIZE(poll_block_hash)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      struct entry
      {
        const requested_fields_t& requested_fields;

        entry(const requested_fields_t& res) : requested_fields(res) {}

        std::string service_node_pubkey; // The public key of the Service Node.
        uint64_t registration_height; // The height at which the registration for the Service Node arrived on the blockchain.
        uint64_t requested_unlock_height; // The height at which contributions will be released and the Service Node expires. 0 if not requested yet.
        uint64_t last_reward_block_height; // The last height at which this Service Node received a reward.
        uint32_t last_reward_transaction_index; // When multiple Service Nodes register on the same height, the order the transaction arrive dictate the order you receive rewards.
        bool active; // True if fully funded and not currently decommissioned (and so `active && !funded` implicitly defines decommissioned)
        bool funded; // True if the required stakes have been submitted to activate this Service Node
        uint64_t state_height; // If active: the state at which registration was completed; if decommissioned: the decommissioning height; if awaiting: the last contribution (or registration) height
        uint32_t decommission_count; // The number of times ther Service Node has been decommisioned since registration.
        int64_t earned_downtime_blocks; // The number of blocks earned towards decommissioning, or the number of blocks remaining until deregistration if currently decommissioned
        std::array<uint16_t, 3> service_node_version; // The major, minor, patch version of the Service Node respectively.
        std::vector<service_node_contributor> contributors; // Array of contributors, contributing to this Service Node.
        uint64_t total_contributed; // The total amount of Loki in atomic units contributed to this Service Node.
        uint64_t total_reserved; // The total amount of Loki in atomic units reserved in this Service Node.
        uint64_t staking_requirement; // The staking requirement in atomic units that is required to be contributed to become a Service Node.
        uint64_t portions_for_operator; // The operator percentage cut to take from each reward expressed in portions, see cryptonote_config.h's STAKING_PORTIONS.
        uint64_t swarm_id; // The identifier of the Service Node's current swarm.
        std::string operator_address; // The wallet address of the operator to which the operator cut of the staking reward is sent to.
        std::string public_ip; // The public ip address of the service node
        uint16_t storage_port; // The port number associated with the storage server
        uint16_t arqnet_port;
        std::string pubkey_ed25519;
        std::string pubkey_x25519;
        uint64_t last_uptime_proof;
        bool storage_server_reachable;
        uint64_t storage_server_reachable_timestamp;
        uint16_t version_major;
        uint16_t version_minor;
        uint16_t version_patch;
        std::vector<service_nodes::checkpoint_vote_record> votes;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(service_node_pubkey)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(registration_height)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(requested_unlock_height)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(last_reward_block_height)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(last_reward_transaction_index)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(active)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(funded)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(state_height)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(decommission_count)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(earned_downtime_blocks)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(service_node_version)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(contributors)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(total_contributed)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(total_reserved)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(staking_requirement)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(portions_for_operator)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(swarm_id)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(operator_address)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(public_ip)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_port)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(arqnet_port)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(pubkey_ed25519);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(pubkey_x25519);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(last_uptime_proof)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_server_reachable)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_server_reachable_timestamp)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(version_major)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(version_minor)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(version_patch)
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(votes)
        END_KV_SERIALIZE_MAP()
      };

      requested_fields_t fields;
      bool polling_mode;

      std::vector<entry> service_node_states; // Array of service node registration information
      uint64_t height; // Current block height
      uint64_t target_height;
      std::string block_hash; // Current block hash
      bool unchanged;
      uint8_t hardfork;
      std::string status; // Generic RPC error code. "OK" is the success value.

      BEGIN_KV_SERIALIZE_MAP()
        if (!this_ref.unchanged) KV_SERIALIZE(service_node_states)
        KV_SERIALIZE(status)
        if (this_ref.fields.height) KV_SERIALIZE(height)
        if (this_ref.fields.target_height) KV_SERIALIZE(target_height)
        if (this_ref.fields.block_hash || (this_ref.polling_mode && !this_ref.unchanged)) KV_SERIALIZE(block_hash)
        if (this_ref.fields.hardfork) KV_SERIALIZE(hardfork)
        if (this_ref.polling_mode) KV_SERIALIZE(unchanged)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_STORAGE_SERVER_PING
  {
    struct request
    {
      int version_major;
      int version_minor;
      int version_patch;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version_major)
        KV_SERIALIZE(version_minor)
        KV_SERIALIZE(version_patch)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_ARQNET_PING
  {
    struct request
    {
      std::array<int, 3> version;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version);
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_GET_STAKING_REQUIREMENT
  {
    struct request_t
    {
      uint64_t height;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t staking_requirement;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(staking_requirement)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct entry
    {
      std::string key_image;
      uint64_t unlock_height;
      uint64_t amount;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(key_image)
        KV_SERIALIZE(unlock_height)
        KV_SERIALIZE(amount)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<entry> blacklist;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blacklist)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_OUTPUT_BLACKLIST
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<uint64_t> blacklist;
      std::string status;
      bool untrusted;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blacklist)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_CHECKPOINTS
  {
    constexpr static uint32_t NUM_CHECKPOINTS_TO_QUERY_BY_DEFAULT = 60;
    constexpr static uint64_t HEIGHT_SENTINEL_VALUE = (UINT64_MAX - 1);
    struct request_t
    {
      uint64_t start_height;
      uint64_t end_height;
      uint32_t count;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(start_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(end_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(count, NUM_CHECKPOINTS_TO_QUERY_BY_DEFAULT)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct voter_to_signature_serialized
    {
      uint16_t voter_index;
      std::string signature;

      voter_to_signature_serialized() = default;
      voter_to_signature_serialized(service_nodes::voter_to_signature const &entry) : voter_index(entry.voter_index), signature(epee::string_tools::pod_to_hex(entry.signature)) { }

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(voter_index)
        KV_SERIALIZE(signature)
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE()
        FIELD(voter_index)
        FIELD(signature)
      END_SERIALIZE()
    };

    struct checkpoint_serialized
    {
      uint8_t version;
      std::string type;
      uint64_t height;
      std::string block_hash;
      std::vector<voter_to_signature_serialized> signatures;
      uint64_t prev_height;

      checkpoint_serialized() = default;
      checkpoint_serialized(checkpoint_t const &checkpoint)
        : version(checkpoint.version)
        , type(checkpoint_t::type_to_string(checkpoint.type))
        , height(checkpoint.height)
        , block_hash(epee::string_tools::pod_to_hex(checkpoint.block_hash))
        , prev_height(checkpoint.prev_height)
      {
        signatures.reserve(checkpoint.signatures.size());
        for (service_nodes::voter_to_signature const &entry : checkpoint.signatures)
          signatures.push_back(entry);
      }

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version);
        KV_SERIALIZE(type);
        KV_SERIALIZE(height);
        KV_SERIALIZE(block_hash);
        KV_SERIALIZE(signatures);
        KV_SERIALIZE(prev_height);
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE()
        FIELD(version)
        FIELD(type)
        FIELD(height)
        FIELD(block_hash)
        FIELD(signatures)
        FIELD(prev_height)
      END_SERIALIZE()
    };

    struct response_t
    {
      std::vector<checkpoint_serialized> checkpoints;
      std::string status;
      bool untrusted;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(checkpoints)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_GET_SN_STATE_CHANGES
  {
    constexpr static uint32_t NUM_BLOCKS_TO_SCAN_BY_DEFAULT = 720;
    constexpr static uint64_t HEIGHT_SENTINEL_VALUE = (UINT64_MAX - 1);
    struct request_t
    {
      uint64_t start_height;
      uint64_t end_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_height)
        KV_SERIALIZE_OPT(end_height, HEIGHT_SENTINEL_VALUE)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      bool untrusted;

      uint32_t total_deregister;
      uint32_t total_ip_change_penalty;
      uint32_t total_decommission;
      uint32_t total_recommission;
      uint32_t total_unlock;
      uint64_t start_height;
      uint64_t end_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(total_deregister)
        KV_SERIALIZE(total_ip_change_penalty)
        KV_SERIALIZE(total_decommission)
        KV_SERIALIZE(total_recommission)
        KV_SERIALIZE(total_unlock)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(end_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  struct COMMAND_RPC_REPORT_PEER_SS_STATUS
  {
    struct request_t
    {
      std::string type;
      std::string pubkey;
      bool passed;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(type)
        KV_SERIALIZE(pubkey)
        KV_SERIALIZE(passed)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };
}