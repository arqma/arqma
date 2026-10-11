// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include "arq_messaging/identity.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <system_error>

namespace arq_router {
/// In-process HTTP listener used by `arqma-router` and tests.
class RouterServer
{
public:
  RouterServer();
  ~RouterServer();

  RouterServer(const RouterServer&) = delete;
  RouterServer& operator=(const RouterServer&) = delete;

  void set_identity(arq_messaging::Identity hop);
  void set_storage_url(std::string url);
  void set_token(std::string token);

  std::error_code listen(const std::string& host, std::uint16_t port);
  void stop();

  std::uint16_t port() const noexcept;
  bool running() const noexcept;
  std::string base_url() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace arq_router
