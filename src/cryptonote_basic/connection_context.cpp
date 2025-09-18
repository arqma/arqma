#include "connection_context.h"

#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "p2p/p2p_protocol_defs.h"

namespace cryptonote
{
  std::size_t cryptonote_connection_context::get_max_bytes(const int command) noexcept
  {
    switch (command)
    {
      case nodetool::COMMAND_HANDSHAKE_T<cryptonote::CORE_SYNC_DATA>::ID:
      case nodetool::COMMAND_TIMED_SYNC_T<cryptonote::CORE_SYNC_DATA>::ID:
        return 65536;
      case nodetool::COMMAND_PING::ID:
      case nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::ID:
        return 4096;
      case cryptonote::NOTIFY_NEW_TRANSACTIONS::ID:
      case cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::ID:
      case cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::ID:
        return 1024 * 1024 * 128; // 128 MB (max packet is a bit less than 100 MB though)
      case cryptonote::NOTIFY_REQUEST_CHAIN::ID:
        return 512 * 1024; // 512 kB
      case cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::ID:
        return 1024 * 1024 * 4; // 4 MB
      case cryptonote::NOTIFY_UPTIME_PROOF::ID:
      case cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::ID:
        return 1024 * 1024; // 1 MB
      case cryptonote::NOTIFY_REQUEST_GET_OBJECTS::ID:
      case cryptonote::NOTIFY_NEW_SERVICE_NODE_VOTE::ID:
        return 1024 * 1024 * 2; // 2 MB
      default:
        break;
    };
    return std::numeric_limits<size_t>::max();
  }
}
