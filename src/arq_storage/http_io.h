// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include "storage_endpoint.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace arq_storage {
constexpr std::size_t max_http_header_bytes = 8192;
constexpr std::size_t max_http_body_bytes = 1024 * 1024;
constexpr std::size_t max_kv_name_bytes = 128;
constexpr std::size_t max_swarm_fallback = 8;
constexpr std::size_t max_snode_urls = 32;
constexpr std::size_t max_kv_entries = 4096;
constexpr std::size_t max_kv_entries_per_namespace = 512;
constexpr std::size_t max_sync_response_bytes = 512 * 1024;
constexpr std::size_t max_gossip_peers = 8;
constexpr std::size_t max_gossip_namespaces = 8;
constexpr std::size_t max_gossip_snode_pubs = 16;

inline bool kv_quota_exceeded(std::size_t total, std::size_t ns_count, bool inserting_new) noexcept
{
  if (!inserting_new)
    return false;
  return total >= max_kv_entries || ns_count >= max_kv_entries_per_namespace;
}

std::string url_encode(std::string_view raw);
std::string url_decode(std::string_view raw);
std::string query_get(std::string_view path, std::string_view key);

inline bool is_hex64(const std::string_view raw) noexcept
{
  if (raw.size() != 64)
    return false;
  for (unsigned char c : raw) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
      return false;
  }
  return true;
}

inline std::uint64_t fnv1a64(const std::string_view raw, std::uint64_t seed) noexcept
{
  for (unsigned char c : raw)
    seed = (seed ^ static_cast<std::uint64_t>(c)) * 1099511628211ull;
  return seed;
}

/// 16 hex chars from FNV-1a64(key + NUL + value + expiry LE bytes).
inline std::string entry_digest_hex(const std::string_view key, const std::string_view value,
                                    const std::uint64_t expiry)
{
  std::string raw;
  raw.reserve(key.size() + 1 + value.size() + 8);
  raw.append(key);
  raw.push_back('\0');
  raw.append(value);
  for (int i = 0; i < 8; ++i)
    raw.push_back(static_cast<char>((expiry >> (8 * i)) & 0xff));
  const auto h = fnv1a64(raw, 14695981039346656037ull);
  static const char* digits = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 0; i < 8; ++i) {
    const unsigned v = static_cast<unsigned>((h >> (8 * (7 - i))) & 0xff);
    out[static_cast<std::size_t>(i) * 2] = digits[v >> 4];
    out[static_cast<std::size_t>(i) * 2 + 1] = digits[v & 0xf];
  }
  return out;
}

/// Stable 32-hex inbox id so URLs do not carry the raw x25519 pubkey.
inline std::string inbox_opaque_id(const std::string_view pubkey_hex)
{
  static const char* digits = "0123456789abcdef";
  const auto a = fnv1a64(pubkey_hex, 14695981039346656037ull);
  const auto b = fnv1a64(pubkey_hex, 0xcbf29ce484222325ull);
  std::string out(32, '0');
  for (int i = 0; i < 8; ++i) {
    const unsigned va = static_cast<unsigned>((a >> (8 * (7 - i))) & 0xff);
    const unsigned vb = static_cast<unsigned>((b >> (8 * (7 - i))) & 0xff);
    out[static_cast<std::size_t>(i) * 2] = digits[va >> 4];
    out[static_cast<std::size_t>(i) * 2 + 1] = digits[va & 0xf];
    out[16 + static_cast<std::size_t>(i) * 2] = digits[vb >> 4];
    out[16 + static_cast<std::size_t>(i) * 2 + 1] = digits[vb & 0xf];
  }
  return out;
}

inline std::string inbox_namespace(const std::string_view pubkey_hex)
{
  if (is_hex64(pubkey_hex))
    return "inbox-" + inbox_opaque_id(pubkey_hex);
  return "inbox-" + std::string{pubkey_hex};
}

inline std::string with_token(std::string path, const std::string_view token)
{
  if (token.empty())
    return path;
  path += path.find('?') == std::string::npos ? '?' : '&';
  path += "token=";
  path += url_encode(token);
  return path;
}

inline bool tokens_equal(const std::string_view a, const std::string_view b) noexcept
{
  if (a.size() != b.size())
    return false;
  unsigned char acc = 0;
  for (std::size_t i = 0; i < a.size(); ++i)
    acc = static_cast<unsigned char>(acc | (static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i])));
  return acc == 0;
}

inline bool request_token_ok(const std::string_view path, const std::string_view expected)
{
  if (expected.empty())
    return true;
  return tokens_equal(query_get(path, "token"), expected);
}

/// Next onion hop must be loopback or the same host as `--storage-url`.
inline bool hop_host_allowed(const std::string_view next_url, const std::string_view storage_url) noexcept
{
  const auto next = parse_endpoint(next_url);
  if (!next || next.tls)
    return false;
  const auto& host = next.host;
  if (host == "127.0.0.1" || host == "::1" || host == "localhost")
    return true;
  const auto storage = parse_endpoint(storage_url);
  return static_cast<bool>(storage) && storage.host == host;
}

inline std::string inbox_pubkey(const std::string_view ns)
{
  constexpr std::string_view prefix = "inbox-";
  if (ns.size() <= prefix.size() || ns.substr(0, prefix.size()) != prefix)
    return {};
  return std::string{ns.substr(prefix.size())};
}
struct HttpResult
{
  int status = 0;
  std::string body;
  std::error_code error{};
  explicit operator bool() const noexcept { return !error && status >= 200 && status < 300; }
};

std::string kv_path(std::string_view ns, std::string_view key);
std::string snodes_path(std::string_view pubkey);

/// Parse newline-separated HTTP base URLs; drop empties, TLS, and junk.
std::vector<std::string> parse_url_lines(std::string_view body);
std::string join_url_lines(const std::vector<std::string>& urls);
/// Union `existing` then `incoming`, first-seen order, cap `max_snode_urls`.
std::vector<std::string> merge_snode_urls(const std::vector<std::string>& existing,
                                          const std::vector<std::string>& incoming);

std::string format_http_request(std::string_view method, std::string_view path, std::string_view host,
                                std::string_view body);

HttpResult http_exchange(const Endpoint& endpoint, std::string_view method, std::string_view path,
                         std::string_view body, std::chrono::milliseconds timeout = std::chrono::milliseconds{2000});
} // namespace arq_storage
