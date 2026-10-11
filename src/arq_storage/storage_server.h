// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <thread>

namespace arq_storage {
/// In-process HTTP Storage Server (also used by the `arqma-storage` binary).
class StorageServer
{
public:
  StorageServer();
  ~StorageServer();

  StorageServer(const StorageServer&) = delete;
  StorageServer& operator=(const StorageServer&) = delete;

  /// Bind `host:port`. Port 0 picks an ephemeral port (tests).
  std::error_code listen(const std::string& host, std::uint16_t port);
  void stop();

  /// Optional on-disk volume (`kv/` + `snodes/`). Empty keeps RAM-only.
  void set_data_dir(std::string path);
  const std::string& data_dir() const noexcept;

  /// Best-effort swarm replica. PUTs fan out with `replicate=0` so peers do not loop.
  void add_peer(std::string base_url);
  /// When non-empty, `/v1/*` requires matching `token=` (status stays open).
  void set_token(std::string token);
  /// Anti-entropy gossip period. `0` disables. Default 15s.
  void set_gossip_interval(std::chrono::seconds interval);

  std::uint16_t port() const noexcept;
  bool running() const noexcept;
  std::string base_url() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace arq_storage
