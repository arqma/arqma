#pragma once
#include "common.h"

using namespace cryptonote;

namespace config
{
namespace stagenet
{
uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x39ca;  // Wallet prefix: as..
uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x1742ca; // Wallet prefix: asi..
uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x1d84ca; // Wallet prefix: ass..
uint16_t const P2P_DEFAULT_PORT = 39993;
uint16_t const RPC_DEFAULT_PORT = 39994;
uint16_t const ZMQ_RPC_DEFAULT_PORT = 39995;
boost::uuids::uuid const NETWORK_ID = {{0x11 ,0x11, 0x11, 0x11 , 0xFF, 0xFF , 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1C}}; // Bender's daydream

static const config_t data = {
    CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
    CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
    CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
    P2P_DEFAULT_PORT,
    RPC_DEFAULT_PORT,
    ZMQ_RPC_DEFAULT_PORT,
    NETWORK_ID
  };

static const struct hard_fork_t hard_forks[] = {
    // version 1 from the start of the blockchain
    { 1, 0, 0, 1341378000 },
    { 7, 1, 0, 1528750800 },
    { 8, 20, 0, 1528751200 },
    { 9, 40, 0, 1530248400 },
    { 10, 100, 0, 1538352000 },
//    { 11, 2500, 0, 1541787000 },
  };
static const uint64_t hard_fork_version_1_till = 1;

} // namespace stagenet
} // namespace config
