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

#include "router_service.h"
#include "arq_storage/storage_endpoint.h"

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

namespace arq_router {
namespace {
bool listen_looks_sane(const std::string& listen) noexcept
{
  if (listen.empty())
    return true; // optional until sockets land
  std::string host;
  std::uint16_t port = 0;
  return arq_storage::parse_listen_address(listen, host, port);
}
} // namespace

std::error_code validate_config(const RouterConfig& config) noexcept
{
  if (!config.enabled)
    return std::make_error_code(std::errc::operation_not_permitted);
  if (config.data_dir.empty())
    return std::make_error_code(std::errc::invalid_argument);
  if (!listen_looks_sane(config.listen))
    return std::make_error_code(std::errc::invalid_argument);
  return {};
}

RouterService::RouterService(RouterConfig config) : config_{std::move(config)} {}

const char* RouterService::state_name() const noexcept
{
  if (running_)
    return "running";
  if (initialized_)
    return "initialized";
  return "idle";
}

std::error_code RouterService::init() noexcept
{
  if (auto ec = validate_config(config_))
    return ec;
  // Experimental lifecycle only — no onion routing sockets yet.
  initialized_ = true;
  running_ = false;
  return {};
}

std::error_code RouterService::start() noexcept
{
  if (!initialized_)
    return std::make_error_code(std::errc::not_connected);
  if (auto ec = validate_config(config_))
    return ec;
  running_ = true;
  return {};
}

std::error_code RouterService::stop() noexcept
{
  running_ = false;
  return {};
}
} // namespace arq_router
