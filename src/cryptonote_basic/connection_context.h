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
#include <unordered_set>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <optional>
#include <boost/optional/optional_fwd.hpp>
#include "net/net_utils_base.h"
#include "copyable_atomic.h"
#include "crypto/hash.h"

namespace cryptonote
{

  struct cryptonote_connection_context: public epee::net_utils::connection_context_base
  {
    enum state
    {
      state_before_handshake = 0, //default state
      state_synchronizing,
      state_standby,
      state_idle,
      state_normal
    };

    static constexpr int handshake_command() noexcept { return 1001; }
    bool handshake_complete() const noexcept { return m_state != state_before_handshake; }

    static size_t get_max_bytes(int command) noexcept;

    void set_state_normal();

    boost::optional<crypto::hash> get_expected_hash(uint64_t height) const;

    state m_state{state_before_handshake};
    std::vector<crypto::hash> m_needed_objects;
    std::vector<crypto::hash> m_expected_heights;
    std::unordered_set<crypto::hash> m_requested_objects;
    uint64_t m_remote_blockchain_height{0};
    uint64_t m_last_response_height{0};
    uint64_t m_expected_heights_start{0};
    std::optional<std::chrono::steady_clock::time_point> m_last_request_time;
    epee::copyable_atomic m_callback_request_count{0}; //in debug purpose: problem with double callback rise
    crypto::hash m_last_known_hash{crypto::null_hash};
    uint32_t m_pruning_seed{0};
    uint16_t m_rpc_port{0};
    bool m_anchor{false};
    uint64_t m_expect_height{0};
    //size_t m_score;  TODO: add score calculations
  };

  inline std::string get_protocol_state_string(cryptonote_connection_context::state s)
  {
    switch (s)
    {
    case cryptonote_connection_context::state_before_handshake:
      return "before_handshake";
    case cryptonote_connection_context::state_synchronizing:
      return "synchronizing";
    case cryptonote_connection_context::state_standby:
      return "standby";
    case cryptonote_connection_context::state_idle:
      return "idle";
    case cryptonote_connection_context::state_normal:
      return "normal";
    default:
      return "unknown";
    }
  }

  inline char get_protocol_state_char(cryptonote_connection_context::state s)
  {
    switch (s)
    {
    case cryptonote_connection_context::state_before_handshake:
      return 'h';
    case cryptonote_connection_context::state_synchronizing:
      return 's';
    case cryptonote_connection_context::state_standby:
      return 'w';
    case cryptonote_connection_context::state_idle:
      return 'i';
    case cryptonote_connection_context::state_normal:
      return 'n';
    default:
      return 'u';
    }
  }

}
