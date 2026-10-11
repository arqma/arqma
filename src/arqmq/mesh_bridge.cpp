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

#include "mesh_bridge.hpp"

#include "arqmq.h"
#include "socket_stack.hpp"
#include "transport.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace arqmq {
namespace {
std::atomic<bool> g_shadow_relay_enabled{false};
std::atomic<uint64_t> g_shadow_attempts{0};
std::atomic<uint64_t> g_shadow_ok{0};
std::atomic<uint64_t> g_shadow_fail{0};
std::atomic<uint64_t> g_live_relays{0};
std::atomic<uint64_t> g_vote_ob_live{0};
std::atomic<uint64_t> g_vote_ob_shadow_ok{0};
std::atomic<uint64_t> g_vote_ob_shadow_fail{0};
std::atomic<uint64_t> g_vote_ob_shadow_in{0};
std::atomic<uint64_t> g_vote_ob_shadow_parse_ok{0};
std::atomic<uint64_t> g_vote_ob_shadow_parse_fail{0};
std::atomic<VoteObPayloadValidator> g_vote_ob_validator{nullptr};
std::atomic<uint64_t> g_pulse_rnd_live{0};
std::atomic<uint64_t> g_pulse_rnd_shadow_ok{0};
std::atomic<uint64_t> g_pulse_rnd_shadow_fail{0};
std::atomic<uint64_t> g_pulse_rnd_shadow_in{0};
std::atomic<uint64_t> g_pulse_rnd_shadow_parse_ok{0};
std::atomic<uint64_t> g_pulse_rnd_shadow_parse_fail{0};
std::atomic<PulseRndPayloadValidator> g_pulse_rnd_validator{nullptr};
std::atomic<uint64_t> g_blink_tx_live{0};
std::atomic<uint64_t> g_blink_tx_shadow_ok{0};
std::atomic<uint64_t> g_blink_tx_shadow_fail{0};
std::atomic<uint64_t> g_blink_tx_shadow_in{0};
std::atomic<uint64_t> g_blink_tx_shadow_parse_ok{0};
std::atomic<uint64_t> g_blink_tx_shadow_parse_fail{0};
std::atomic<BlinkTxPayloadValidator> g_blink_tx_validator{nullptr};
std::atomic<int> g_shadow_port_offset{0};
std::mutex g_shadow_endpoint_mu;
std::string g_shadow_endpoint;

void reset_shadow_stats() noexcept
{
  g_shadow_attempts.store(0, std::memory_order_relaxed);
  g_shadow_ok.store(0, std::memory_order_relaxed);
  g_shadow_fail.store(0, std::memory_order_relaxed);
  g_live_relays.store(0, std::memory_order_relaxed);
  g_vote_ob_live.store(0, std::memory_order_relaxed);
  g_vote_ob_shadow_ok.store(0, std::memory_order_relaxed);
  g_vote_ob_shadow_fail.store(0, std::memory_order_relaxed);
  g_vote_ob_shadow_in.store(0, std::memory_order_relaxed);
  g_vote_ob_shadow_parse_ok.store(0, std::memory_order_relaxed);
  g_vote_ob_shadow_parse_fail.store(0, std::memory_order_relaxed);
  g_pulse_rnd_live.store(0, std::memory_order_relaxed);
  g_pulse_rnd_shadow_ok.store(0, std::memory_order_relaxed);
  g_pulse_rnd_shadow_fail.store(0, std::memory_order_relaxed);
  g_pulse_rnd_shadow_in.store(0, std::memory_order_relaxed);
  g_pulse_rnd_shadow_parse_ok.store(0, std::memory_order_relaxed);
  g_pulse_rnd_shadow_parse_fail.store(0, std::memory_order_relaxed);
  g_blink_tx_live.store(0, std::memory_order_relaxed);
  g_blink_tx_shadow_ok.store(0, std::memory_order_relaxed);
  g_blink_tx_shadow_fail.store(0, std::memory_order_relaxed);
  g_blink_tx_shadow_in.store(0, std::memory_order_relaxed);
  g_blink_tx_shadow_parse_ok.store(0, std::memory_order_relaxed);
  g_blink_tx_shadow_parse_fail.store(0, std::memory_order_relaxed);
}

bool vote_ob_wire_shape_ok(const std::string_view payload) noexcept
{
  // Structural stand-in for bt_deserialize(vote_ob) without linking arqnet.
  // Obligation votes are a bencode dict with keys v,t,h,g,i,s,wi,sc and a 64-byte sig.
  if (payload.size() < 20 || payload.front() != 'd' || payload.back() != 'e')
    return false;
  auto has = [&](const std::string_view token) { return payload.find(token) != std::string_view::npos; };
  if (!has("1:v") || !has("1:t") || !has("1:h") || !has("1:g") || !has("1:i") || !has("1:s"))
    return false;
  if (!has("2:wi") || !has("2:sc") || has("2:bh"))
    return false;
  return has("64:");
}

bool is_vote_ob(const std::string_view command) noexcept
{
  return command == "vote_ob";
}

bool is_pulse_rnd(const std::string_view command) noexcept
{
  return command == "pulse_rnd";
}

bool is_blink_tx(const std::string_view command) noexcept
{
  return command == "blink_tx";
}

/// Packed Pulse vote v2: version(1) || height(8) || round(1) || leader(4) || index(4) ||
/// prev(32) || payload_hash(32) || sig(64). Must match pulse::k_relay_vote_bytes.
bool pulse_rnd_wire_shape_ok(const std::string_view payload) noexcept
{
  constexpr size_t k_pulse_rnd_wire_bytes = 146;
  constexpr uint8_t k_pulse_rnd_wire_version = 2;
  return payload.size() == k_pulse_rnd_wire_bytes && static_cast<uint8_t>(payload[0]) == k_pulse_rnd_wire_version;
}

/// Packed Blink vote v1: version(1) || height(8) || txid(32) || index(4) || pub(32) || sig(64).
/// Must match arq_blink::k_relay_vote_bytes.
bool blink_tx_wire_shape_ok(const std::string_view payload) noexcept
{
  constexpr size_t k_blink_tx_wire_bytes = 141;
  constexpr uint8_t k_blink_tx_wire_version = 1;
  return payload.size() == k_blink_tx_wire_bytes && static_cast<uint8_t>(payload[0]) == k_blink_tx_wire_version;
}

void record_shadow_inbound(const std::string_view command, const std::string_view payload) noexcept
{
  // Publish parse outcome before bumping `*_in` with release so readers that
  // observe inbound with acquire cannot see in>0 while both parse counters are
  // still 0 (flaky under ASan / weak memory).
  if (is_vote_ob(command)) {
    if (vote_ob_wire_payload_ok(payload))
      g_vote_ob_shadow_parse_ok.fetch_add(1, std::memory_order_relaxed);
    else
      g_vote_ob_shadow_parse_fail.fetch_add(1, std::memory_order_relaxed);
    g_vote_ob_shadow_in.fetch_add(1, std::memory_order_release);
  }
  if (is_pulse_rnd(command)) {
    if (pulse_rnd_wire_payload_ok(payload))
      g_pulse_rnd_shadow_parse_ok.fetch_add(1, std::memory_order_relaxed);
    else
      g_pulse_rnd_shadow_parse_fail.fetch_add(1, std::memory_order_relaxed);
    g_pulse_rnd_shadow_in.fetch_add(1, std::memory_order_release);
  }
  if (is_blink_tx(command)) {
    if (blink_tx_wire_payload_ok(payload))
      g_blink_tx_shadow_parse_ok.fetch_add(1, std::memory_order_relaxed);
    else
      g_blink_tx_shadow_parse_fail.fetch_add(1, std::memory_order_relaxed);
    g_blink_tx_shadow_in.fetch_add(1, std::memory_order_release);
  }
}

void clear_shadow_endpoint()
{
  std::lock_guard<std::mutex> lock{g_shadow_endpoint_mu};
  g_shadow_endpoint.clear();
  g_shadow_port_offset.store(0, std::memory_order_relaxed);
}
} // namespace

const char* mesh_transport_name() noexcept
{
  // Capability: stage ≥4 + running SocketStack. Live sends use native_mesh_live_at.
  return native_mesh_ready() && native_transport_active() ? k_transport_arqmq : k_transport_snnetwork;
}

const char* live_mesh_transport_name(const uint8_t hard_fork_version) noexcept
{
  return native_mesh_live_at(hard_fork_version) ? k_transport_arqmq : k_transport_snnetwork;
}

bool peer_mesh_is_snnetwork() noexcept
{
  return !(native_mesh_ready() && native_transport_active());
}

bool hf_permits_native_mesh(const uint8_t hard_fork_version) noexcept
{
  return hard_fork_version >= k_hf_native_arqnet_mesh;
}

namespace {
// Incremental native-mesh port stages.
// 0 = missing Curve/ZAP allow path
// 1 = Curve/ZAP landed; peer table missing
// 2 = peer table landed; outbound send missing
// 3 = peer send landed; stagenet vote_ob parity / daemon wiring unverified
// 4 = implementation ready (live relay still needs HF20+ and a CURVE SocketStack)
constexpr int k_native_mesh_port_stage = 4;
} // namespace

bool native_mesh_implementation_ready() noexcept
{
  return k_native_mesh_port_stage >= 4;
}

bool native_mesh_ready_at(const uint8_t hard_fork_version) noexcept
{
  return hf_permits_native_mesh(hard_fork_version) && native_mesh_implementation_ready();
}

bool native_mesh_ready() noexcept
{
  // Without a chain height, only the implementation gate applies.
  return native_mesh_implementation_ready();
}

bool native_mesh_live_at(const uint8_t hard_fork_version) noexcept
{
  if (!native_mesh_ready_at(hard_fork_version))
    return false;
  auto* stack = active_socket_stack();
  return stack && stack->curve_zap_configured();
}

bool hf_requires_native_mesh_exclusive(const uint8_t hard_fork_version) noexcept
{
  return hard_fork_version >= k_hf_native_mesh_exclusive && native_mesh_implementation_ready();
}

const char* native_mesh_blocker() noexcept
{
  if (k_native_mesh_port_stage < 1)
    return "curve-zap-peer-relay-not-ported";
  if (k_native_mesh_port_stage < 2)
    return "peer-endpoints-missing";
  if (k_native_mesh_port_stage < 3)
    return "peer-send-path-missing";
  if (k_native_mesh_port_stage < 4)
    return "vote-ob-parity-unverified";
  return "none";
}

void attach_compatible_mesh_mirrors(SocketStack& stack)
{
  // Mirror the production command set for dual-run ACL/framing checks.
  // Replies make ownership of the live peer mesh explicit: SNNetwork.
  stack.register_handler("vote_ob", CategoryAcl::ServiceNode,
                         [](const InboundRequest&) { return std::string{k_transport_snnetwork}; });
  stack.register_handler("pulse_rnd", CategoryAcl::ServiceNode,
                         [](const InboundRequest&) { return std::string{k_transport_snnetwork}; });
  stack.register_handler("blink_tx", CategoryAcl::ServiceNode,
                         [](const InboundRequest&) { return std::string{k_transport_snnetwork}; });
  stack.register_handler("ping", CategoryAcl::Basic, [](const InboundRequest&) { return std::string{"pong"}; });
  stack.register_handler("pong", CategoryAcl::Basic, [](const InboundRequest&) { return std::string{}; });
  stack.register_handler("arqnet_status", CategoryAcl::Basic, [](const InboundRequest&) {
    return std::string{"backend="} + to_string(current_backend()) + ";transport=" + transport_name() +
           ";mesh=" + mesh_transport_name();
  });
}

void attach_compatible_mesh_mirrors_if_active()
{
  if (auto* stack = active_socket_stack())
    attach_compatible_mesh_mirrors(*stack);
}

void configure_mesh_shadow(SocketStack& stack, std::string public_key, std::string secret_key, AllowConnection allow)
{
  stack.set_curve_identity(std::move(public_key), std::move(secret_key));
  stack.set_allow_connection(std::move(allow));
}

std::string endpoint_with_port_offset(const std::string_view endpoint, const int port_offset)
{
  if (endpoint.empty() || port_offset == 0)
    return std::string{endpoint};
  const auto colon = endpoint.rfind(':');
  if (colon == std::string_view::npos || colon + 1 >= endpoint.size())
    return {};
  // Require tcp://host:port shape (at least one prior ':').
  if (endpoint.find(':') == colon)
    return {};
  try {
    const int port = std::stoi(std::string{endpoint.substr(colon + 1)});
    if (port <= 0 || port > 65535)
      return {};
    const long long next = static_cast<long long>(port) + port_offset;
    if (next <= 0 || next > 65535)
      return {};
    return std::string{endpoint.substr(0, colon + 1)} + std::to_string(next);
  } catch (...) {
    return {};
  }
}

std::error_code start_mesh_shadow_listener(SocketStack& stack, const std::string_view live_arqnet_bind)
{
  const std::string bind = endpoint_with_port_offset(live_arqnet_bind, k_mesh_shadow_port_offset);
  if (bind.empty())
    return std::make_error_code(std::errc::invalid_argument);
  if (!stack.curve_zap_configured())
    return std::make_error_code(std::errc::invalid_argument);

  // Count + parse inbound shadow vote_ob / pulse_rnd / blink_tx (observability only; does not affect consensus).
  stack.register_handler("vote_ob", CategoryAcl::ServiceNode, [](const InboundRequest& req) {
    record_shadow_inbound("vote_ob", req.payload);
    return std::string{k_transport_snnetwork};
  });
  stack.register_handler("pulse_rnd", CategoryAcl::ServiceNode, [](const InboundRequest& req) {
    record_shadow_inbound("pulse_rnd", req.payload);
    return std::string{k_transport_snnetwork};
  });
  stack.register_handler("blink_tx", CategoryAcl::ServiceNode, [](const InboundRequest& req) {
    record_shadow_inbound("blink_tx", req.payload);
    return std::string{k_transport_snnetwork};
  });

  const auto ec = stack.bind_curve(bind);
  if (ec)
    return ec;

  {
    std::lock_guard<std::mutex> lock{g_shadow_endpoint_mu};
    g_shadow_endpoint = stack.last_curve_endpoint();
    if (g_shadow_endpoint.empty())
      g_shadow_endpoint = bind;
  }
  g_shadow_port_offset.store(k_mesh_shadow_port_offset, std::memory_order_relaxed);
  return {};
}

bool native_mesh_shadow_relay_enabled() noexcept
{
  return g_shadow_relay_enabled.load(std::memory_order_relaxed);
}

void set_native_mesh_shadow_relay_enabled(const bool enabled) noexcept
{
  if (enabled)
    reset_shadow_stats();
  else
    clear_shadow_endpoint();
  g_shadow_relay_enabled.store(enabled, std::memory_order_relaxed);
}

std::string native_mesh_shadow_endpoint()
{
  std::lock_guard<std::mutex> lock{g_shadow_endpoint_mu};
  return g_shadow_endpoint;
}

MeshShadowStats native_mesh_shadow_stats() noexcept
{
  // Acquire on inbound counters pairs with release in record_shadow_inbound.
  const uint64_t vote_in = g_vote_ob_shadow_in.load(std::memory_order_acquire);
  const uint64_t pulse_in = g_pulse_rnd_shadow_in.load(std::memory_order_acquire);
  const uint64_t blink_in = g_blink_tx_shadow_in.load(std::memory_order_acquire);
  return MeshShadowStats{g_shadow_attempts.load(std::memory_order_relaxed),
                         g_shadow_ok.load(std::memory_order_relaxed),
                         g_shadow_fail.load(std::memory_order_relaxed),
                         g_live_relays.load(std::memory_order_relaxed),
                         g_vote_ob_live.load(std::memory_order_relaxed),
                         g_vote_ob_shadow_ok.load(std::memory_order_relaxed),
                         g_vote_ob_shadow_fail.load(std::memory_order_relaxed),
                         vote_in,
                         g_vote_ob_shadow_parse_ok.load(std::memory_order_relaxed),
                         g_vote_ob_shadow_parse_fail.load(std::memory_order_relaxed),
                         g_pulse_rnd_live.load(std::memory_order_relaxed),
                         g_pulse_rnd_shadow_ok.load(std::memory_order_relaxed),
                         g_pulse_rnd_shadow_fail.load(std::memory_order_relaxed),
                         pulse_in,
                         g_pulse_rnd_shadow_parse_ok.load(std::memory_order_relaxed),
                         g_pulse_rnd_shadow_parse_fail.load(std::memory_order_relaxed),
                         g_blink_tx_live.load(std::memory_order_relaxed),
                         g_blink_tx_shadow_ok.load(std::memory_order_relaxed),
                         g_blink_tx_shadow_fail.load(std::memory_order_relaxed),
                         blink_in,
                         g_blink_tx_shadow_parse_ok.load(std::memory_order_relaxed),
                         g_blink_tx_shadow_parse_fail.load(std::memory_order_relaxed)};
}

void note_live_mesh_relay(const std::string_view command) noexcept
{
  if (!native_mesh_shadow_relay_enabled())
    return;
  g_live_relays.fetch_add(1, std::memory_order_relaxed);
  if (is_vote_ob(command))
    g_vote_ob_live.fetch_add(1, std::memory_order_relaxed);
  if (is_pulse_rnd(command))
    g_pulse_rnd_live.fetch_add(1, std::memory_order_relaxed);
  if (is_blink_tx(command))
    g_blink_tx_live.fetch_add(1, std::memory_order_relaxed);
}

void note_inbound_mesh_shadow(const std::string_view command, const std::string_view payload) noexcept
{
  record_shadow_inbound(command, payload);
}

uint32_t native_mesh_shadow_ok_rate_bps() noexcept
{
  const uint64_t attempts = g_shadow_attempts.load(std::memory_order_relaxed);
  if (attempts == 0)
    return 0;
  const uint64_t ok = g_shadow_ok.load(std::memory_order_relaxed);
  return static_cast<uint32_t>((ok * 10000ull) / attempts);
}

bool native_mesh_shadow_parity_sample_ok(const uint64_t min_vote_ob_live, const uint32_t min_ok_rate_bps) noexcept
{
  const uint64_t vote_live = g_vote_ob_live.load(std::memory_order_relaxed);
  if (vote_live < min_vote_ob_live)
    return false;
  const uint64_t vote_ok = g_vote_ob_shadow_ok.load(std::memory_order_relaxed);
  const uint64_t vote_fail = g_vote_ob_shadow_fail.load(std::memory_order_relaxed);
  const uint64_t vote_attempts = vote_ok + vote_fail;
  if (vote_attempts == 0)
    return false;
  const uint32_t rate = static_cast<uint32_t>((vote_ok * 10000ull) / vote_attempts);
  if (rate < min_ok_rate_bps)
    return false;

  const uint64_t vin = g_vote_ob_shadow_in.load(std::memory_order_acquire);
  if (vin < min_vote_ob_live)
    return false;
  const uint64_t parse_ok = g_vote_ob_shadow_parse_ok.load(std::memory_order_relaxed);
  const uint64_t parse_fail = g_vote_ob_shadow_parse_fail.load(std::memory_order_relaxed);
  const uint64_t parse_attempts = parse_ok + parse_fail;
  if (parse_attempts == 0)
    return false;
  const uint32_t parse_rate = static_cast<uint32_t>((parse_ok * 10000ull) / parse_attempts);
  return parse_rate >= min_ok_rate_bps;
}

void set_vote_ob_payload_validator(const VoteObPayloadValidator validator) noexcept
{
  g_vote_ob_validator.store(validator, std::memory_order_relaxed);
}

bool vote_ob_wire_payload_ok(const std::string_view payload) noexcept
{
  const auto validator = g_vote_ob_validator.load(std::memory_order_relaxed);
  if (validator) {
    try {
      return validator(payload);
    } catch (...) {
      return false;
    }
  }
  return vote_ob_wire_shape_ok(payload);
}

void set_pulse_rnd_payload_validator(const PulseRndPayloadValidator validator) noexcept
{
  g_pulse_rnd_validator.store(validator, std::memory_order_relaxed);
}

bool pulse_rnd_wire_payload_ok(const std::string_view payload) noexcept
{
  const auto validator = g_pulse_rnd_validator.load(std::memory_order_relaxed);
  if (validator) {
    try {
      return validator(payload);
    } catch (...) {
      return false;
    }
  }
  return pulse_rnd_wire_shape_ok(payload);
}

void set_blink_tx_payload_validator(const BlinkTxPayloadValidator validator) noexcept
{
  g_blink_tx_validator.store(validator, std::memory_order_relaxed);
}

bool blink_tx_wire_payload_ok(const std::string_view payload) noexcept
{
  const auto validator = g_blink_tx_validator.load(std::memory_order_relaxed);
  if (validator) {
    try {
      return validator(payload);
    } catch (...) {
      return false;
    }
  }
  return blink_tx_wire_shape_ok(payload);
}

void shadow_send_to_peer(const std::string_view pubkey, const std::string_view command, const std::string_view payload,
                         const std::string_view hint)
{
  if (!native_mesh_shadow_relay_enabled())
    return;
  auto* stack = active_socket_stack();
  const bool vote = is_vote_ob(command);
  const bool pulse = is_pulse_rnd(command);
  const bool blink = is_blink_tx(command);
  if (!stack || !stack->curve_zap_configured() || pubkey.size() != 32 || command.empty()) {
    g_shadow_attempts.fetch_add(1, std::memory_order_relaxed);
    g_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    if (vote)
      g_vote_ob_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    if (pulse)
      g_pulse_rnd_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    if (blink)
      g_blink_tx_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  std::string send_hint{hint};
  const int offset = g_shadow_port_offset.load(std::memory_order_relaxed);
  if (!send_hint.empty() && offset != 0) {
    const std::string rewritten = endpoint_with_port_offset(send_hint, offset);
    if (!rewritten.empty())
      send_hint = rewritten;
  }

  g_shadow_attempts.fetch_add(1, std::memory_order_relaxed);
  if (!send_hint.empty())
    stack->peers().note_peer(std::string{pubkey}, send_hint, true);
  if (stack->send_to_peer(pubkey, command, payload, send_hint)) {
    g_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    if (vote)
      g_vote_ob_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    if (pulse)
      g_pulse_rnd_shadow_fail.fetch_add(1, std::memory_order_relaxed);
    if (blink)
      g_blink_tx_shadow_fail.fetch_add(1, std::memory_order_relaxed);
  } else {
    g_shadow_ok.fetch_add(1, std::memory_order_relaxed);
    if (vote)
      g_vote_ob_shadow_ok.fetch_add(1, std::memory_order_relaxed);
    if (pulse)
      g_pulse_rnd_shadow_ok.fetch_add(1, std::memory_order_relaxed);
    if (blink)
      g_blink_tx_shadow_ok.fetch_add(1, std::memory_order_relaxed);
  }
}

void primary_mesh_send_to_peer(const std::string_view pubkey, const std::string_view command,
                               const std::string_view payload, const std::string_view hint)
{
  // Dead unless the implementation gate is open (stage ≥4). Callers that skip
  // SNNetwork must also require native_mesh_live_at(hf) so legacy nodes fallback.
  if (!native_mesh_ready())
    return;
  auto* stack = active_socket_stack();
  if (!stack || !stack->curve_zap_configured() || pubkey.size() != 32 || command.empty())
    return;

  std::string send_hint{hint};
  if (!send_hint.empty()) {
    const std::string rewritten = endpoint_with_port_offset(send_hint, k_mesh_shadow_port_offset);
    if (!rewritten.empty())
      send_hint = rewritten;
  }
  if (!send_hint.empty())
    stack->peers().note_peer(std::string{pubkey}, send_hint, true);
  (void)stack->send_to_peer(pubkey, command, payload, send_hint);
}
} // namespace arqmq
