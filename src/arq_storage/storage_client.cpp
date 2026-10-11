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

#include "storage_client.h"
#include "http_io.h"
#include "arq_messaging/swarm_map.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

namespace {
std::error_code not_connected() noexcept
{
  return std::make_error_code(std::errc::not_connected);
}

std::error_code invalid_argument() noexcept
{
  return std::make_error_code(std::errc::invalid_argument);
}

bool tcp_connect_only(const arq_storage::Endpoint& endpoint) noexcept
{
  try {
    boost::asio::io_context io;
    boost::asio::ip::tcp::resolver resolver{io};
    const auto results = resolver.resolve(endpoint.host, std::to_string(endpoint.port));
    boost::asio::ip::tcp::socket socket{io};
    boost::system::error_code ec;
    boost::asio::connect(socket, results, ec);
    if (ec)
      return false;
    socket.close();
    return true;
  } catch (...) {
    return false;
  }
}

/// Cleartext HTTP GET probe. Any response starting with "HTTP/" counts as
/// reachable (including 4xx). TLS endpoints stay on TCP-only until a TLS
/// client stack is wired into arq_storage.
bool http_get_probe(const arq_storage::Endpoint& endpoint) noexcept
{
  try {
    const auto request = arq_storage::format_http_get_request(endpoint);
    if (request.empty())
      return false;

    boost::asio::io_context io;
    boost::asio::ip::tcp::resolver resolver{io};
    const auto results = resolver.resolve(endpoint.host, std::to_string(endpoint.port));
    boost::asio::ip::tcp::socket socket{io};
    boost::system::error_code ec;
    boost::asio::connect(socket, results, ec);
    if (ec)
      return false;

#if defined(_WIN32)
    DWORD ms = 2000;
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    boost::asio::write(socket, boost::asio::buffer(request), ec);
    if (ec)
      return false;

    std::array<char, 16> buf{};
    const std::size_t n = boost::asio::read(socket, boost::asio::buffer(buf), boost::asio::transfer_at_least(5), ec);
    socket.close();
    if (n < 5)
      return false;
    return std::string_view{buf.data(), 5} == "HTTP/";
  } catch (...) {
    return false;
  }
}

std::mutex g_daemon_mutex;
arq_storage::Config g_daemon_config{};
} // namespace

namespace arq_storage {
StorageClient::StorageClient(Config config) noexcept
    : config_{std::move(config)}, endpoint_{parse_endpoint(config_.base_url)}
{
  if (config_.token.empty()) {
    if (const char* token = std::getenv("ARQMA_STACK_TOKEN"); token && *token)
      config_.token = token;
  }
}

std::error_code StorageClient::ping() const noexcept
{
  if (config_.backend == Backend::InMemory)
    return {};

  if (!endpoint_)
    return not_connected();

  if (endpoint_.tls)
    return tcp_connect_only(endpoint_) ? std::error_code{} : not_connected();

  return http_get_probe(endpoint_) ? std::error_code{} : not_connected();
}

namespace {
std::uint64_t json_u64_field(const std::string_view body, const std::string_view key) noexcept
{
  const std::string needle = "\"" + std::string{key} + "\":";
  const auto pos = body.find(needle);
  if (pos == std::string_view::npos)
    return 0;
  const auto start = pos + needle.size();
  char* end = nullptr;
  const auto value = std::strtoull(body.data() + start, &end, 10);
  if (end == body.data() + start)
    return 0;
  return static_cast<std::uint64_t>(value);
}

std::string json_string_field(const std::string_view body, const std::string_view key)
{
  const std::string needle = "\"" + std::string{key} + "\":\"";
  const auto pos = body.find(needle);
  if (pos == std::string_view::npos)
    return {};
  const auto start = pos + needle.size();
  const auto end = body.find('"', start);
  if (end == std::string_view::npos)
    return {};
  return std::string{body.substr(start, end - start)};
}
} // namespace

StatusSnapshot StorageClient::fetch_status() const noexcept
{
  StatusSnapshot out;
  if (config_.backend == Backend::InMemory) {
    out.reachable = true;
    out.service = "arqma-storage-inmemory";
    return out;
  }
  if (!endpoint_ || endpoint_.tls) {
    out.error = not_connected().message();
    return out;
  }
  const auto result = http_exchange(endpoint_, "GET", "/status", {}, config_.connect_timeout);
  if (!result) {
    out.error = result.error ? result.error.message() : not_connected().message();
    return out;
  }
  out.reachable = true;
  out.service = json_string_field(result.body, "service");
  if (out.service.empty() && result.body.find("arqma-storage") != std::string::npos)
    out.service = "arqma-storage";
  out.gossip_interval_sec = json_u64_field(result.body, "gossip_interval_sec");
  out.peer_count = json_u64_field(result.body, "peer_count");
  out.snode_count = json_u64_field(result.body, "snode_count");
  out.kv_entries = json_u64_field(result.body, "kv_entries");
  out.gossip_rounds = json_u64_field(result.body, "gossip_rounds");
  out.digest_ok = json_u64_field(result.body, "digest_ok");
  out.sync_ok = json_u64_field(result.body, "sync_ok");
  out.membership_ok = json_u64_field(result.body, "membership_ok");
  out.gossip_fail = json_u64_field(result.body, "gossip_fail");
  return out;
}

std::error_code StorageClient::store(const StoreRequest& request) noexcept
{
  if (request.namespace_name.empty() || request.key.empty())
    return invalid_argument();
  if (config_.backend == Backend::InMemory) {
    std::lock_guard<std::mutex> lock{mutex_};
    values_[{request.namespace_name, request.key}] = request.value;
    return {};
  }
  if (!endpoint_ || endpoint_.tls)
    return not_connected();
  auto path = kv_path(request.namespace_name, request.key);
  if (request.ttl_seconds != 0)
    path += "&ttl=" + std::to_string(request.ttl_seconds);
  const auto result =
      http_exchange(endpoint_, "PUT", with_token(path, config_.token), request.value, config_.connect_timeout);
  if (!result)
    return result.error ? result.error : not_connected();
  return {};
}

Result<std::string> StorageClient::retrieve(std::string namespace_name, std::string key) const noexcept
{
  if (namespace_name.empty() || key.empty())
    return {{}, invalid_argument()};
  if (config_.backend == Backend::InMemory) {
    std::lock_guard<std::mutex> lock{mutex_};
    const auto it = values_.find({namespace_name, key});
    if (it == values_.end())
      return {{}, std::make_error_code(std::errc::no_such_file_or_directory)};
    return {it->second, {}};
  }
  if (!endpoint_ || endpoint_.tls)
    return {{}, not_connected()};
  const auto result = http_exchange(endpoint_, "GET", with_token(kv_path(namespace_name, key), config_.token), {},
                                    config_.connect_timeout);
  if (result.status != 404 && result)
    return {result.body, {}};
  if (result.status != 404 && result.error)
    return {{}, result.error};
  for (const auto& ep : inbox_swarm_endpoints(namespace_name)) {
    const auto replica =
        http_exchange(ep, "GET", with_token(kv_path(namespace_name, key), config_.token), {}, config_.connect_timeout);
    if (replica)
      return {replica.body, {}};
  }
  if (result.status == 404)
    return {{}, std::make_error_code(std::errc::no_such_file_or_directory)};
  return {{}, result.error ? result.error : not_connected()};
}

Result<std::vector<std::string>> StorageClient::list_keys(std::string namespace_name) const noexcept
{
  if (namespace_name.empty())
    return {{}, invalid_argument()};
  if (config_.backend == Backend::InMemory) {
    std::vector<std::string> keys;
    std::lock_guard<std::mutex> lock{mutex_};
    for (const auto& kv : values_) {
      if (kv.first.first == namespace_name)
        keys.push_back(kv.first.second);
    }
    return {keys, {}};
  }
  if (!endpoint_ || endpoint_.tls)
    return {{}, not_connected()};
  const auto result =
      http_exchange(endpoint_, "GET", with_token("/v1/list?ns=" + url_encode(namespace_name), config_.token), {},
                    config_.connect_timeout);
  if (!result)
    return {{}, result.error ? result.error : not_connected()};
  std::set<std::string> unique;
  std::string line;
  std::istringstream iss{result.body};
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (!line.empty())
      unique.insert(std::move(line));
  }
  for (const auto& ep : inbox_swarm_endpoints(namespace_name)) {
    const auto replica = http_exchange(
        ep, "GET", with_token("/v1/list?ns=" + url_encode(namespace_name), config_.token), {}, config_.connect_timeout);
    if (!replica)
      continue;
    std::istringstream replica_iss{replica.body};
    while (std::getline(replica_iss, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (!line.empty())
        unique.insert(std::move(line));
    }
  }
  return {{unique.begin(), unique.end()}, {}};
}

Result<std::vector<std::string>> StorageClient::get_snodes_for_pubkey(std::string pubkey) const noexcept
{
  if (pubkey.empty())
    return {{}, invalid_argument()};
  if (config_.backend == Backend::InMemory) {
    std::lock_guard<std::mutex> lock{mutex_};
    const auto it = snodes_.find(pubkey);
    if (it == snodes_.end())
      return {{}, {}};
    return {it->second, {}};
  }
  if (!endpoint_ || endpoint_.tls)
    return {{}, not_connected()};
  const auto result =
      http_exchange(endpoint_, "GET", with_token(snodes_path(pubkey), config_.token), {}, config_.connect_timeout);
  if (!result)
    return {{}, result.error ? result.error : not_connected()};
  return {parse_url_lines(result.body), {}};
}

void StorageClient::set_snodes_for_pubkey(std::string pubkey, std::vector<std::string> snodes)
{
  if (config_.backend == Backend::InMemory) {
    std::lock_guard<std::mutex> lock{mutex_};
    snodes_[std::move(pubkey)] = std::move(snodes);
    return;
  }
  if (!endpoint_ || endpoint_.tls)
    return;
  http_exchange(endpoint_, "PUT", with_token(snodes_path(pubkey), config_.token),
                join_url_lines(merge_snode_urls({}, snodes)), config_.connect_timeout);
}

Result<std::pair<std::uint64_t, std::vector<std::string>>> StorageClient::get_swarm(std::string pubkey) const noexcept
{
  if (pubkey.empty())
    return {{}, invalid_argument()};
  if (config_.backend == Backend::InMemory) {
    std::lock_guard<std::mutex> lock{mutex_};
    std::vector<std::string> members;
    const auto it = snodes_.find(pubkey);
    if (it != snodes_.end())
      members = it->second;
    return {{arq_messaging::hash_pubkey_to_swarm(pubkey), std::move(members)}, {}};
  }
  if (!endpoint_ || endpoint_.tls)
    return {{}, not_connected()};
  const auto result =
      http_exchange(endpoint_, "GET", with_token("/v1/swarm?pubkey=" + url_encode(pubkey), config_.token), {},
                    config_.connect_timeout);
  if (!result)
    return {{}, result.error ? result.error : not_connected()};
  std::string line;
  std::istringstream iss{result.body};
  if (!std::getline(iss, line))
    return {{}, not_connected()};
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  char* end = nullptr;
  const auto id = static_cast<std::uint64_t>(std::strtoull(line.c_str(), &end, 10));
  if (end == line.c_str() || (end && *end != '\0'))
    return {{}, invalid_argument()};
  const auto rest = result.body.size() > line.size() ? result.body.substr(line.size() + 1) : std::string{};
  return {{id, parse_url_lines(rest)}, {}};
}

std::vector<Endpoint> StorageClient::inbox_swarm_endpoints(const std::string& namespace_name) const
{
  std::vector<Endpoint> out;
  if (config_.backend != Backend::Remote || !endpoint_ || endpoint_.tls)
    return out;
  const auto pub = inbox_pubkey(namespace_name);
  if (pub.empty())
    return out;
  const auto members = get_snodes_for_pubkey(pub);
  if (!members)
    return out;
  const auto self = format_http_authority(endpoint_.host, endpoint_.port);
  for (const auto& url : members.value) {
    const auto ep = parse_endpoint(url);
    if (!ep || ep.tls)
      continue;
    if (format_http_authority(ep.host, ep.port) == self)
      continue;
    out.push_back(ep);
    if (out.size() >= max_swarm_fallback)
      break;
  }
  return out;
}

void configure_daemon_client(Config config)
{
  std::lock_guard<std::mutex> lock{g_daemon_mutex};
  g_daemon_config = std::move(config);
}

StorageClient daemon_client()
{
  std::lock_guard<std::mutex> lock{g_daemon_mutex};
  return StorageClient{g_daemon_config};
}
} // namespace arq_storage
