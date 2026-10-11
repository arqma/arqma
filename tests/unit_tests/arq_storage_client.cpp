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

#include "gtest/gtest.h"

#include "arq_messaging/swarm_map.hpp"
#include "arq_storage/http_io.h"
#include "arq_storage/storage_client.h"
#include "arq_storage/storage_endpoint.h"
#include "arq_storage/storage_server.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <thread>

TEST(arq_storage_endpoint, parses_http_and_https_urls)
{
  const auto http = arq_storage::parse_endpoint("http://127.0.0.1:22021/status");
  ASSERT_TRUE(http);
  EXPECT_FALSE(http.tls);
  EXPECT_EQ("127.0.0.1", http.host);
  EXPECT_EQ(22021, http.port);
  EXPECT_EQ("/status", http.path);

  const auto https = arq_storage::parse_endpoint("https://storage.example");
  ASSERT_TRUE(https);
  EXPECT_TRUE(https.tls);
  EXPECT_EQ("storage.example", https.host);
  EXPECT_EQ(443, https.port);
  EXPECT_EQ("/", https.path);
}

TEST(arq_storage_endpoint, rejects_invalid_urls)
{
  EXPECT_FALSE(arq_storage::parse_endpoint(""));
  EXPECT_FALSE(arq_storage::parse_endpoint("ftp://127.0.0.1:1"));
  EXPECT_FALSE(arq_storage::parse_endpoint("http://"));
  EXPECT_FALSE(arq_storage::parse_endpoint("http://host:99999"));
}

TEST(arq_storage_endpoint, parses_ipv6_and_listen_addresses)
{
  const auto ep = arq_storage::parse_endpoint("http://[::1]:22021/status");
  ASSERT_TRUE(ep);
  EXPECT_EQ("::1", ep.host);
  EXPECT_EQ(22021, ep.port);
  EXPECT_EQ("/status", ep.path);
  EXPECT_EQ("[::1]:22021", arq_storage::format_http_authority(ep.host, ep.port));
  EXPECT_EQ("[::1]:22021", arq_storage::http_host_header(ep));

  std::string host;
  std::uint16_t port = 0;
  ASSERT_TRUE(arq_storage::parse_listen_address("[::1]:22021", host, port));
  EXPECT_EQ("::1", host);
  EXPECT_EQ(22021, port);
  EXPECT_TRUE(arq_storage::parse_listen_address("127.0.0.1:0", host, port));
  EXPECT_EQ(0, port);
  EXPECT_FALSE(arq_storage::parse_listen_address("127.0.0.1", host, port));
  EXPECT_FALSE(arq_storage::parse_listen_address("bad", host, port));
}

TEST(arq_storage_endpoint, formats_http_get_request)
{
  const auto ep = arq_storage::parse_endpoint("http://127.0.0.1:22021/status");
  ASSERT_TRUE(ep);
  const auto req = arq_storage::format_http_get_request(ep);
  EXPECT_NE(std::string::npos, req.find("GET /status HTTP/1.1\r\n"));
  EXPECT_NE(std::string::npos, req.find("Host: 127.0.0.1:22021\r\n"));
  EXPECT_NE(std::string::npos, req.find("Connection: close\r\n\r\n"));
}

TEST(arq_storage_client, remote_without_url_is_not_connected)
{
  arq_storage::StorageClient client{{arq_storage::Backend::Remote, "", std::chrono::milliseconds{100}}};
  EXPECT_EQ(std::make_error_code(std::errc::not_connected), client.ping());
}

TEST(arq_storage_client, remote_with_closed_port_is_not_connected)
{
  // Port 1 is almost never accepting connections on developer workstations.
  arq_storage::Config cfg{arq_storage::Backend::Remote, "http://127.0.0.1:1", std::chrono::milliseconds{200}};
  arq_storage::StorageClient client{cfg};
  ASSERT_TRUE(client.endpoint());
  EXPECT_EQ(std::make_error_code(std::errc::not_connected), client.ping());
}

TEST(arq_storage_client, in_memory_store_retrieve_roundtrip)
{
  arq_storage::StorageClient client{{arq_storage::Backend::InMemory, "", std::chrono::milliseconds{100}}};
  EXPECT_FALSE(client.ping());
  EXPECT_FALSE(client.store({"messages", "hello", "world"}));

  const auto retrieve = client.retrieve("messages", "hello");
  EXPECT_FALSE(retrieve.error);
  EXPECT_EQ("world", retrieve.value);

  client.set_snodes_for_pubkey("pk", {"sn-a", "sn-b"});
  const auto snodes = client.get_snodes_for_pubkey("pk");
  ASSERT_FALSE(snodes.error);
  ASSERT_EQ(2u, snodes.value.size());
  EXPECT_EQ("sn-a", snodes.value[0]);
}

TEST(arq_storage_client, daemon_client_config_roundtrip)
{
  arq_storage::configure_daemon_client({arq_storage::Backend::InMemory, "", std::chrono::milliseconds{50}});
  auto client = arq_storage::daemon_client();
  EXPECT_EQ(arq_storage::Backend::InMemory, client.backend());
  EXPECT_FALSE(client.ping());
  arq_storage::configure_daemon_client({});
}

TEST(arq_storage_server, remote_client_http_roundtrip)
{
  arq_storage::StorageServer server;
  ASSERT_FALSE(server.listen("127.0.0.1", 0)) << "bind ephemeral storage port";
  ASSERT_NE(0, server.port());
  arq_storage::Config cfg;
  cfg.backend = arq_storage::Backend::Remote;
  cfg.base_url = server.base_url();
  cfg.connect_timeout = std::chrono::milliseconds{2000};
  arq_storage::StorageClient client{cfg};
  EXPECT_FALSE(client.ping());
  EXPECT_FALSE(client.store({"messages", "hello", "world"}));
  const auto got = client.retrieve("messages", "hello");
  EXPECT_FALSE(got.error);
  EXPECT_EQ("world", got.value);
  const auto keys = client.list_keys("messages");
  ASSERT_FALSE(keys.error);
  ASSERT_EQ(1u, keys.value.size());
  EXPECT_EQ("hello", keys.value[0]);
  client.set_snodes_for_pubkey("pk", {"http://127.0.0.1:22021", "http://127.0.0.1:22022"});
  const auto snodes = client.get_snodes_for_pubkey("pk");
  ASSERT_FALSE(snodes.error);
  ASSERT_EQ(2u, snodes.value.size());
  EXPECT_EQ("http://127.0.0.1:22021", snodes.value[0]);
  server.stop();
  EXPECT_FALSE(server.running());
}

TEST(arq_storage_server, persists_kv_across_restart)
{
  const auto dir = std::filesystem::temp_directory_path() /
                   ("arq-storage-ut-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  {
    arq_storage::StorageServer server;
    server.set_data_dir(dir.string());
    ASSERT_FALSE(server.listen("127.0.0.1", 0));
    arq_storage::Config cfg{arq_storage::Backend::Remote, server.base_url(), std::chrono::milliseconds{2000}};
    arq_storage::StorageClient client{cfg};
    ASSERT_FALSE(client.store({"ns", "k", "durable"}));
    server.stop();
  }
  {
    arq_storage::StorageServer server;
    server.set_data_dir(dir.string());
    ASSERT_FALSE(server.listen("127.0.0.1", 0));
    arq_storage::Config cfg{arq_storage::Backend::Remote, server.base_url(), std::chrono::milliseconds{2000}};
    arq_storage::StorageClient client{cfg};
    const auto got = client.retrieve("ns", "k");
    EXPECT_FALSE(got.error);
    EXPECT_EQ("durable", got.value);
    server.stop();
  }
  std::filesystem::remove_all(dir);
}

TEST(arq_storage_server, replicates_put_to_peer)
{
  arq_storage::StorageServer primary;
  arq_storage::StorageServer replica;
  ASSERT_FALSE(primary.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  primary.add_peer(replica.base_url());
  arq_storage::Config cfg{arq_storage::Backend::Remote, primary.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient writer{cfg};
  ASSERT_FALSE(writer.store({"swarm", "k", "copy"}));
  arq_storage::Config replica_cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient reader{replica_cfg};
  const auto got = reader.retrieve("swarm", "k");
  EXPECT_FALSE(got.error);
  EXPECT_EQ("copy", got.value);
  primary.stop();
  replica.stop();
}

TEST(arq_storage_server, honors_ttl)
{
  arq_storage::StorageServer server;
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  arq_storage::Config cfg{arq_storage::Backend::Remote, server.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient client{cfg};
  arq_storage::StoreRequest req{"messages", "ephemeral", "gone"};
  req.ttl_seconds = 1;
  ASSERT_FALSE(client.store(req));
  const auto got = client.retrieve("messages", "ephemeral");
  EXPECT_FALSE(got.error);
  EXPECT_EQ("gone", got.value);
  std::this_thread::sleep_for(std::chrono::seconds{2});
  const auto expired = client.retrieve("messages", "ephemeral");
  EXPECT_TRUE(expired.error);
  const auto keys = client.list_keys("messages");
  ASSERT_FALSE(keys.error);
  EXPECT_TRUE(keys.value.empty());
  server.stop();
}

TEST(arq_storage_server, loads_legacy_kv_without_magic)
{
  const auto dir =
      std::filesystem::temp_directory_path() /
      ("arq-storage-legacy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir / "kv" / "ns");
  {
    std::ofstream out{dir / "kv" / "ns" / "k", std::ios::binary};
    out << "legacy";
  }
  arq_storage::StorageServer server;
  server.set_data_dir(dir.string());
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  arq_storage::Config cfg{arq_storage::Backend::Remote, server.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient client{cfg};
  const auto got = client.retrieve("ns", "k");
  EXPECT_FALSE(got.error);
  EXPECT_EQ("legacy", got.value);
  server.stop();
  std::filesystem::remove_all(dir);
}

TEST(arq_storage_server, fans_inbox_put_to_swarm_members)
{
  arq_storage::StorageServer primary;
  arq_storage::StorageServer replica;
  ASSERT_FALSE(primary.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  arq_storage::Config cfg{arq_storage::Backend::Remote, primary.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient writer{cfg};
  writer.set_snodes_for_pubkey("pk", {replica.base_url()});
  ASSERT_FALSE(writer.store({"inbox-pk", "k", "copy"}));
  arq_storage::Config replica_cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient reader{replica_cfg};
  const auto got = reader.retrieve("inbox-pk", "k");
  EXPECT_FALSE(got.error);
  EXPECT_EQ("copy", got.value);
  primary.stop();
  replica.stop();
}

TEST(arq_storage_server, retrieve_falls_back_to_swarm_member)
{
  arq_storage::StorageServer directory;
  arq_storage::StorageServer replica;
  ASSERT_FALSE(directory.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  arq_storage::Config replica_cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient replica_writer{replica_cfg};
  ASSERT_FALSE(replica_writer.store({"inbox-pk", "k", "hidden"}));
  arq_storage::Config dir_cfg{arq_storage::Backend::Remote, directory.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient reader{dir_cfg};
  reader.set_snodes_for_pubkey("pk", {replica.base_url()});
  const auto got = reader.retrieve("inbox-pk", "k");
  EXPECT_FALSE(got.error);
  EXPECT_EQ("hidden", got.value);
  directory.stop();
  replica.stop();
}

TEST(arq_storage_server, list_keys_merges_swarm_members)
{
  arq_storage::StorageServer directory;
  arq_storage::StorageServer replica;
  ASSERT_FALSE(directory.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  arq_storage::Config replica_cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient replica_writer{replica_cfg};
  ASSERT_FALSE(replica_writer.store({"inbox-pk", "only-replica", "x"}));
  arq_storage::Config dir_cfg{arq_storage::Backend::Remote, directory.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient reader{dir_cfg};
  reader.set_snodes_for_pubkey("pk", {replica.base_url()});
  ASSERT_FALSE(reader.store({"inbox-pk", "on-dir", "y"}));
  const auto keys = reader.list_keys("inbox-pk");
  ASSERT_FALSE(keys.error);
  ASSERT_EQ(2u, keys.value.size());
  EXPECT_EQ("on-dir", keys.value[0]);
  EXPECT_EQ("only-replica", keys.value[1]);
  directory.stop();
  replica.stop();
}

TEST(arq_storage_server, swarm_status_lists_members)
{
  arq_storage::StorageServer server;
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  arq_storage::Config cfg{arq_storage::Backend::Remote, server.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient client{cfg};
  client.set_snodes_for_pubkey("alice", {"http://127.0.0.1:1"});
  const auto ep = arq_storage::parse_endpoint(server.base_url());
  const auto got = arq_storage::http_exchange(ep, "GET", "/v1/swarm?pubkey=alice", {});
  ASSERT_TRUE(got);
  const auto expected = std::to_string(arq_messaging::hash_pubkey_to_swarm("alice"));
  EXPECT_EQ(0u, got.body.find(expected + "\n"));
  EXPECT_NE(std::string::npos, got.body.find("http://127.0.0.1:1"));
  server.stop();
}

TEST(arq_storage_server, merge_snode_urls_uniques_and_cap)
{
  const auto merged = arq_storage::merge_snode_urls({"http://127.0.0.1:22021/", "https://example.com:443"},
                                                    {"http://127.0.0.1:22021", "http://127.0.0.1:22022", "not-a-url"});
  ASSERT_EQ(2u, merged.size());
  EXPECT_EQ("http://127.0.0.1:22021", merged[0]);
  EXPECT_EQ("http://127.0.0.1:22022", merged[1]);
  std::vector<std::string> many;
  many.reserve(arq_storage::max_snode_urls + 4);
  for (std::size_t i = 0; i < arq_storage::max_snode_urls + 4; ++i)
    many.push_back("http://127.0.0.1:" + std::to_string(20000 + i));
  EXPECT_EQ(arq_storage::max_snode_urls, arq_storage::merge_snode_urls({}, many).size());
}

TEST(arq_storage_server, merges_snode_lists_on_put)
{
  arq_storage::StorageServer server;
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  arq_storage::Config cfg{arq_storage::Backend::Remote, server.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient client{cfg};
  client.set_snodes_for_pubkey("pk", {"http://127.0.0.1:22021"});
  client.set_snodes_for_pubkey("pk", {"http://127.0.0.1:22022"});
  const auto members = client.get_snodes_for_pubkey("pk");
  ASSERT_FALSE(members.error);
  ASSERT_EQ(2u, members.value.size());
  EXPECT_EQ("http://127.0.0.1:22021", members.value[0]);
  EXPECT_EQ("http://127.0.0.1:22022", members.value[1]);
  const auto swarm = client.get_swarm("pk");
  ASSERT_FALSE(swarm.error);
  EXPECT_EQ(arq_messaging::hash_pubkey_to_swarm("pk"), swarm.value.first);
  EXPECT_EQ(members.value, swarm.value.second);
  server.stop();
}

TEST(arq_storage_server, gossips_snode_list_to_members)
{
  arq_storage::StorageServer directory;
  arq_storage::StorageServer replica;
  ASSERT_FALSE(directory.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  arq_storage::Config cfg{arq_storage::Backend::Remote, directory.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient writer{cfg};
  writer.set_snodes_for_pubkey("pk", {replica.base_url(), "http://127.0.0.1:9"});
  arq_storage::Config replica_cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient reader{replica_cfg};
  const auto members = reader.get_snodes_for_pubkey("pk");
  ASSERT_FALSE(members.error);
  ASSERT_EQ(2u, members.value.size());
  EXPECT_EQ(replica.base_url(), members.value[0]);
  EXPECT_EQ("http://127.0.0.1:9", members.value[1]);
  directory.stop();
  replica.stop();
}

TEST(arq_storage_server, gossips_kv_via_anti_entropy)
{
  arq_storage::StorageServer primary;
  arq_storage::StorageServer replica;
  primary.set_gossip_interval(std::chrono::seconds{0});
  replica.set_gossip_interval(std::chrono::seconds{1});
  ASSERT_FALSE(primary.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  primary.add_peer(replica.base_url());
  replica.add_peer(primary.base_url());

  const auto primary_ep = arq_storage::parse_endpoint(primary.base_url());
  const auto replica_ep = arq_storage::parse_endpoint(replica.base_url());
  ASSERT_TRUE(primary_ep);
  ASSERT_TRUE(replica_ep);
  // Seed the namespace on B so gossip has an ns to digest/pull against A.
  ASSERT_TRUE(arq_storage::http_exchange(replica_ep, "PUT", "/v1/kv?ns=gossip&key=_seed&replicate=0", "1"));
  // replicate=0 so only anti-entropy gossip propagates the payload.
  ASSERT_TRUE(arq_storage::http_exchange(primary_ep, "PUT", "/v1/kv?ns=gossip&key=payload&replicate=0", "from-a"));

  const auto digest = arq_storage::http_exchange(primary_ep, "GET", "/v1/digest?ns=gossip", {});
  ASSERT_TRUE(digest);
  EXPECT_NE(std::string::npos, digest.body.find("payload "));
  const auto expected = arq_storage::entry_digest_hex("payload", "from-a", 0);
  EXPECT_NE(std::string::npos, digest.body.find(expected));

  // Payload can land mid-gossip_once before gossip_rounds++ — wait for both.
  bool converged = false;
  arq_storage::StatusSnapshot snap{};
  for (int i = 0; i < 60; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    arq_storage::Config cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
    arq_storage::StorageClient reader{cfg};
    const auto got = reader.retrieve("gossip", "payload");
    snap = reader.fetch_status();
    if (!got.error && got.value == "from-a" && snap.reachable && snap.gossip_rounds >= 1u && snap.digest_ok >= 1u &&
        snap.sync_ok >= 1u) {
      converged = true;
      break;
    }
  }
  EXPECT_TRUE(converged);
  EXPECT_TRUE(snap.reachable);
  EXPECT_GE(snap.gossip_rounds, 1u);
  EXPECT_GE(snap.digest_ok, 1u);
  EXPECT_GE(snap.sync_ok, 1u);
  primary.stop();
  replica.stop();
}

TEST(arq_storage_server, gossips_snode_membership_via_anti_entropy)
{
  arq_storage::StorageServer primary;
  arq_storage::StorageServer replica;
  primary.set_gossip_interval(std::chrono::seconds{0});
  replica.set_gossip_interval(std::chrono::seconds{1});
  ASSERT_FALSE(primary.listen("127.0.0.1", 0));
  ASSERT_FALSE(replica.listen("127.0.0.1", 0));
  primary.add_peer(replica.base_url());
  replica.add_peer(primary.base_url());

  const auto primary_ep = arq_storage::parse_endpoint(primary.base_url());
  ASSERT_TRUE(primary_ep);
  const std::string members = replica.base_url() + "\nhttp://127.0.0.1:9\n";
  // replicate=0: only epidemic membership gossip should teach B about pubkey pk.
  ASSERT_TRUE(arq_storage::http_exchange(primary_ep, "PUT", "/v1/snodes?pubkey=pk&replicate=0", members));

  const auto catalog = arq_storage::http_exchange(primary_ep, "GET", "/v1/snodes", {});
  ASSERT_TRUE(catalog);
  EXPECT_NE(std::string::npos, catalog.body.find("pk"));

  // Membership can land mid-gossip_once before gossip_rounds++ — wait for both.
  bool converged = false;
  arq_storage::StatusSnapshot snap{};
  for (int i = 0; i < 60; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    arq_storage::Config cfg{arq_storage::Backend::Remote, replica.base_url(), std::chrono::milliseconds{2000}};
    arq_storage::StorageClient reader{cfg};
    const auto got = reader.get_snodes_for_pubkey("pk");
    snap = reader.fetch_status();
    if (!got.error && got.value.size() == 2u && got.value[0] == replica.base_url() &&
        got.value[1] == "http://127.0.0.1:9" && snap.reachable && snap.gossip_rounds >= 1u &&
        snap.membership_ok >= 1u) {
      converged = true;
      break;
    }
  }
  EXPECT_TRUE(converged);
  EXPECT_TRUE(snap.reachable);
  EXPECT_GE(snap.gossip_rounds, 1u);
  EXPECT_GE(snap.membership_ok, 1u);
  primary.stop();
  replica.stop();
}

TEST(arq_storage_server, rejects_oversized_content_length)
{
  arq_storage::StorageServer server;
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  boost::asio::io_context io;
  boost::asio::ip::tcp::socket socket{io};
  socket.connect(boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address("127.0.0.1"), server.port()});
  const std::string req =
      "PUT /v1/kv?ns=n&key=k HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 2000000\r\nConnection: close\r\n\r\n";
  boost::asio::write(socket, boost::asio::buffer(req));
  boost::asio::streambuf buf;
  boost::asio::read_until(socket, buf, "\r\n");
  std::istream is{&buf};
  std::string line;
  std::getline(is, line);
  EXPECT_NE(std::string::npos, line.find("413"));
  server.stop();
}

TEST(arq_storage_server, token_protects_kv_and_status_stays_open)
{
  arq_storage::StorageServer server;
  server.set_token("stack-secret");
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  const auto ep = arq_storage::parse_endpoint(server.base_url());
  const auto status = arq_storage::http_exchange(ep, "GET", "/status", {});
  ASSERT_TRUE(status);
  EXPECT_NE(std::string::npos, status.body.find("arqma-storage"));
  EXPECT_NE(std::string::npos, status.body.find("\"peer_count\":"));
  EXPECT_NE(std::string::npos, status.body.find("\"gossip_interval_sec\":"));
  const auto denied = arq_storage::http_exchange(ep, "PUT", "/v1/kv?ns=n&key=k", "x");
  EXPECT_EQ(401, denied.status);
  const auto ok =
      arq_storage::http_exchange(ep, "PUT", arq_storage::with_token("/v1/kv?ns=n&key=k", "stack-secret"), "x");
  EXPECT_TRUE(ok);
  server.stop();
}

TEST(arq_storage_server, status_reports_peers_and_kv)
{
  arq_storage::StorageServer server;
  server.set_gossip_interval(std::chrono::seconds{0});
  ASSERT_FALSE(server.listen("127.0.0.1", 0));
  server.add_peer("http://127.0.0.1:9");
  const auto ep = arq_storage::parse_endpoint(server.base_url());
  ASSERT_TRUE(arq_storage::http_exchange(ep, "PUT", "/v1/kv?ns=n&key=k", "v"));
  ASSERT_TRUE(arq_storage::http_exchange(ep, "PUT", "/v1/snodes?pubkey=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                         "http://127.0.0.1:9\n"));

  arq_storage::StorageClient client{{arq_storage::Backend::Remote, server.base_url()}};
  const auto snap = client.fetch_status();
  EXPECT_TRUE(snap.reachable);
  EXPECT_EQ("arqma-storage", snap.service);
  EXPECT_EQ(0u, snap.gossip_interval_sec);
  EXPECT_EQ(1u, snap.peer_count);
  EXPECT_EQ(1u, snap.snode_count);
  EXPECT_GE(snap.kv_entries, 1u);
  EXPECT_EQ(0u, snap.gossip_rounds);
  EXPECT_EQ(0u, snap.digest_ok);
  EXPECT_EQ(0u, snap.sync_ok);
  EXPECT_EQ(0u, snap.membership_ok);
  EXPECT_EQ(0u, snap.gossip_fail);
  server.stop();
}

TEST(arq_storage_http, inbox_namespace_hides_pubkey_and_hop_allowlist)
{
  const std::string hex(64, 'a');
  const auto ns = arq_storage::inbox_namespace(hex);
  EXPECT_EQ(0u, ns.find("inbox-"));
  EXPECT_EQ(38u, ns.size());
  EXPECT_TRUE(ns.find(hex) == std::string::npos);
  EXPECT_EQ(ns, arq_storage::inbox_namespace(hex));
  EXPECT_TRUE(arq_storage::hop_host_allowed("http://127.0.0.1:1091", "http://127.0.0.1:22021"));
  EXPECT_FALSE(arq_storage::hop_host_allowed("http://8.8.8.8:80", "http://127.0.0.1:22021"));
}
