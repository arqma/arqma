
#pragma once
#include "common.h"

using namespace cryptonote;

namespace config
{
uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x2cca; // Wallet prefix: ar...
uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x116bc7; // Wallet prefix: aRi..
uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x6847; // Wallet prefix: aRS..
uint16_t const P2P_DEFAULT_PORT = 19993;
uint16_t const RPC_DEFAULT_PORT = 19994;
uint16_t const ZMQ_RPC_DEFAULT_PORT = 19995;
boost::uuids::uuid const NETWORK_ID = {{0x11, 0x11, 0x11, 0x11 , 0xFF, 0xFF , 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1A}}; // Bender's nightmare
std::string const GENESIS_TX = "011201ff00011e026bc5c7db8a664f652d78adb587ac4d759c6757258b64ef9cba3c0354e64fb2e42101abca6a39c561d0897be183eb0143990eba201aa7d2c652ab0555d28bb4b70728";
uint32_t const GENESIS_NONCE = 19993;

namespace mainnet
{
static const config_t data = {
    CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
    CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
    CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
    P2P_DEFAULT_PORT,
    RPC_DEFAULT_PORT,
    ZMQ_RPC_DEFAULT_PORT,
    NETWORK_ID,
    GENESIS_TX,
    GENESIS_NONCE
  };

static const hard_fork_t hard_forks[] = {
  // version 1 from the start of the blockchain
  { 1, 0, 0, 1341378000 },
  { 7, 1, 0, 1528750800 },
  { 8, 100, 0, 1528751200 },
  { 9, 7000, 0, 1530320400 },
  { 10, 61250, 0, 1543615200 },
};
static const uint64_t hard_fork_version_1_till = 1;

} // namespace mainnet
} // namespace config
