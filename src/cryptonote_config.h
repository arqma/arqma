// Copyright (c) 2018-2019, The Arqma Network
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

#include <string>
#include <boost/uuid/uuid.hpp>
#include <stdexcept>
#include <chrono>

#define CRYPTONOTE_DNS_TIMEOUT_MS                       20000

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V2           300*2
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3           100*3
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V4           CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V2            11
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V9            BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V2

// MONEY_SUPPLY - total number coins to be generated
#define MONEY_SUPPLY                                    ((uint64_t)50000000000000000)
#define EMISSION_SPEED_FACTOR_PER_MINUTE                (22)
#define FINAL_SUBSIDY_PER_MINUTE                        ((uint64_t)300000000)


#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2    60000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1    20000 //size of block (bytes) after which reward for block calculated using block size - before first fork
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5    300000 //size of block (bytes) after which reward for block calculated using block size - second change, from v5
#define CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE   100000 // size in blocks of the long term block weight median window
#define CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR 50
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600

#define FEE_PER_KB_OLD                                  ((uint64_t)10000000)
#define FEE_PER_KB                                      ((uint64_t)20000)
#define FEE_PER_BYTE                                    ((uint64_t)6)
#define HF_16_FEE                                       ((uint64_t)1000)
#define DYNAMIC_FEE_PER_KB_BASE_FEE                     ((uint64_t)20000)
#define DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD            ((uint64_t)10000000000)
#define DYNAMIC_FEE_PER_KB_BASE_FEE_V5                  ((uint64_t)20000 * (uint64_t)CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5)
#define DYNAMIC_FEE_PER_BYTE_BASE_FEE_V13               ((uint64_t)(DYNAMIC_FEE_PER_KB_BASE_FEE_V5) * 50 / 1000)
#define DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT        ((uint64_t)1500)
#define HF_16_DYNAMIC_REFERENCE_FEE                     ((uint64_t)13944)

#define ORPHANED_BLOCKS_MAX_COUNT                       100

#define DIFFICULTY_TARGET_V3                            DIFFICULTY_TARGET_V2
#define DIFFICULTY_TARGET_V2                            240  // seconds
#define DIFFICULTY_TARGET_V1                            120  // seconds - before first fork
#define DIFFICULTY_WINDOW                               720 // blocks
#define DIFFICULTY_WINDOW_V2                            30
#define DIFFICULTY_WINDOW_V3                            17
#define DIFFICULTY_LAG                                  15  // !!!
#define DIFFICULTY_CUT                                  60  // timestamps to cut after sorting

#define DIFFICULTY_TARGET_V10                           120
#define DIFFICULTY_BLOCKS_COUNT_V10                     (DIFFICULTY_WINDOW_V10 + 1)
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V10           11
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V10          360
#define DIFFICULTY_WINDOW_V10                           90

#define DIFFICULTY_TARGET_V11                           120
#define DIFFICULTY_BLOCKS_COUNT_V11                     (DIFFICULTY_WINDOW_V11 + 1)
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V11           11
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V11          360
#define DIFFICULTY_WINDOW_V11                           90

#define DIFFICULTY_TARGET_V16                           DIFFICULTY_TARGET_V11
#define DIFFICULTY_WINDOW_V16                           DIFFICULTY_WINDOW_V11
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V16           11
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V16          CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V11
#define DIFFICULTY_BLOCKS_COUNT_V16                     (DIFFICULTY_WINDOW_V16 + 1)

#define DIFFICULTY_BLOCKS_COUNT_V3                      (DIFFICULTY_WINDOW_V3 + 1)
#define DIFFICULTY_BLOCKS_COUNT_V2                      (DIFFICULTY_WINDOW_V2 + 1) // added to make N=N
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG

#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V1   DIFFICULTY_TARGET_V1 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2   DIFFICULTY_TARGET_V2 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1

#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET_V2 // just alias; used by tests

#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000   // by default, blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_MAX_COUNT                  2048   // must be a power of 2, greater than 128, equal to SEEDHASH_EPOCH_BLOCKS

#define CRYPTONOTE_MEMPOOL_TX_LIVETIME                  (86400*3) // seconds, three days
#define CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME   604800 // seconds, one week

#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_BLOCK_COUNT     1000
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_TX_COUNT        20000

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   32
#define P2P_DEFAULT_CONNECTIONS_COUNT_TEST              2

#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60         // secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     52428800   // 50MB maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       // 5 seconds
#define P2P_DEFAULT_SOCKS_CONNECT_TIMEOUT               60         // seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             5000       // 5 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  // 2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       // 5 seconds
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70
#define P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT            2
#define P2P_DEFAULT_SYNC_SEARCH_CONNECTIONS_COUNT       2

#define P2P_DEFAULT_LIMIT_RATE_UP                       4096       // kbps
#define P2P_DEFAULT_LIMIT_RATE_DOWN                     16384      // kbps

#define P2P_FAILED_ADDR_FORGET_SECONDS                  (24*60*60)    // 1 day
#define P2P_IP_BLOCKTIME                                (2*60*60*24) // 2 days
#define P2P_IP_FAILS_BEFORE_BLOCK                       5
#define P2P_IDLE_CONNECTION_KILL_INTERVAL               (30)     // 30 seconds

#define P2P_SUPPORT_FLAG_FLUFFY_BLOCKS                  0x01
#define P2P_SUPPORT_FLAGS                               P2P_SUPPORT_FLAG_FLUFFY_BLOCKS

#define CRYPTONOTE_NAME                                 "arqma"
#define CRYPTONOTE_POOLDATA_FILENAME                    "poolstate.bin"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME              "data.mdb"
#define CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME         "lock.mdb"
#define P2P_NET_DATA_FILENAME                           "p2pstate.bin"
#define MINER_CONFIG_FILE_NAME                          "miner_conf.json"

#define THREAD_STACK_SIZE                               10 * 1024 * 1024

#define HF_VERSION_DYNAMIC_FEE                          4
#define HF_VERSION_MIN_MIXIN_4                          6
#define HF_VERSION_MIN_MIXIN_6                          7
#define HF_VERSION_ENFORCE_RCT                          6

#define HF_VERSION_MIN_MIXIN_10                         13

#define HF_VERSION_LOWER_FEE                            10
#define HF_VERSION_PER_BYTE_FEE                         13
#define HF_FORBID_BORROMEAN                             13
#define HF_VERSION_LONG_TERM_BLOCK_WEIGHT               14

#define HF_VERSION_SERVICE_NODES                        16

#define PER_KB_FEE_QUANTIZATION_DECIMALS                8

#define HASH_OF_HASHES_STEP                             256

#define DEFAULT_TXPOOL_MAX_WEIGHT                       648000000ull // 3 days at 300000, in bytes

#define BULLETPROOF_MAX_OUTPUTS                         16

#define CRYPTONOTE_PRUNING_STRIPE_SIZE                  4096         // the smaller, the smoother the increase
#define CRYPTONOTE_PRUNING_LOG_STRIPES                  3            // the higher, the more space saved
#define CRYPTONOTE_PRUNING_TIP_BLOCKS                   5500         // the smaller, the more space saved
//#define CRYPTONOTE_PRUNING_DEBUG_SPOOF_SEED

static constexpr uint64_t POISSON_CHECK_TRIGGER = 5;  // Reorg size that triggers poisson timestamp check
static constexpr uint64_t POISSON_CHECK_DEPTH = 128;  // Main-chain depth of the poisson check. The attacker will have to tamper 50% of those blocks
static constexpr double POISSON_LOG_P_REJECT = -75.0; // Reject reorg if the probablity that the timestamps are genuine is below e^x, -75 = 10^-33

// New constants are intended to go here
#define STAKING_RELOCK_WINDOW_BLOCKS                    180
#define STAKING_REQUIREMENT_LOCK_BLOCKS_EXCESS          20
#define STAKING_SHARE_PARTS                             UINT64_C(0xfffffffffffffffc) // Use a multiple of four, so that it divides easily by max number of contributors.
#define MAX_NUMBER_OF_CONTRIBUTORS                      4
#define MIN_STAKE_SHARE                                 (STAKING_SHARE_PARTS / MAX_NUMBER_OF_CONTRIBUTORS)

#define STAKING_AUTHORIZATION_EXPIRATION_WINDOW         (86400*14) // (seconds_per_day times days)

#define MEMPOOL_PRUNE_NON_STANDARD_TX_LIFETIME          (7200) // value in seconds
#define BLOCKS_EXPECTED_IN_HOURS(val)                   ((3600 / DIFFICULTY_TARGET_V16) * (val))
#define BLOCKS_EXPECTED_IN_DAYS(val)                    (BLOCKS_EXPECTED_IN_HOURS(24) * (val))
#define BLOCKS_EXPECTED_IN_YEARS(val)                   (BLOCKS_EXPECTED_IN_DAYS(365) * (val))

// testing constants
// TODO: To be removed after successful tests.


static_assert(STAKING_SHARE_PARTS % MAX_NUMBER_OF_CONTRIBUTORS == 0, "Use a multiple of four, so that it divides easily by max number of contributors.");
static_assert(STAKING_SHARE_PARTS % 2 == 0, "Use a multiple of two, so that it divides easily by two contributors.");
static_assert(STAKING_SHARE_PARTS % 3 == 0, "Use a multiple of three, so that it divides easily by three contributors.");

#define UPTIME_PROOF_BUFFER_IN_SECONDS                  (300)
#define UPTIME_PROOF_FREQUENCY_IN_SECONDS               (3600)
#define UPTIME_PROOF_MAX_TIME_IN_SECONDS                (UPTIME_PROOF_FREQUENCY_IN_SECONDS * 2 + UPTIME_PROOF_BUFFER_IN_SECONDS)

namespace config
{
  const uint64_t DEFAULT_FEE_ATOMIC_XMR_PER_KB = 500; // Just a placeholder! Change me!
  const uint64_t DEFAULT_DUST_THRESHOLD = ((uint64_t)20000); // Deprecated
  const uint64_t BASE_REWARD_CLAMP_THRESHOLD = ((uint64_t)100000);

  const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x2cca; // Wallet prefix: ar... // decimal prefix: 11466
  const uint64_t CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x116bc7; // Wallet prefix: aRi... // decimal prefix: 1141703
  const uint64_t CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x6847; // Wallet prefix: aRS... // decimal prefix: 26695
  const uint16_t P2P_DEFAULT_PORT = 19993;
  const uint16_t RPC_DEFAULT_PORT = 19994;
  const uint16_t ZMQ_DEFAULT_PORT = 19995;
  boost::uuids::uuid const NETWORK_ID = { {
      0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1A
    } }; // Bender's nightmare
  std::string const GENESIS_TX = "011201ff00011e026bc5c7db8a664f652d78adb587ac4d759c6757258b64ef9cba3c0354e64fb2e42101abca6a39c561d0897be183eb0143990eba201aa7d2c652ab0555d28bb4b70728";
  const uint32_t GENESIS_NONCE = 19993;
  const uint64_t REWARD_INTERVAL_IN_BLOCKS = ((60 * 60 * 24 * 2) / DIFFICULTY_TARGET_V16);
  std::string const GOVERNANCE_WALLET_ADDRESS = "ar2govGzKKncQTPTNEre3BGVGF4faUgNh5EiycjidUXMfwoMeHZSXvTay2AwURXzQDNvh3Hd2Vyn2iXctEZE5CncCdJpphqB";
  std::string const DEV_WALLET_ADDRESS = "";

  namespace testnet
  {
    const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x53ca; // Wallet prefix: at... // decimal prefix: 21450
    const uint64_t CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x504a; // Wallet prefix: ati... // decimal prefix: 20554
    const uint64_t CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x524a; // Wallet prefix: ats... // decimal prefix: 21066
    const uint16_t P2P_DEFAULT_PORT = 29993;
    const uint16_t RPC_DEFAULT_PORT = 29994;
    const uint16_t ZMQ_DEFAULT_PORT = 29995;
    boost::uuids::uuid const NETWORK_ID = { {
        0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1B
      } }; // Bender's daydream
    const uint64_t REWARD_INTERVAL_IN_BLOCKS = 50;
    std::string const GOVERNANCE_WALLET_ADDRESS = "atywxUgKyRajPKSRHWDtjyPTaE9thXgyrTpk9RsdkYzCdPYSxtTkW8Jj6xgrNHXLEd22yb7Gdk39DoJpgrLKTe85NP22nsWGY";
    std::string const DEV_WALLET_ADDRESS = "";
  }

   namespace stagenet
   {
     const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x39ca; // Wallet prefix: as... // decimal prefix: 14794
     const uint64_t CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x1742ca; // Wallet prefix: asi... // decimal prefix: 1524426
     const uint64_t CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x1d84ca; // Wallet prefix: ass... // decimal prefix: 1934538
     const uint16_t P2P_DEFAULT_PORT = 39993;
     const uint16_t RPC_DEFAULT_PORT = 39994;
     const uint16_t ZMQ_DEFAULT_PORT = 39995;
     boost::uuids::uuid const NETWORK_ID = { {
         0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1C
       } }; // Bender's daydream
     const uint64_t REWARD_INTERVAL_IN_BLOCKS = 50;
     std::string const GOVERNANCE_WALLET_ADDRESS = "as2H2hydWETBhUdFiRD5qZHw3QsVdirAsAc3P4HQSSWx6KMv3PQZPtkU3VeKtJaVe9a7SP1pqxb8rCdh2WJtVK7B2RMzQBFRJ";
     std::string const DEV_WALLET_ADDRESS = "as2V1HH36fNQXrzb3aSwcbEfmQHAoWgBabhSrfimiGNV7gVjJaSvbed7XndTpNe5yHczdwVWF2FtCg2Yd7z3pker2VCsS5Gzq";
   }

   namespace blockchain_settings
   {
     static constexpr uint64_t ARQMA = 1000000000; // Atomic representation of a value 1 ARQMA
     static const uint8_t ARQMA_DECIMALS = 9;
     const uint64_t PREMINE = 7500000000000000; // Premine Reward (already burned).
     const uint64_t PREMINE_BURN = 5100000000000000; // Will need to be set after knowing exact amount.
     const uint64_t MAXIMUM_BLOCK_SIZE_LIMIT = 2097152; // It is set to 2048kB (2MB)
     const uint64_t MINIMUM_BLOCK_SIZE_LIMIT = 1048576; // It is set to 1024kB (1MB)
     static const uint8_t ARQMA_GENESIS_BLOCK_MAJOR_VERSION = 1;
     static const uint8_t ARQMA_GENESIS_BLOCK_MINOR_VERSION = 1;
     static const uint8_t ARQMA_BLOCK_UNLOCK_CONFIRMATIONS = 18; // How many blocks mined are needed to unlock block_reward.
     static const uint8_t min_output_age = 20;
   }

   namespace tx_settings
   {
     static const size_t ARQMA_TX_CONFIRMATIONS_REQUIRED = 4; // How many blocks are needed to confirm transaction sent.
     const uint64_t ARQMA_TX_LOCK_SECONDS = 360;  // Transaction lock stated in seconds related to time-based per_output_unlock.
     const uint64_t TRANSACTION_SIZE_LIMIT = 49152; // I did set it to 48kB for now but it need to be verified.
     const uint64_t MAX_TRANSACTIONS_IN_BLOCK = 1024; // Maximum allowed transactions in One Block

     static const uint64_t min_tx_amount = 10000;
     static const uint64_t min_amount_blockage_fee = (MONEY_SUPPLY - 1);
     static constexpr uint64_t tx_mixin = 10;
     static constexpr uint64_t tx_ring_size = tx_mixin + 1;
   }

   namespace sync
   {
     static constexpr size_t NORMAL_SYNC = 20;
     static constexpr size_t FAST_SYNC = 100;
     static constexpr size_t MAX_SYNC = 2048;
   }

   namespace governance
   {
     static const uint64_t gr = 15;
     static const uint64_t sn = 2;
   }
}

namespace arqma
{
  static constexpr size_t seed_nodes_qty = 12;
  const char *const mainnet_core_nodes[] =
  {
    "144.217.242.16",
    "161.97.102.172",
    "it-support.mal-bit.com",
    "207.244.249.105",
    "139.99.106.122",
    "164.68.123.118"
  };

  const char *const testnet_core_nodes[] =
  {
    "161.97.102.172",
    "139.99.106.122",
    "77.93.206.172",
    "it-support.mal-bit.com"
  };

  const char *const stagenet_core_nodes[] =
  {
    "161.97.102.172",
    "139.99.106.122",
    "77.93.206.172",
    "it-support.mal-bit.com",
    "164.68.123.118",
    "144.217.242.16"
  };
}

namespace cryptonote
{
  enum network_version
  {
    network_version_1 = 1,
    network_version_7 = 7,
    network_version_8,
    network_version_9,
    network_version_10,
    network_version_11,
    network_version_12,
    network_version_13,
    network_version_14,
    network_version_15,
    network_version_16
  };

  enum network_type : uint8_t
    {
      MAINNET = 0,
      TESTNET,
      STAGENET,
      FAKECHAIN,
      UNDEFINED = 255
    };
    struct config_t
    {
      uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
      uint64_t CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
      uint64_t CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
      uint16_t P2P_DEFAULT_PORT;
      uint16_t RPC_DEFAULT_PORT;
      uint16_t ZMQ_DEFAULT_PORT;
      boost::uuids::uuid NETWORK_ID;
      std::string GENESIS_TX;
      uint32_t GENESIS_NONCE;
      uint64_t REWARD_INTERVAL_IN_BLOCKS;
      std::string const *GOVERNANCE_WALLET_ADDRESS;
      std::string const *DEV_WALLET_ADDRESS;
    };
    inline const config_t& get_config(network_type nettype, uint8_t hard_fork_version = 7)
    {
      static config_t mainnet = {
        ::config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        ::config::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
        ::config::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
        ::config::P2P_DEFAULT_PORT,
        ::config::RPC_DEFAULT_PORT,
        ::config::ZMQ_DEFAULT_PORT,
        ::config::NETWORK_ID,
        ::config::GENESIS_TX,
        ::config::GENESIS_NONCE,
        ::config::REWARD_INTERVAL_IN_BLOCKS,
        &::config::GOVERNANCE_WALLET_ADDRESS,
        &::config::DEV_WALLET_ADDRESS
      };

      static config_t testnet = {
        ::config::testnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        ::config::testnet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
        ::config::testnet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
        ::config::testnet::P2P_DEFAULT_PORT,
        ::config::testnet::RPC_DEFAULT_PORT,
        ::config::testnet::ZMQ_DEFAULT_PORT,
        ::config::testnet::NETWORK_ID,
        ::config::GENESIS_TX,
        ::config::GENESIS_NONCE,
        ::config::testnet::REWARD_INTERVAL_IN_BLOCKS,
        &::config::testnet::GOVERNANCE_WALLET_ADDRESS,
        &::config::testnet::DEV_WALLET_ADDRESS
      };

      static config_t stagenet = {
        ::config::stagenet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        ::config::stagenet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
        ::config::stagenet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
        ::config::stagenet::P2P_DEFAULT_PORT,
        ::config::stagenet::RPC_DEFAULT_PORT,
        ::config::stagenet::ZMQ_DEFAULT_PORT,
        ::config::stagenet::NETWORK_ID,
        ::config::GENESIS_TX,
        ::config::GENESIS_NONCE,
        ::config::stagenet::REWARD_INTERVAL_IN_BLOCKS,
        &::config::stagenet::GOVERNANCE_WALLET_ADDRESS,
        &::config::stagenet::DEV_WALLET_ADDRESS
      };

      switch (nettype)
      {
        case MAINNET:
        case FAKECHAIN:
        {
          if(nettype == FAKECHAIN)
            mainnet.REWARD_INTERVAL_IN_BLOCKS = 50;

          return mainnet;
        }
        case TESTNET:
        {
          return testnet;
        }

        case STAGENET:
        {
          return stagenet;
        }

        default: throw std::runtime_error("Invalid network type");
      }
   };
}
