#include "connection_context.h"

#include <boost/optional/optional.hpp>
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "p2p/p2p_protocol_defs.h"

namespace cryptonote
{
  std::size_t cryptonote_connection_context::get_max_bytes(const int command) noexcept
  {
    switch (command)
    {
      case nodetool::COMMAND_HANDSHAKE_T<cryptonote::CORE_SYNC_DATA>::ID:
        return 65536;
      case nodetool::COMMAND_TIMED_SYNC_T<cryptonote::CORE_SYNC_DATA>::ID:
        return 65536;
      case nodetool::COMMAND_PING::ID:
        return 4096;
      case nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::ID:
        return 4096;
      case cryptonote::NOTIFY_NEW_BLOCK::ID:
        return 1024 * 1024 * 128; // 128 MB (max packet is a bit less than 100 MB though)
      case cryptonote::NOTIFY_NEW_TRANSACTIONS::ID:
        return 1024 * 1024 * 128; // 128 MB (max packet is a bit less than 100 MB though)
      case cryptonote::NOTIFY_REQUEST_GET_OBJECTS::ID:
        return 1024 * 1024 * 2; // 2 MB
      case cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::ID:
        return 1024 * 1024 * 128; // 128 MB (max packet is a bit less than 100 MB though)
      case cryptonote::NOTIFY_REQUEST_CHAIN::ID:
        return 512 * 1024; // 512 kB
      case cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::ID:
        return 1024 * 1024 * 4; // 4 MB
      case cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::ID:
        return 1024 * 1024 * 4; // 4 MB, but it does not includes transaction data
      case cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::ID:
        return 1024 * 1024; // 1 MB
      case cryptonote::NOTIFY_UPTIME_PROOF::ID:
        return 1024 * 1024 * 10;
      case cryptonote::NOTIFY_NEW_SERVICE_NODE_VOTE::ID:
        return 1024 * 1024 * 10;
      default:
        break;
    };
    return std::numeric_limits<size_t>::max();
  }

  void cryptonote_connection_context::set_state_normal()
  {
    m_state = state_normal;
    m_expected_heights_start = 0;
    m_needed_objects.clear();
    m_needed_objects.shrink_to_fit();
    m_expected_heights.clear();
    m_expected_heights.shrink_to_fit();
    m_requested_objects.clear();
  }

  boost::optional<crypto::hash> cryptonote_connection_context::get_expected_hash(const uint64_t height) const
  {
    const auto difference = height - m_expected_heights_start;
    if (height < m_expected_heights_start || m_expected_heights.size() < difference)
      return boost::none;
    return m_expected_heights[difference];
  }
}

