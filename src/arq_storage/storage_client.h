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

#include "storage_endpoint.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace arq_storage {
template <typename T>
struct Result
{
  T value{};
  std::error_code error{};

  explicit operator bool() const noexcept { return !error; }
};

struct StoreRequest
{
  std::string namespace_name;
  std::string key;
  std::string value;
  std::uint32_t ttl_seconds = 0;
};

/// Snapshot from `GET /status` on a live `arqma-storage` process.
struct StatusSnapshot
{
  bool reachable = false;
  std::string service;
  std::uint64_t gossip_interval_sec = 0;
  std::uint64_t peer_count = 0;
  std::uint64_t snode_count = 0;
  std::uint64_t kv_entries = 0;
  std::uint64_t gossip_rounds = 0;
  std::uint64_t digest_ok = 0;
  std::uint64_t sync_ok = 0;
  std::uint64_t membership_ok = 0;
  std::uint64_t gossip_fail = 0;
  std::string error;
};

/// Remote talks to an external Storage Server. InMemory is for tests only.
enum class Backend
{
  Remote,
  InMemory
};

struct Config
{
  Backend backend = Backend::Remote;
  /// Example: http://127.0.0.1:22021 — required for Remote reachability probes.
  std::string base_url;
  std::chrono::milliseconds connect_timeout{2000};
  /// Optional stack token (`ARQMA_STACK_TOKEN`). Empty disables auth.
  std::string token;
};

class StorageClient
{
public:
  explicit StorageClient(Config config = {}) noexcept;

  Backend backend() const noexcept { return config_.backend; }
  const Config& config() const noexcept { return config_; }
  Endpoint endpoint() const noexcept { return endpoint_; }

  std::error_code ping() const noexcept;
  /// Probe `GET /status` and parse gossip / catalog counters when reachable.
  StatusSnapshot fetch_status() const noexcept;
  std::error_code store(const StoreRequest& request) noexcept;
  Result<std::string> retrieve(std::string namespace_name, std::string key) const noexcept;
  Result<std::vector<std::string>> list_keys(std::string namespace_name) const noexcept;
  Result<std::vector<std::string>> get_snodes_for_pubkey(std::string pubkey) const noexcept;
  Result<std::pair<std::uint64_t, std::vector<std::string>>> get_swarm(std::string pubkey) const noexcept;

  void set_snodes_for_pubkey(std::string pubkey, std::vector<std::string> snodes);

private:
  std::vector<Endpoint> inbox_swarm_endpoints(const std::string& namespace_name) const;
  Config config_;
  Endpoint endpoint_;
  mutable std::mutex mutex_;
  std::map<std::pair<std::string, std::string>, std::string> values_;
  std::map<std::string, std::vector<std::string>> snodes_;
};

/// Process-wide client used by daemon RPC status handlers.
void configure_daemon_client(Config config);
StorageClient daemon_client();
} // namespace arq_storage
