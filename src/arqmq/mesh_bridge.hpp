// Copyright (c) 2018 - 2026, The Arqma Network
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

#pragma once

#include "curve_zap.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace arqmq {
class SocketStack;

/// Shadow CURVE listen port = live arqnet port + this offset (avoids P2P/RPC/ZMQ/ANET).
inline constexpr int k_mesh_shadow_port_offset = 10000;

/// Attaches dual-run command mirrors on the dedicated SocketStack without
/// replacing SNNetwork wire handlers. No-op when native transport is inactive.
void attach_compatible_mesh_mirrors(SocketStack& stack);

/// Convenience: attach mirrors when `active_socket_stack()` is running.
void attach_compatible_mesh_mirrors_if_active();

/// Loads CURVE identity + ZAP allow onto the active SocketStack for shadow mesh
/// work. Does not bind a listener (SNNetwork keeps the live arqnet port).
void configure_mesh_shadow(SocketStack& stack, std::string public_key, std::string secret_key, AllowConnection allow);

/// Bind SocketStack CURVE on `live_bind` with `k_mesh_shadow_port_offset`.
/// Outbound shadow sends rewrite peer hints by the same offset.
std::error_code start_mesh_shadow_listener(SocketStack& stack, std::string_view live_arqnet_bind);

/// Rewrite `tcp://host:port` by adding `port_offset`. Empty on parse failure.
std::string endpoint_with_port_offset(std::string_view endpoint, int port_offset);

/// Opt-in dual-write of peer commands onto SocketStack (default off).
bool native_mesh_shadow_relay_enabled() noexcept;
void set_native_mesh_shadow_relay_enabled(bool enabled) noexcept;

/// Actual bound shadow CURVE endpoint (may differ from request if port was 0).
std::string native_mesh_shadow_endpoint();

struct MeshShadowStats
{
  uint64_t attempts = 0;
  uint64_t ok = 0;
  uint64_t fail = 0;
  /// SNNetwork live peer relays observed while dual-run is active.
  uint64_t live_relays = 0;
  uint64_t vote_ob_live = 0;
  uint64_t vote_ob_shadow_ok = 0;
  uint64_t vote_ob_shadow_fail = 0;
  uint64_t vote_ob_shadow_in = 0;
  /// Inbound shadow vote_ob payloads that parse as obligation-vote wire.
  uint64_t vote_ob_shadow_parse_ok = 0;
  uint64_t vote_ob_shadow_parse_fail = 0;
  uint64_t pulse_rnd_live = 0;
  uint64_t pulse_rnd_shadow_ok = 0;
  uint64_t pulse_rnd_shadow_fail = 0;
  uint64_t pulse_rnd_shadow_in = 0;
  uint64_t pulse_rnd_shadow_parse_ok = 0;
  uint64_t pulse_rnd_shadow_parse_fail = 0;
  uint64_t blink_tx_live = 0;
  uint64_t blink_tx_shadow_ok = 0;
  uint64_t blink_tx_shadow_fail = 0;
  uint64_t blink_tx_shadow_in = 0;
  uint64_t blink_tx_shadow_parse_ok = 0;
  uint64_t blink_tx_shadow_parse_fail = 0;
};

/// Cumulative shadow/live counters (reset when shadow is toggled on).
MeshShadowStats native_mesh_shadow_stats() noexcept;

/// Record one live SNNetwork peer relay (call alongside snn.send).
void note_live_mesh_relay(std::string_view command) noexcept;

/// Count an inbound SocketStack `vote_ob` / `pulse_rnd` / `blink_tx` (shadow listener or native handler).
void note_inbound_mesh_shadow(std::string_view command, std::string_view payload) noexcept;

/// Shadow ok-rate in basis points (0..10000). Returns 0 when attempts==0.
uint32_t native_mesh_shadow_ok_rate_bps() noexcept;

/// True when soak sample is large enough, outbound vote_ob shadow ok-rate meets
/// floor, **and** inbound payloads parse as obligation-vote wire at the same
/// floor. Pulse `pulse_rnd` counters are observability only (not part of this
/// gate — Pulse gossip starts at HF20). Does not flip cutover by itself.
bool native_mesh_shadow_parity_sample_ok(uint64_t min_vote_ob_live = 32, uint32_t min_ok_rate_bps = 9500) noexcept;

/// Optional daemon hook: same decoder the native inbound path will use at stage 4.
/// nullptr restores the structural bencode shape check.
using VoteObPayloadValidator = bool (*)(std::string_view payload);
void set_vote_ob_payload_validator(VoteObPayloadValidator validator) noexcept;

/// True when `payload` is a well-formed obligation `vote_ob` wire body.
bool vote_ob_wire_payload_ok(std::string_view payload) noexcept;

using PulseRndPayloadValidator = bool (*)(std::string_view payload);
void set_pulse_rnd_payload_validator(PulseRndPayloadValidator validator) noexcept;

/// True when `payload` is a well-formed Pulse `pulse_rnd` packed vote.
bool pulse_rnd_wire_payload_ok(std::string_view payload) noexcept;

using BlinkTxPayloadValidator = bool (*)(std::string_view payload);
void set_blink_tx_payload_validator(BlinkTxPayloadValidator validator) noexcept;

/// True when `payload` is a well-formed Blink `blink_tx` packed vote.
bool blink_tx_wire_payload_ok(std::string_view payload) noexcept;

/// Best-effort shadow send; never throws. No-op unless shadow relay is enabled
/// and the active stack has CURVE identity configured.
void shadow_send_to_peer(std::string_view pubkey, std::string_view command, std::string_view payload,
                         std::string_view hint = {});

/// Primary native mesh send after cutover (`native_mesh_ready()`). Rewrites
/// peer hints by `k_mesh_shadow_port_offset`. No-op unless stage ≥4 and the
/// active stack has CURVE identity. Live relay still requires `native_mesh_live_at`.
void primary_mesh_send_to_peer(std::string_view pubkey, std::string_view command, std::string_view payload,
                               std::string_view hint = {});
} // namespace arqmq
