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
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace arq_messaging {
using swarm_id = std::uint64_t;

/// Soft caps for in-memory swarm membership until Storage Server owns the map.
constexpr std::size_t max_service_nodes_per_swarm = 100;
constexpr std::size_t max_swarm_mappings = 10000;

inline bool allow_service_node_count(std::size_t count) noexcept
{
  return count <= max_service_nodes_per_swarm;
}

/// Deterministic swarm id from pubkey bytes (FNV-1a 64-bit). Not a
/// consensus primitive — scaffolding until Storage Server owns assignment.
inline swarm_id hash_pubkey_to_swarm(std::string_view pubkey) noexcept
{
  constexpr std::uint64_t offset = 14695981039346656037ull;
  constexpr std::uint64_t prime = 1099511628211ull;
  std::uint64_t hash = offset;
  for (unsigned char c : pubkey) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= prime;
  }
  return hash == 0 ? 1 : hash;
}

struct SwarmMapping
{
  swarm_id id = 0;
  std::vector<std::string> service_nodes;
  std::error_code error{};

  explicit operator bool() const noexcept { return !error; }
};

class SwarmMap
{
public:
  virtual ~SwarmMap() = default;

  virtual std::error_code refresh() { return std::make_error_code(std::errc::function_not_supported); }

  virtual SwarmMapping get_swarm(std::string_view pubkey) const
  {
    (void)pubkey;
    SwarmMapping mapping;
    mapping.error = std::make_error_code(std::errc::function_not_supported);
    return mapping;
  }
};

/// Deterministic in-memory swarm assignment for tests and local scaffolding.
class InMemorySwarmMap : public SwarmMap
{
public:
  std::error_code set_mapping(std::string pubkey, SwarmMapping mapping)
  {
    if (pubkey.empty())
      return std::make_error_code(std::errc::invalid_argument);
    if (!allow_service_node_count(mapping.service_nodes.size()))
      return std::make_error_code(std::errc::message_size);

    std::lock_guard<std::mutex> lock{mutex_};
    if (mappings_.find(pubkey) == mappings_.end() && mappings_.size() >= max_swarm_mappings)
      return std::make_error_code(std::errc::no_space_on_device);
    mappings_[std::move(pubkey)] = std::move(mapping);
    return {};
  }

  std::size_t size() const
  {
    std::lock_guard<std::mutex> lock{mutex_};
    return mappings_.size();
  }

  std::error_code refresh() override { return {}; }

  SwarmMapping get_swarm(std::string_view pubkey) const override
  {
    std::lock_guard<std::mutex> lock{mutex_};
    const auto it = mappings_.find(std::string{pubkey});
    if (it == mappings_.end()) {
      SwarmMapping missing;
      missing.error = std::make_error_code(std::errc::no_such_file_or_directory);
      return missing;
    }
    return it->second;
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, SwarmMapping> mappings_;
};
} // namespace arq_messaging
