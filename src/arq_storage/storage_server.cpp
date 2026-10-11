// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "storage_server.h"
#include "http_io.h"
#include "arq_messaging/swarm_map.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace arq_storage {
namespace {
std::map<std::string, std::string> parse_digest_body(const std::string_view body)
{
  std::map<std::string, std::string> out;
  std::size_t pos = 0;
  while (pos < body.size()) {
    const auto nl = body.find('\n', pos);
    const auto line = nl == std::string_view::npos ? body.substr(pos) : body.substr(pos, nl - pos);
    pos = nl == std::string_view::npos ? body.size() : nl + 1;
    if (line.empty())
      continue;
    const auto sp = line.rfind(' ');
    if (sp == std::string_view::npos || line.size() - sp - 1 != 16)
      continue;
    out.emplace(std::string{line.substr(0, sp)}, std::string{line.substr(sp + 1)});
  }
  return out;
}

bool apply_sync_frame(const std::string_view body, std::size_t& pos, std::string& key, std::uint64_t& expiry,
                      std::string& value)
{
  auto read_line = [&](std::string_view& out) -> bool {
    if (pos >= body.size())
      return false;
    const auto nl = body.find('\n', pos);
    if (nl == std::string_view::npos)
      return false;
    out = body.substr(pos, nl - pos);
    pos = nl + 1;
    return true;
  };
  std::string_view k_line;
  std::string_view e_line;
  std::string_view l_line;
  if (!read_line(k_line) || !read_line(e_line) || !read_line(l_line))
    return false;
  if (k_line.size() < 2 || k_line[0] != 'K' || k_line[1] != ' ')
    return false;
  if (e_line.size() < 2 || e_line[0] != 'E' || e_line[1] != ' ')
    return false;
  if (l_line.size() < 2 || l_line[0] != 'L' || l_line[1] != ' ')
    return false;
  key.assign(k_line.substr(2));
  expiry = static_cast<std::uint64_t>(std::strtoull(std::string{e_line.substr(2)}.c_str(), nullptr, 10));
  const auto len = static_cast<std::size_t>(std::strtoull(std::string{l_line.substr(2)}.c_str(), nullptr, 10));
  if (len > max_http_body_bytes || pos + len > body.size())
    return false;
  value.assign(body.data() + pos, len);
  pos += len;
  return true;
}
} // namespace

struct StorageServer::Impl
{
  boost::asio::io_context io;
  boost::asio::ip::tcp::acceptor acceptor{io};
  std::thread thread;
  std::thread gossip_thread;
  std::atomic<bool> running{false};
  std::atomic<int> gossip_interval_sec{15};
  std::atomic<std::uint64_t> gossip_rounds{0};
  std::atomic<std::uint64_t> digest_ok{0};
  std::atomic<std::uint64_t> sync_ok{0};
  std::atomic<std::uint64_t> membership_ok{0};
  std::atomic<std::uint64_t> gossip_fail{0};
  std::atomic<std::uint16_t> port{0};
  std::string host{"127.0.0.1"};
  std::string data_dir;
  std::string token;
  std::vector<std::string> peers;
  std::mutex mu;
  struct Stored
  {
    std::string value;
    std::uint64_t expiry = 0;
  };
  std::map<std::pair<std::string, std::string>, Stored> values;
  std::map<std::string, std::string> snodes;

  static constexpr char k_magic[4] = {'A', 'R', 'Q', '1'};
  static constexpr std::uint32_t k_max_ttl = 14 * 24 * 60 * 60;

  static std::uint64_t now_unix() { return static_cast<std::uint64_t>(std::time(nullptr)); }

  static void write_u64_le(std::ostream& out, std::uint64_t v)
  {
    for (int i = 0; i < 8; ++i)
      out.put(static_cast<char>((v >> (8 * i)) & 0xff));
  }

  static std::uint64_t read_u64_le(const std::string& raw, std::size_t off)
  {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
      v |= static_cast<std::uint64_t>(static_cast<unsigned char>(raw[off + static_cast<std::size_t>(i)])) << (8 * i);
    return v;
  }

  std::filesystem::path kv_path_on_disk(const std::string& ns, const std::string& key) const
  {
    return std::filesystem::path{data_dir} / "kv" / url_encode(ns) / url_encode(key);
  }

  std::filesystem::path snodes_path_on_disk(const std::string& pub) const
  {
    return std::filesystem::path{data_dir} / "snodes" / url_encode(pub);
  }

  void persist_kv(const std::string& ns, const std::string& key, const Stored& stored)
  {
    if (data_dir.empty())
      return;
    const auto path = kv_path_on_disk(ns, key);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    if (!out)
      return;
    out.write(k_magic, 4);
    write_u64_le(out, stored.expiry);
    out.write(stored.value.data(), static_cast<std::streamsize>(stored.value.size()));
  }

  void erase_kv_file(const std::string& ns, const std::string& key)
  {
    if (data_dir.empty())
      return;
    std::error_code ec;
    std::filesystem::remove(kv_path_on_disk(ns, key), ec);
  }

  static Stored decode_stored(std::string raw)
  {
    Stored stored;
    if (raw.size() >= 12 && std::memcmp(raw.data(), k_magic, 4) == 0) {
      stored.expiry = read_u64_le(raw, 4);
      stored.value = raw.substr(12);
    } else {
      stored.value = std::move(raw);
    }
    return stored;
  }

  bool expired(const Stored& stored) const { return stored.expiry != 0 && now_unix() >= stored.expiry; }

  void persist_snodes(const std::string& pub, const std::string& body)
  {
    if (data_dir.empty())
      return;
    const auto path = snodes_path_on_disk(pub);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    if (out)
      out.write(body.data(), static_cast<std::streamsize>(body.size()));
  }

  void replicate(const std::string& path, const std::string& body, std::vector<std::string> extra = {})
  {
    std::vector<std::string> urls;
    {
      std::lock_guard<std::mutex> lock{mu};
      urls = peers;
    }
    urls.insert(urls.end(), extra.begin(), extra.end());
    const auto self = "http://" + format_http_authority(host, port.load());
    std::set<std::string> seen;
    seen.insert(self);
    for (auto& url : urls) {
      if (!url.empty() && url.back() == '/')
        url.pop_back();
      if (url.empty() || !seen.insert(url).second)
        continue;
      const auto ep = parse_endpoint(url);
      http_exchange(ep, "PUT", with_token(path, token), body);
    }
  }

  /// Local apply (replicate=0 semantics): store + persist, no fan-out.
  bool local_put(const std::string& ns, const std::string& key, Stored stored)
  {
    std::lock_guard<std::mutex> lock{mu};
    if (expired(stored))
      return false;
    const bool inserting_new = values.find({ns, key}) == values.end();
    std::size_t ns_count = 0;
    if (inserting_new) {
      for (const auto& kv : values) {
        if (kv.first.first == ns)
          ++ns_count;
      }
    }
    if (kv_quota_exceeded(values.size(), ns_count, inserting_new))
      return false;
    values[{ns, key}] = stored;
    persist_kv(ns, key, stored);
    return true;
  }

  std::string self_url() const { return "http://" + format_http_authority(host, port.load()); }

  static std::vector<std::string> parse_pubkey_lines(const std::string_view body)
  {
    std::vector<std::string> out;
    std::string line;
    std::istringstream iss{std::string{body}};
    while (std::getline(iss, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
        line.pop_back();
      std::size_t start = 0;
      while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
        ++start;
      if (start)
        line = line.substr(start);
      if (line.empty() || line.size() > max_kv_name_bytes)
        continue;
      out.push_back(std::move(line));
      if (out.size() >= max_gossip_snode_pubs)
        break;
    }
    return out;
  }

  /// Pull/merge swarm membership lists with a peer (epidemic; replicate=0).
  std::string status_body()
  {
    std::size_t peer_n = 0;
    std::size_t snode_n = 0;
    std::size_t kv_n = 0;
    {
      std::lock_guard<std::mutex> lock{mu};
      peer_n = peers.size();
      snode_n = snodes.size();
      for (const auto& kv : values) {
        if (!expired(kv.second))
          ++kv_n;
      }
    }
    std::ostringstream out;
    out << "{\"service\":\"arqma-storage\"" << ",\"gossip_interval_sec\":" << gossip_interval_sec.load()
        << ",\"peer_count\":" << peer_n << ",\"snode_count\":" << snode_n << ",\"kv_entries\":" << kv_n
        << ",\"gossip_rounds\":" << gossip_rounds.load() << ",\"digest_ok\":" << digest_ok.load()
        << ",\"sync_ok\":" << sync_ok.load() << ",\"membership_ok\":" << membership_ok.load()
        << ",\"gossip_fail\":" << gossip_fail.load() << "}";
    return out.str();
  }

  void gossip_membership_with_peer(const std::string& peer_url)
  {
    const auto ep = parse_endpoint(peer_url);
    if (!ep)
      return;
    const auto catalog_res =
        http_exchange(ep, "GET", with_token("/v1/snodes", token), {}, std::chrono::milliseconds{2000});
    std::vector<std::string> remote_pubs;
    if (catalog_res) {
      remote_pubs = parse_pubkey_lines(catalog_res.body);
      membership_ok.fetch_add(1, std::memory_order_relaxed);
    } else {
      gossip_fail.fetch_add(1, std::memory_order_relaxed);
    }

    std::map<std::string, std::string> local_copy;
    {
      std::lock_guard<std::mutex> lock{mu};
      local_copy = snodes;
    }

    std::vector<std::string> pubs;
    std::set<std::string> seen;
    auto push_pub = [&](const std::string& pub) {
      if (pub.empty() || pub.size() > max_kv_name_bytes || !seen.insert(pub).second)
        return;
      if (pubs.size() >= max_gossip_snode_pubs)
        return;
      pubs.push_back(pub);
    };
    for (const auto& kv : local_copy)
      push_pub(kv.first);
    for (const auto& pub : remote_pubs)
      push_pub(pub);

    for (const auto& pub : pubs) {
      if (!running.load())
        return;
      const auto get_path = with_token(snodes_path(pub), token);
      const auto remote_res = http_exchange(ep, "GET", get_path, {}, std::chrono::milliseconds{2000});
      const auto remote_urls = remote_res ? parse_url_lines(remote_res.body) : std::vector<std::string>{};

      std::string merged;
      std::vector<std::string> member_urls;
      bool local_changed = false;
      {
        std::lock_guard<std::mutex> lock{mu};
        const auto it = snodes.find(pub);
        const auto existing = it == snodes.end() ? std::vector<std::string>{} : parse_url_lines(it->second);
        member_urls = merge_snode_urls(existing, remote_urls);
        merged = join_url_lines(member_urls);
        if (it == snodes.end() || it->second != merged) {
          snodes[pub] = merged;
          persist_snodes(pub, merged);
          local_changed = true;
        }
      }
      (void)local_changed;
      const std::string remote_body = remote_res ? remote_res.body : std::string{};
      if (merged != remote_body && !merged.empty()) {
        http_exchange(ep, "PUT", with_token(snodes_path(pub) + "&replicate=0", token), merged,
                      std::chrono::milliseconds{2000});
      }
    }
  }

  void gossip_with_peer(const std::string& peer_url, const std::string& ns)
  {
    const auto ep = parse_endpoint(peer_url);
    if (!ep)
      return;
    const auto digest_path = with_token("/v1/digest?ns=" + url_encode(ns), token);
    const auto digest_res = http_exchange(ep, "GET", digest_path, {}, std::chrono::milliseconds{2000});
    if (!digest_res) {
      gossip_fail.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    digest_ok.fetch_add(1, std::memory_order_relaxed);
    const auto remote = parse_digest_body(digest_res.body);

    std::map<std::string, std::string> local;
    std::map<std::string, Stored> local_entries;
    {
      std::lock_guard<std::mutex> lock{mu};
      for (const auto& kv : values) {
        if (kv.first.first != ns)
          continue;
        if (expired(kv.second))
          continue;
        local.emplace(kv.first.second, entry_digest_hex(kv.first.second, kv.second.value, kv.second.expiry));
        local_entries.emplace(kv.first.second, kv.second);
      }
    }

    std::string want;
    for (const auto& item : remote) {
      const auto it = local.find(item.first);
      if (it == local.end() || it->second != item.second) {
        want.append(item.first);
        want.push_back('\n');
      }
    }
    if (!want.empty()) {
      const auto sync_path = with_token("/v1/sync?ns=" + url_encode(ns), token);
      const auto sync_res = http_exchange(ep, "POST", sync_path, want, std::chrono::milliseconds{2000});
      if (sync_res) {
        sync_ok.fetch_add(1, std::memory_order_relaxed);
        std::size_t pos = 0;
        while (pos < sync_res.body.size()) {
          std::string key;
          std::uint64_t expiry = 0;
          std::string value;
          if (!apply_sync_frame(sync_res.body, pos, key, expiry, value))
            break;
          Stored stored;
          stored.value = std::move(value);
          stored.expiry = expiry;
          local_put(ns, key, std::move(stored));
        }
      }
    }

    for (const auto& item : local_entries) {
      if (remote.count(item.first))
        continue;
      auto path = kv_path(ns, item.first) + "&replicate=0";
      if (item.second.expiry != 0) {
        const auto now = now_unix();
        if (item.second.expiry <= now)
          continue;
        path += "&ttl=" + std::to_string(item.second.expiry - now);
      }
      http_exchange(ep, "PUT", with_token(path, token), item.second.value, std::chrono::milliseconds{2000});
    }
  }

  void gossip_once()
  {
    std::vector<std::string> peer_urls;
    std::vector<std::string> namespaces;
    {
      std::lock_guard<std::mutex> lock{mu};
      peer_urls = peers;
      std::set<std::string> ns_set;
      for (const auto& kv : values) {
        if (expired(kv.second))
          continue;
        ns_set.insert(kv.first.first);
      }
      namespaces.assign(ns_set.begin(), ns_set.end());
    }
    const auto self = self_url();
    std::size_t peer_n = 0;
    for (auto url : peer_urls) {
      if (peer_n >= max_gossip_peers)
        break;
      if (!url.empty() && url.back() == '/')
        url.pop_back();
      if (url.empty() || url == self)
        continue;
      ++peer_n;
      if (!running.load())
        return;
      gossip_membership_with_peer(url);
      std::size_t ns_n = 0;
      for (const auto& ns : namespaces) {
        if (ns_n >= max_gossip_namespaces)
          break;
        ++ns_n;
        if (!running.load())
          return;
        gossip_with_peer(url, ns);
      }
    }
    gossip_rounds.fetch_add(1, std::memory_order_relaxed);
  }

  void gossip_run()
  {
    while (running.load()) {
      const int interval = gossip_interval_sec.load();
      if (interval <= 0)
        break;
      // Gossip first, then sleep — avoids a full-interval blind window on start
      // and shrinks the race where KV lands before gossip_rounds++.
      try {
        gossip_once();
      } catch (...) {
      }
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{interval};
      while (running.load() && gossip_interval_sec.load() > 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }

  void load_disk()
  {
    if (data_dir.empty())
      return;
    std::error_code ec;
    const auto kv_root = std::filesystem::path{data_dir} / "kv";
    if (std::filesystem::exists(kv_root, ec)) {
      for (const auto& ns_ent : std::filesystem::directory_iterator{kv_root, ec}) {
        if (!ns_ent.is_directory())
          continue;
        const auto ns = url_decode(ns_ent.path().filename().string());
        for (const auto& key_ent : std::filesystem::directory_iterator{ns_ent.path(), ec}) {
          if (!key_ent.is_regular_file())
            continue;
          std::error_code size_ec;
          const auto sz = std::filesystem::file_size(key_ent.path(), size_ec);
          if (size_ec || sz > max_http_body_bytes)
            continue;
          std::ifstream in{key_ent.path(), std::ios::binary};
          std::string value((std::istreambuf_iterator<char>(in)), {});
          const auto key = url_decode(key_ent.path().filename().string());
          auto stored = decode_stored(std::move(value));
          if (expired(stored)) {
            std::filesystem::remove(key_ent.path(), ec);
            continue;
          }
          values[{ns, key}] = std::move(stored);
        }
      }
    }
    const auto sn_root = std::filesystem::path{data_dir} / "snodes";
    if (std::filesystem::exists(sn_root, ec)) {
      for (const auto& ent : std::filesystem::directory_iterator{sn_root, ec}) {
        if (!ent.is_regular_file())
          continue;
        std::error_code size_ec;
        const auto sz = std::filesystem::file_size(ent.path(), size_ec);
        if (size_ec || sz > max_http_body_bytes)
          continue;
        std::ifstream in{ent.path(), std::ios::binary};
        std::string body((std::istreambuf_iterator<char>(in)), {});
        snodes[url_decode(ent.path().filename().string())] = std::move(body);
      }
    }
  }

  std::string handle(const std::string& method, const std::string& path, const std::string& body)
  {
    const auto path_only = path.substr(0, path.find('?'));
    if (method == "GET" && (path_only == "/" || path_only == "/status")) {
      const auto body = status_body();
      return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
             "\r\nConnection: close\r\n\r\n" + body;
    }
    if (!request_token_ok(path, token))
      return "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    {
      std::lock_guard<std::mutex> lock{mu};
      std::vector<std::pair<std::string, std::string>> drop;
      for (const auto& kv : values) {
        if (expired(kv.second))
          drop.push_back(kv.first);
      }
      for (const auto& item : drop) {
        erase_kv_file(item.first, item.second);
        values.erase(item);
      }
    }

    if (path_only == "/v1/kv") {
      const auto ns = query_get(path, "ns");
      const auto key = query_get(path, "key");
      if (ns.empty() || key.empty() || ns.size() > max_kv_name_bytes || key.size() > max_kv_name_bytes)
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      if (method == "PUT") {
        Stored stored;
        stored.value = body;
        std::uint32_t ttl = 0;
        const auto ttl_raw = query_get(path, "ttl");
        if (!ttl_raw.empty()) {
          ttl = static_cast<std::uint32_t>(std::strtoul(ttl_raw.c_str(), nullptr, 10));
          if (ttl > k_max_ttl)
            ttl = k_max_ttl;
          if (ttl != 0)
            stored.expiry = now_unix() + ttl;
        }
        std::vector<std::string> swarm_urls;
        {
          std::lock_guard<std::mutex> lock{mu};
          const bool inserting_new = values.find({ns, key}) == values.end();
          std::size_t ns_count = 0;
          if (inserting_new) {
            for (const auto& kv : values) {
              if (kv.first.first == ns)
                ++ns_count;
            }
          }
          if (kv_quota_exceeded(values.size(), ns_count, inserting_new))
            return "HTTP/1.1 507 Insufficient Storage\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
          values[{ns, key}] = stored;
          persist_kv(ns, key, stored);
          const auto pub = inbox_pubkey(ns);
          if (!pub.empty()) {
            const auto it = snodes.find(pub);
            if (it != snodes.end())
              swarm_urls = parse_url_lines(it->second);
          }
        }
        if (query_get(path, "replicate") != "0") {
          auto replica_path = kv_path(ns, key) + "&replicate=0";
          if (ttl != 0)
            replica_path += "&ttl=" + std::to_string(ttl);
          replicate(replica_path, body, swarm_urls);
        }
        return "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      }
      if (method == "GET") {
        std::lock_guard<std::mutex> lock{mu};
        const auto it = values.find({ns, key});
        if (it == values.end())
          return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        if (expired(it->second)) {
          erase_kv_file(ns, key);
          values.erase(it);
          return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        }
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\nContent-Length: " << it->second.value.size() << "\r\nConnection: close\r\n\r\n"
            << it->second.value;
        return oss.str();
      }
    }

    if (path_only == "/v1/snodes") {
      const auto pub = query_get(path, "pubkey");
      if (pub.empty()) {
        if (method == "GET") {
          std::string payload;
          {
            std::lock_guard<std::mutex> lock{mu};
            std::size_t n = 0;
            for (const auto& kv : snodes) {
              if (n >= max_gossip_snode_pubs)
                break;
              payload.append(kv.first);
              payload.push_back('\n');
              ++n;
            }
          }
          std::ostringstream oss;
          oss << "HTTP/1.1 200 OK\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n" << payload;
          return oss.str();
        }
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      }
      if (method == "PUT") {
        std::string merged;
        std::vector<std::string> member_urls;
        {
          std::lock_guard<std::mutex> lock{mu};
          const auto it = snodes.find(pub);
          const auto existing = it == snodes.end() ? std::vector<std::string>{} : parse_url_lines(it->second);
          member_urls = merge_snode_urls(existing, parse_url_lines(body));
          merged = join_url_lines(member_urls);
          snodes[pub] = merged;
          persist_snodes(pub, merged);
        }
        if (query_get(path, "replicate") != "0")
          replicate(snodes_path(pub) + "&replicate=0", merged, member_urls);
        return "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      }
      if (method == "GET") {
        std::lock_guard<std::mutex> lock{mu};
        const auto it = snodes.find(pub);
        const std::string& payload = it == snodes.end() ? std::string{} : it->second;
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n" << payload;
        return oss.str();
      }
    }

    if (path_only == "/v1/list" && method == "GET") {
      const auto ns = query_get(path, "ns");
      if (ns.empty())
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      std::string payload;
      std::vector<std::pair<std::string, std::string>> drop;
      {
        std::lock_guard<std::mutex> lock{mu};
        for (const auto& kv : values) {
          if (kv.first.first != ns)
            continue;
          if (expired(kv.second)) {
            drop.push_back(kv.first);
            continue;
          }
          payload.append(kv.first.second);
          payload.push_back('\n');
        }
        for (const auto& item : drop) {
          erase_kv_file(item.first, item.second);
          values.erase(item);
        }
      }
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n" << payload;
      return oss.str();
    }

    if (path_only == "/v1/swarm" && method == "GET") {
      const auto pub = query_get(path, "pubkey");
      if (pub.empty())
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      std::string members;
      {
        std::lock_guard<std::mutex> lock{mu};
        const auto it = snodes.find(pub);
        if (it != snodes.end())
          members = it->second;
      }
      std::ostringstream oss;
      oss << arq_messaging::hash_pubkey_to_swarm(pub) << '\n' << members;
      const auto payload = oss.str();
      std::ostringstream http;
      http << "HTTP/1.1 200 OK\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n" << payload;
      return http.str();
    }

    if (path_only == "/v1/digest" && method == "GET") {
      const auto ns = query_get(path, "ns");
      if (ns.empty() || ns.size() > max_kv_name_bytes)
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      std::string payload;
      std::vector<std::pair<std::string, std::string>> drop;
      {
        std::lock_guard<std::mutex> lock{mu};
        for (const auto& kv : values) {
          if (kv.first.first != ns)
            continue;
          if (expired(kv.second)) {
            drop.push_back(kv.first);
            continue;
          }
          payload.append(kv.first.second);
          payload.push_back(' ');
          payload.append(entry_digest_hex(kv.first.second, kv.second.value, kv.second.expiry));
          payload.push_back('\n');
        }
        for (const auto& item : drop) {
          erase_kv_file(item.first, item.second);
          values.erase(item);
        }
      }
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n" << payload;
      return oss.str();
    }

    if (path_only == "/v1/sync" && method == "POST") {
      const auto ns = query_get(path, "ns");
      if (ns.empty() || ns.size() > max_kv_name_bytes)
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      std::vector<std::string> keys;
      {
        std::size_t pos = 0;
        while (pos < body.size()) {
          const auto nl = body.find('\n', pos);
          const auto line = nl == std::string::npos ? body.substr(pos) : body.substr(pos, nl - pos);
          pos = nl == std::string::npos ? body.size() : nl + 1;
          if (!line.empty() && line.size() <= max_kv_name_bytes)
            keys.push_back(line);
        }
      }
      std::string payload;
      {
        std::lock_guard<std::mutex> lock{mu};
        for (const auto& key : keys) {
          if (payload.size() >= max_sync_response_bytes)
            break;
          const auto it = values.find({ns, key});
          if (it == values.end())
            continue;
          if (expired(it->second)) {
            erase_kv_file(ns, key);
            values.erase(it);
            continue;
          }
          const auto& stored = it->second;
          if (payload.size() + key.size() + stored.value.size() + 64 > max_sync_response_bytes && !payload.empty())
            break;
          payload.append("K ");
          payload.append(key);
          payload.push_back('\n');
          payload.append("E ");
          payload.append(std::to_string(stored.expiry));
          payload.push_back('\n');
          payload.append("L ");
          payload.append(std::to_string(stored.value.size()));
          payload.push_back('\n');
          payload.append(stored.value);
        }
      }
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n" << payload;
      return oss.str();
    }

    return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  }

  void serve(boost::asio::ip::tcp::socket socket)
  {
    try {
      boost::asio::streambuf buf{max_http_header_bytes};
      boost::system::error_code ec;
      boost::asio::read_until(socket, buf, "\r\n\r\n", ec);
      if (ec)
        return;
      std::istream is{&buf};
      std::string request_line;
      std::getline(is, request_line);
      if (!request_line.empty() && request_line.back() == '\r')
        request_line.pop_back();
      std::string method;
      std::string path;
      {
        std::istringstream ls{request_line};
        std::string ver;
        ls >> method >> path >> ver;
      }
      std::size_t content_length = 0;
      std::string header;
      while (std::getline(is, header)) {
        if (!header.empty() && header.back() == '\r')
          header.pop_back();
        if (header.empty())
          break;
        const auto colon = header.find(':');
        if (colon == std::string::npos)
          continue;
        std::string name = header.substr(0, colon);
        for (char& c : name)
          c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        if (name == "content-length")
          content_length = static_cast<std::size_t>(std::strtoul(header.c_str() + colon + 1, nullptr, 10));
      }
      if (content_length > max_http_body_bytes) {
        const char* too_large = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        boost::asio::write(socket, boost::asio::buffer(too_large, std::strlen(too_large)), ec);
        return;
      }
      std::string body(std::istreambuf_iterator<char>(is), {});
      if (content_length > body.size()) {
        std::string rest(content_length - body.size(), '\0');
        boost::asio::read(socket, boost::asio::buffer(rest), boost::asio::transfer_exactly(rest.size()), ec);
        if (!ec)
          body.append(rest);
      }
      const auto response = handle(method, path, body);
      boost::asio::write(socket, boost::asio::buffer(response), ec);
    } catch (...) {
    }
  }

  void run()
  {
    while (running.load()) {
      boost::system::error_code ec;
      boost::asio::ip::tcp::socket socket{io};
      acceptor.accept(socket, ec);
      if (!running.load())
        break;
      if (ec)
        continue;
      serve(std::move(socket));
    }
  }
};

StorageServer::StorageServer() : impl_(std::make_unique<Impl>()) {}

StorageServer::~StorageServer()
{
  stop();
}

std::error_code StorageServer::listen(const std::string& host, const std::uint16_t port)
{
  stop();
  try {
    impl_->load_disk();
    const auto addr = boost::asio::ip::make_address(host);
    boost::asio::ip::tcp::endpoint ep{addr, port};
    impl_->acceptor = boost::asio::ip::tcp::acceptor{impl_->io};
    impl_->acceptor.open(ep.protocol());
    impl_->acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    impl_->acceptor.bind(ep);
    impl_->acceptor.listen();
    impl_->port = impl_->acceptor.local_endpoint().port();
    impl_->host = host;
    impl_->running = true;
    impl_->thread = std::thread([this] { impl_->run(); });
    if (impl_->gossip_interval_sec.load() > 0)
      impl_->gossip_thread = std::thread([this] { impl_->gossip_run(); });
    return {};
  } catch (...) {
    return std::make_error_code(std::errc::address_in_use);
  }
}

void StorageServer::stop()
{
  if (!impl_)
    return;
  // Closing the acceptor alone does not reliably unblock a synchronous
  // accept() on another thread (join then hangs). Set running=false and
  // self-connect first so accept() returns, then tear down.
  const auto port = impl_->port.load();
  const std::string host = impl_->host;
  impl_->running = false;
  if (port != 0) {
    try {
      boost::asio::io_context wake_io;
      boost::asio::ip::tcp::socket wake{wake_io};
      boost::system::error_code wake_ec;
      wake.connect({boost::asio::ip::make_address(host), port}, wake_ec);
      wake.close(wake_ec);
    } catch (...) {
    }
  }
  boost::system::error_code ec;
  impl_->acceptor.cancel(ec);
  impl_->acceptor.close(ec);
  impl_->io.stop();
  if (impl_->thread.joinable())
    impl_->thread.join();
  if (impl_->gossip_thread.joinable())
    impl_->gossip_thread.join();
  impl_->io.restart();
  impl_->port = 0;
}

std::uint16_t StorageServer::port() const noexcept
{
  return impl_ ? impl_->port.load() : 0;
}

bool StorageServer::running() const noexcept
{
  return impl_ && impl_->running.load();
}

std::string StorageServer::base_url() const
{
  if (!running())
    return {};
  return "http://" + format_http_authority(impl_->host, port());
}

void StorageServer::set_data_dir(std::string path)
{
  impl_->data_dir = std::move(path);
}

const std::string& StorageServer::data_dir() const noexcept
{
  static const std::string empty;
  return impl_ ? impl_->data_dir : empty;
}

void StorageServer::add_peer(std::string base_url)
{
  if (!base_url.empty() && base_url.back() == '/')
    base_url.pop_back();
  if (base_url.empty() || !parse_endpoint(base_url))
    return;
  std::lock_guard<std::mutex> lock{impl_->mu};
  if (std::find(impl_->peers.begin(), impl_->peers.end(), base_url) != impl_->peers.end())
    return;
  impl_->peers.push_back(std::move(base_url));
}

void StorageServer::set_token(std::string token)
{
  impl_->token = std::move(token);
}

void StorageServer::set_gossip_interval(std::chrono::seconds interval)
{
  const int sec = interval.count() < 0 ? 0 : static_cast<int>(interval.count());
  impl_->gossip_interval_sec.store(sec);
  if (sec > 0 && impl_->running.load() && !impl_->gossip_thread.joinable())
    impl_->gossip_thread = std::thread([this] { impl_->gossip_run(); });
}
} // namespace arq_storage
