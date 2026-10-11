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

#include <cstdint>
#include <system_error>

namespace arqmq {
class SocketStack;

/// Must match `cryptonote::network_version_20` / `HF_VERSION_NATIVE_ARQNET_MESH`.
inline constexpr uint8_t k_hf_native_arqnet_mesh = 20;
/// Must match `cryptonote::network_version_21` / `HF_VERSION_PULSE_EXCLUSIVE`.
inline constexpr uint8_t k_hf_native_mesh_exclusive = 21;
/// Messaging facade backends.
/// `LegacyArqNet` keeps production mesh I/O on `arqnet::SNNetwork`.
/// `ArqMq` starts the dedicated ArqMQ socket/worker stack (`transport=arqmq`).
/// Peer quorum stays on SNNetwork unless `native_mesh_live_at` (HF20+ / stage 4 /
/// CURVE stack).
enum class Backend
{
  LegacyArqNet,
  ArqMq
};

enum class CategoryAcl
{
  Denied,
  Basic,
  ServiceNode,
  Admin
};

struct Config
{
  Backend backend = Backend::LegacyArqNet;
  CategoryAcl default_acl = CategoryAcl::Denied;
};

const char* to_string(Backend backend) noexcept;
const char* to_string(CategoryAcl acl) noexcept;

/// Returns true when `granted` is at least as privileged as `required`.
/// Ordering: Denied < Basic < ServiceNode < Admin.
bool allows(CategoryAcl required, CategoryAcl granted) noexcept;

/// Initializes the messaging facade. `ArqMq` starts dedicated socket internals.
std::error_code init(const Config& config = {}) noexcept;

/// Shuts down the messaging facade and stops any dedicated socket stack.
std::error_code shutdown() noexcept;

/// Returns the currently selected backend, even if initialization failed.
Backend current_backend() noexcept;

/// Stable name of the active wire transport under the facade.
const char* transport_name() noexcept;

/// Returns true when the dedicated ArqMQ socket stack is running.
bool native_transport_active() noexcept;

/// Peer Curve/ZMQ mesh capability name (`arqmq` when stage ≥4 and the dedicated
/// stack is running; otherwise `snnetwork`). Live relay still needs HF20+ and
/// CURVE — see `native_mesh_live_at`.
const char* mesh_transport_name() noexcept;

/// Live peer-quorum transport name at this hard-fork version.
const char* live_mesh_transport_name(uint8_t hard_fork_version) noexcept;

/// True while the dedicated stack is not carrying peer mesh (legacy default).
bool peer_mesh_is_snnetwork() noexcept;

/// True when the active hard-fork version permits native mesh cutover (HF20+).
bool hf_permits_native_mesh(uint8_t hard_fork_version) noexcept;

/// False until Curve/ZAP + peer send + vote_ob relay are ported (stage ≥4).
bool native_mesh_implementation_ready() noexcept;

/// HF permit AND implementation ready (does not require a running SocketStack).
bool native_mesh_ready_at(uint8_t hard_fork_version) noexcept;

/// Convenience without chain context: true after the implementation gate (stage ≥4).
bool native_mesh_ready() noexcept;

/// True when this node should send quorum traffic on SocketStack: HF20+,
/// stage ≥4, and the active stack has CURVE identity. Otherwise SNNetwork.
bool native_mesh_live_at(uint8_t hard_fork_version) noexcept;

/// True from HF21: SNNetwork is no longer the intended live mesh (exclusive).
bool hf_requires_native_mesh_exclusive(uint8_t hard_fork_version) noexcept;

/// Stable reason code while native mesh cannot cut over.
/// Stages: curve-zap-peer-relay-not-ported → peer-endpoints-missing →
/// peer-send-path-missing → vote-ob-parity-unverified → none.
const char* native_mesh_blocker() noexcept;

/// Returns the default ACL configured at init (Denied after shutdown).
CategoryAcl default_acl() noexcept;

/// Returns true when the facade is initialized and ready for use.
bool is_initialized() noexcept;

/// Active dedicated socket stack, or nullptr when using legacy SNNetwork only.
SocketStack* active_socket_stack() noexcept;
} // namespace arqmq
