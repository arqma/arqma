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

#include "arqmq.h"

#include <string>
#include <string_view>

namespace arqmq {
/// Network class used only for backend selection policy (not consensus).
enum class NetworkClass
{
  Mainnet,
  Testnet,
  Stagenet
};

struct BackendSelection
{
  Backend backend = Backend::LegacyArqNet;
  /// True when the operator request was overridden for safety/compatibility.
  bool overridden = false;
  std::string reason;
};

/// Resolve `--arqnet-backend` for the active network.
///
/// Mainnet policy (production): experimental `arqmq` is refused unless
/// `allow_experimental_on_mainnet` is explicitly set. Peer mesh remains
/// SNNetwork regardless of selection until cutover.
BackendSelection resolve_backend(std::string_view requested, NetworkClass network,
                                 bool allow_experimental_on_mainnet = false);

struct MeshShadowSelection
{
  bool enabled = false;
  /// True when the operator request was overridden for safety/compatibility.
  bool overridden = false;
  std::string reason;
};

/// Resolve `--arqnet-mesh-shadow` for the active network.
///
/// Requires an active ArqMQ SocketStack. Mainnet refuses shadow dual-write
/// unless `allow_experimental_on_mainnet` is set. Live peer mesh stays SNNetwork.
MeshShadowSelection resolve_mesh_shadow(bool requested, NetworkClass network, bool allow_experimental_on_mainnet,
                                        bool native_transport_active);
} // namespace arqmq
