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

#include "arq_messaging/identity.hpp"
#include "arq_messaging/onion_layer.hpp"
#include "arq_messaging/onion_request.hpp"
#include "arq_messaging/sealed_sender.hpp"
#include "arq_messaging/swarm_map.hpp"
#include "arq_router/router_http.h"
#include "arq_router/router_server.h"
#include "arq_router/router_service.h"
#include "arq_storage/http_io.h"
#include "arq_storage/storage_client.h"
#include "arq_storage/storage_server.h"
#include "rpc/rpc_auth.h"

#include <chrono>
#include <filesystem>

TEST(arq_messaging_swarm, in_memory_mapping_roundtrip)
{
  arq_messaging::InMemorySwarmMap map;
  arq_messaging::SwarmMapping mapping;
  mapping.id = 42;
  mapping.service_nodes = {"a", "b"};
  EXPECT_FALSE(map.set_mapping("pk", mapping));

  EXPECT_FALSE(map.refresh());
  const auto got = map.get_swarm("pk");
  ASSERT_TRUE(got);
  EXPECT_EQ(42u, got.id);
  ASSERT_EQ(2u, got.service_nodes.size());
}

TEST(arq_messaging_swarm, rejects_oversized_membership)
{
  arq_messaging::InMemorySwarmMap map;
  arq_messaging::SwarmMapping mapping;
  mapping.id = 1;
  mapping.service_nodes.assign(arq_messaging::max_service_nodes_per_swarm + 1, "sn");
  EXPECT_EQ(std::make_error_code(std::errc::message_size), map.set_mapping("pk", mapping));
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), map.set_mapping("", mapping));
}

TEST(arq_messaging_swarm, hash_pubkey_to_swarm_is_deterministic)
{
  const auto a = arq_messaging::hash_pubkey_to_swarm("abc");
  const auto b = arq_messaging::hash_pubkey_to_swarm("abc");
  const auto c = arq_messaging::hash_pubkey_to_swarm("abd");
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(0u, a);
}

TEST(arq_messaging_onion, validates_complete_three_hop_request)
{
  arq_messaging::OnionRequest req;
  req.endpoint = "/store";
  for (auto& hop : req.hops) {
    hop.service_node_pubkey = "pk";
    hop.address = "127.0.0.1";
    hop.port = 19994;
  }
  EXPECT_TRUE(arq_messaging::validate_onion_request(req));
  req.hops[1].port = 0;
  EXPECT_FALSE(arq_messaging::validate_onion_request(req));
}

TEST(arq_messaging_identity, generate_curve25519_keypair)
{
  arq_messaging::Identity a{};
  arq_messaging::Identity b{};
  EXPECT_FALSE(arq_messaging::generate_identity(a));
  EXPECT_FALSE(arq_messaging::generate_identity(b));
  EXPECT_FALSE(a.public_key.is_null());
  EXPECT_NE(a.public_key.data, b.public_key.data);
  const auto path = (std::filesystem::temp_directory_path() / "arq-identity-ut.bin").string();
  ASSERT_FALSE(arq_messaging::save_identity(path, a));
  arq_messaging::Identity loaded{};
  ASSERT_FALSE(arq_messaging::load_identity(path, loaded));
  EXPECT_EQ(a.public_key.data, loaded.public_key.data);
  std::filesystem::remove(path);
}

TEST(arq_messaging_identity, default_msg_dir_under_home)
{
  EXPECT_EQ(std::filesystem::path("/home/a") / ".arqma" / "msg", arq_messaging::default_msg_dir("/home/a"));
  EXPECT_EQ(std::filesystem::path("/home/a") / ".arqma" / "msg" / "identity",
            arq_messaging::default_msg_dir("/home/a") / "identity");
}

TEST(arq_messaging_sealed_box, seal_open_roundtrip)
{
  arq_messaging::Identity recipient{};
  ASSERT_FALSE(arq_messaging::generate_identity(recipient));

  const std::vector<std::uint8_t> plain{1, 2, 3, 4, 5, 9};
  std::vector<std::uint8_t> cipher;
  ASSERT_FALSE(arq_messaging::seal_payload(recipient.public_key, plain, cipher));
  EXPECT_GT(cipher.size(), plain.size());

  std::vector<std::uint8_t> opened;
  ASSERT_FALSE(arq_messaging::open_payload(recipient, cipher, opened));
  EXPECT_EQ(plain, opened);

  arq_messaging::Identity other{};
  ASSERT_FALSE(arq_messaging::generate_identity(other));
  EXPECT_TRUE(arq_messaging::open_payload(other, cipher, opened));
}

TEST(arq_router, experimental_lifecycle_when_enabled)
{
  arq_router::RouterConfig cfg;
  cfg.enabled = true;
  cfg.data_dir = "/tmp/arq-router-test";
  cfg.listen = "127.0.0.1:1090";
  arq_router::RouterService router{cfg};
  EXPECT_STREQ("idle", router.state_name());
  EXPECT_FALSE(router.init());
  EXPECT_TRUE(router.initialized());
  EXPECT_STREQ("initialized", router.state_name());
  EXPECT_FALSE(router.start());
  EXPECT_TRUE(router.running());
  EXPECT_STREQ("running", router.state_name());
  EXPECT_FALSE(router.stop());
  EXPECT_FALSE(router.running());
}

TEST(arq_router, disabled_rejects_init)
{
  arq_router::RouterService router{};
  EXPECT_EQ(std::make_error_code(std::errc::operation_not_permitted), router.init());
}

TEST(arq_router, enabled_requires_data_dir_and_sane_listen)
{
  arq_router::RouterConfig cfg;
  cfg.enabled = true;
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), arq_router::validate_config(cfg));
  cfg.data_dir = "/tmp/arq-router";
  cfg.listen = "bad";
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), arq_router::validate_config(cfg));
  cfg.listen = "127.0.0.1:1090";
  EXPECT_FALSE(arq_router::validate_config(cfg));
  cfg.listen = "[::1]:1090";
  EXPECT_FALSE(arq_router::validate_config(cfg));

  arq_router::RouterService router{cfg};
  EXPECT_EQ(std::make_error_code(std::errc::not_connected), router.start());
}

TEST(arq_router, peels_one_onion_hop_over_http_handler)
{
  arq_messaging::Identity hop{};
  ASSERT_FALSE(arq_messaging::generate_identity(hop));
  const std::vector<std::uint8_t> inner{0xca, 0xfe, 0x01};
  std::vector<std::uint8_t> outer;
  ASSERT_FALSE(arq_messaging::wrap_onion_layer(hop.public_key, inner, outer));
  const auto res = arq_router::handle_http("POST", "/v1/peel", std::string(outer.begin(), outer.end()), hop);
  EXPECT_NE(std::string::npos, res.find("HTTP/1.1 200 OK"));
  EXPECT_NE(std::string::npos, res.find(std::string(inner.begin(), inner.end())));
  const auto status = arq_router::handle_http("GET", "/", "", hop);
  EXPECT_NE(std::string::npos, status.find("arqma-router"));
}

TEST(arq_router, store_after_peel_writes_storage)
{
  arq_storage::StorageServer storage;
  ASSERT_FALSE(storage.listen("127.0.0.1", 0));
  arq_messaging::Identity hop{};
  ASSERT_FALSE(arq_messaging::generate_identity(hop));
  const std::string payload = "routed-body";
  std::vector<std::uint8_t> outer;
  ASSERT_FALSE(arq_messaging::wrap_onion_layer(hop.public_key,
                                               std::vector<std::uint8_t>(payload.begin(), payload.end()), outer));
  const auto res = arq_router::handle_http("POST", "/v1/store?ns=inbox&key=k1", std::string(outer.begin(), outer.end()),
                                           hop, storage.base_url());
  EXPECT_NE(std::string::npos, res.find("204"));
  arq_storage::Config cfg{arq_storage::Backend::Remote, storage.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient client{cfg};
  const auto got = client.retrieve("inbox", "k1");
  EXPECT_FALSE(got.error);
  EXPECT_EQ(payload, got.value);
  storage.stop();
}

TEST(arq_router, forwards_second_hop_then_stores)
{
  arq_storage::StorageServer storage;
  ASSERT_FALSE(storage.listen("127.0.0.1", 0));
  arq_messaging::Identity hop0{};
  arq_messaging::Identity hop1{};
  ASSERT_FALSE(arq_messaging::generate_identity(hop0));
  ASSERT_FALSE(arq_messaging::generate_identity(hop1));

  arq_router::RouterServer last;
  last.set_identity(hop1);
  last.set_storage_url(storage.base_url());
  ASSERT_FALSE(last.listen("127.0.0.1", 0));

  arq_router::RouterServer first;
  first.set_identity(hop0);
  ASSERT_FALSE(first.listen("127.0.0.1", 0));

  const std::string payload = "two-hop-body";
  std::vector<std::uint8_t> onion;
  ASSERT_FALSE(arq_messaging::compose_onion_route({hop0.public_key, hop1.public_key},
                                                  {first.base_url(), last.base_url()},
                                                  std::vector<std::uint8_t>(payload.begin(), payload.end()), onion));

  const auto ep = arq_storage::parse_endpoint(first.base_url());
  const auto put =
      arq_storage::http_exchange(ep, "POST", "/v1/store?ns=inbox&key=k2", std::string(onion.begin(), onion.end()));
  ASSERT_TRUE(put);

  arq_storage::Config cfg{arq_storage::Backend::Remote, storage.base_url(), std::chrono::milliseconds{2000}};
  arq_storage::StorageClient client{cfg};
  const auto got = client.retrieve("inbox", "k2");
  EXPECT_FALSE(got.error);
  EXPECT_EQ(payload, got.value);
  first.stop();
  last.stop();
  storage.stop();
}

TEST(rpc_auth, access_levels_and_operator_methods)
{
  using cryptonote::rpc::AccessLevel;
  EXPECT_TRUE(cryptonote::rpc::access_allows(AccessLevel::Public, AccessLevel::Admin));
  EXPECT_FALSE(cryptonote::rpc::access_allows(AccessLevel::Admin, AccessLevel::Public));
  EXPECT_TRUE(cryptonote::rpc::method_requires_operator("start_mining"));
  EXPECT_FALSE(cryptonote::rpc::method_requires_operator("get_info"));
  EXPECT_EQ(AccessLevel::Public, cryptonote::rpc::daemon_access_level(true, true));
  EXPECT_EQ(AccessLevel::Operator, cryptonote::rpc::daemon_access_level(false, true));
  EXPECT_EQ(AccessLevel::Admin, cryptonote::rpc::daemon_access_level(true, false));
  EXPECT_FALSE(cryptonote::rpc::allow_rpc_method("stop_daemon", AccessLevel::Public));
  EXPECT_TRUE(cryptonote::rpc::allow_rpc_method("stop_daemon", AccessLevel::Operator));
  EXPECT_TRUE(cryptonote::rpc::allow_rpc_method("get_info", AccessLevel::Public));
}

TEST(rpc_auth, catalog_covers_map_gated_operator_methods)
{
  using cryptonote::rpc::AccessLevel;
  constexpr std::string_view mapped[] = {
      "arqnet_ping",
      "banned",
      "flush_txpool",
      "generateblocks",
      "get_alternate_chains",
      "get_arqnet_status",
      "get_bans",
      "get_blink_status",
      "get_coinbase_tx_sum",
      "get_connections",
      "get_net_stats",
      "get_peer_list",
      "get_public_nodes",
      "get_pulse_status",
      "get_service_node_key",
      "get_service_node_privkey",
      "get_service_node_registration_cmd",
      "get_service_node_registration_cmd_raw",
      "get_storage_status",
      "in_peers",
      "mining_status",
      "out_peers",
      "pop_blocks",
      "prune_blockchain",
      "relay_tx",
      "report_peer_storage_server_status",
      "save_bc",
      "set_bans",
      "set_limit",
      "set_log_categories",
      "set_log_level",
      "start_mining",
      "stop_daemon",
      "stop_mining",
      "storage_server_ping",
      "sync_info",
      "test_trigger_p2p_resync",
      "update",
  };
  ASSERT_EQ(sizeof(cryptonote::rpc::k_operator_rpc_methods) / sizeof(cryptonote::rpc::k_operator_rpc_methods[0]),
            sizeof(mapped) / sizeof(mapped[0]));
  for (std::size_t i = 0; i < sizeof(mapped) / sizeof(mapped[0]); ++i) {
    EXPECT_EQ(mapped[i], cryptonote::rpc::k_operator_rpc_methods[i]);
    EXPECT_TRUE(cryptonote::rpc::method_requires_operator(mapped[i]));
    EXPECT_FALSE(cryptonote::rpc::allow_restricted_map(true, mapped[i]));
    EXPECT_TRUE(cryptonote::rpc::allow_restricted_map(false, mapped[i]));
  }
  EXPECT_TRUE(cryptonote::rpc::allow_restricted_map(true, "get_info"));
  EXPECT_FALSE(cryptonote::rpc::allow_rpc_method("get_pulse_status", AccessLevel::Public));
  EXPECT_TRUE(cryptonote::rpc::allow_rpc_method("get_pulse_status", AccessLevel::Operator));
}

TEST(arq_storage_http, kv_quota_counts_namespace_and_global)
{
  EXPECT_FALSE(arq_storage::kv_quota_exceeded(0, 0, false));
  EXPECT_FALSE(arq_storage::kv_quota_exceeded(arq_storage::max_kv_entries - 1,
                                              arq_storage::max_kv_entries_per_namespace - 1, true));
  EXPECT_TRUE(arq_storage::kv_quota_exceeded(arq_storage::max_kv_entries, 0, true));
  EXPECT_TRUE(arq_storage::kv_quota_exceeded(0, arq_storage::max_kv_entries_per_namespace, true));
  EXPECT_FALSE(
      arq_storage::kv_quota_exceeded(arq_storage::max_kv_entries, arq_storage::max_kv_entries_per_namespace, false));
}
