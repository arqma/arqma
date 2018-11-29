#pragma once
#include "common.h"

using namespace cryptonote;

namespace config
{
namespace testnet
{
uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x53ca; // Wallet prefix: at...
uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x504a; // Wallet prefix: ati..
uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x524a; // Wallet prefix: ats..
uint16_t const P2P_DEFAULT_PORT = 29993;
uint16_t const RPC_DEFAULT_PORT = 29994;
uint16_t const ZMQ_RPC_DEFAULT_PORT = 29995;
boost::uuids::uuid const NETWORK_ID = {{0x11 ,0x11, 0x11, 0x11 , 0xFF, 0xFF , 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1B}};

static const config_t data = {
    CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
    CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
    CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
    P2P_DEFAULT_PORT,
    RPC_DEFAULT_PORT,
    ZMQ_RPC_DEFAULT_PORT,
    NETWORK_ID
  };

static const hard_fork_t hard_forks[] = {
    { 1, 0, 0, 1341378000 },
    { 7, 1, 0, 1528750800 },
    { 8, 10, 0, 1528751200 },
    { 9, 20, 0, 1530248400 },
    { 10, 1000, 0, 1538352000 },
  };

static const uint64_t hard_fork_version_1_till = 1;

} // namespace testnet
} // namespace config
