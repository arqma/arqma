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

#include "peer_table.hpp"

namespace arqmq {
void PeerTable::note_peer(std::string pubkey, std::string hint, const bool service_node)
{
  if (pubkey.size() != 32)
    return;
  std::lock_guard<std::mutex> lock{mu_};
  auto& entry = peers_[pubkey];
  entry.pubkey = std::move(pubkey);
  if (!hint.empty())
    entry.hint = std::move(hint);
  entry.service_node = service_node;
}

std::optional<PeerEndpoint> PeerTable::find(const std::string_view pubkey) const
{
  if (pubkey.size() != 32)
    return std::nullopt;
  std::lock_guard<std::mutex> lock{mu_};
  const auto it = peers_.find(std::string{pubkey});
  if (it == peers_.end())
    return std::nullopt;
  return it->second;
}

bool PeerTable::erase(const std::string_view pubkey)
{
  if (pubkey.size() != 32)
    return false;
  std::lock_guard<std::mutex> lock{mu_};
  return peers_.erase(std::string{pubkey}) > 0;
}

void PeerTable::clear()
{
  std::lock_guard<std::mutex> lock{mu_};
  peers_.clear();
}

size_t PeerTable::size() const
{
  std::lock_guard<std::mutex> lock{mu_};
  return peers_.size();
}
} // namespace arqmq
