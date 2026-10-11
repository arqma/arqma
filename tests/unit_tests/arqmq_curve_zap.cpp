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

#include "arqmq/arqmq.h"
#include "arqmq/curve_zap.hpp"
#include "arqmq/mesh_bridge.hpp"
#include "arqmq/peer_table.hpp"
#include "arqmq/socket_stack.hpp"
#include "arqnet/bt_serialize.h"
#include "arq_blink/blink.h"
#include "cryptonote_core/pulse.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <zmq.h>

namespace {
std::string make_pubkey(const char fill)
{
  return std::string(32, fill);
}

std::string make_obligation_vote_payload()
{
  arqnet::bt_dict vote{
      {"v", int64_t{0}},
      {"t", int64_t{0}},
      {"h", int64_t{100}},
      {"g", int64_t{1}},
      {"i", int64_t{0}},
      {"s", std::string(64, 'S')},
      {"wi", int64_t{0}},
      {"sc", int64_t{0}},
  };
  return arqnet::bt_serialize(vote);
}

struct FrameScratch
{
  std::string version = "1.0";
  std::string req_id = "rid";
  std::string domain = arqmq::k_zap_auth_domain_sn;
  std::string address = "127.0.0.1";
  std::string identity;
  std::string mechanism = "CURVE";
  std::string creds;

  std::vector<std::string_view> views() const { return {version, req_id, domain, address, identity, mechanism, creds}; }
};

/// Bind shadow CURVE listener, retrying nearby ports on EADDRINUSE (CI Debug flakes).
bool start_shadow_listener(arqmq::SocketStack& stack, std::string& live_ep, const int preferred_port)
{
  for (int delta = 0; delta <= 40; delta += 2)
  {
    live_ep = "tcp://127.0.0.1:" + std::to_string(preferred_port + delta);
    if (!arqmq::start_mesh_shadow_listener(stack, live_ep))
      return true;
  }
  return false;
}
} // namespace

TEST(arqmq_curve_zap, accepts_service_node_and_denies_unknown)
{
  const auto pubkey = make_pubkey('A');
  const auto allow_sn = [](const std::string&, const std::string& pk) {
    return pk == make_pubkey('A') ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  };

  FrameScratch ok_frames;
  ok_frames.creds = pubkey;
  const auto ok = arqmq::evaluate_curve_zap_frames(ok_frames.views(), allow_sn);
  EXPECT_EQ("200", ok.status_code);
  EXPECT_EQ("S:" + arqmq::to_hex_lower(pubkey), ok.user_id);

  FrameScratch denied_frames;
  denied_frames.creds = make_pubkey('B');
  const auto denied = arqmq::evaluate_curve_zap_frames(denied_frames.views(), allow_sn);
  EXPECT_EQ("400", denied.status_code);
  EXPECT_TRUE(denied.user_id.empty());
}

TEST(arqmq_curve_zap, rejects_bad_domain_mechanism_and_key_size)
{
  const auto allow = [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::ServiceNode; };

  FrameScratch bad_domain;
  bad_domain.creds = make_pubkey('A');
  bad_domain.domain = "wrong.domain";
  EXPECT_EQ("400", arqmq::evaluate_curve_zap_frames(bad_domain.views(), allow).status_code);

  FrameScratch bad_mech;
  bad_mech.creds = make_pubkey('A');
  bad_mech.mechanism = "PLAIN";
  EXPECT_EQ("500", arqmq::evaluate_curve_zap_frames(bad_mech.views(), allow).status_code);

  FrameScratch short_key;
  short_key.creds.assign(31, 'x');
  EXPECT_EQ("500", arqmq::evaluate_curve_zap_frames(short_key.views(), allow).status_code);
}

TEST(arqmq_curve_zap, peer_table_tracks_sn_endpoints)
{
  arqmq::PeerTable table;
  const auto pk = make_pubkey('P');
  table.note_peer(pk, "tcp://127.0.0.1:19990", true);
  table.note_peer(std::string(31, 'x'), "tcp://bad", true); // ignored
  ASSERT_EQ(1u, table.size());
  const auto found = table.find(pk);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ("tcp://127.0.0.1:19990", found->hint);
  EXPECT_TRUE(found->service_node);
  EXPECT_TRUE(table.erase(pk));
  EXPECT_EQ(0u, table.size());
}

bool make_curve_keypair(std::string& pub_bin, std::string& sec_bin)
{
  char public_key[41]{};
  char secret_key[41]{};
  if (zmq_curve_keypair(public_key, secret_key) != 0)
    return false;
  std::array<uint8_t, 32> pub{};
  std::array<uint8_t, 32> sec{};
  if (!zmq_z85_decode(pub.data(), public_key) || !zmq_z85_decode(sec.data(), secret_key))
    return false;
  pub_bin.assign(reinterpret_cast<const char*>(pub.data()), pub.size());
  sec_bin.assign(reinterpret_cast<const char*>(sec.data()), sec.size());
  return true;
}

TEST(arqmq_curve_zap, socket_stack_bind_curve_and_peer_table)
{
  std::string pub_bin;
  std::string sec_bin;
  ASSERT_TRUE(make_curve_keypair(pub_bin, sec_bin));

  arqmq::SocketStack stack;
  ASSERT_FALSE(stack.start());
  stack.set_curve_identity(pub_bin, sec_bin);
  stack.set_allow_connection([](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::ServiceNode; });
  ASSERT_TRUE(stack.curve_zap_configured());

  EXPECT_FALSE(stack.bind_curve("tcp://127.0.0.1:0"));
  EXPECT_FALSE(stack.last_curve_endpoint().empty());
  stack.peers().note_peer(make_pubkey('Z'), "tcp://127.0.0.1:1", true);
  EXPECT_EQ(1u, stack.peers().size());
  stack.stop();
}

TEST(arqmq_curve_zap, curve_peer_send_delivers_vote_ob)
{
  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  std::mutex mu;
  std::condition_variable cv;
  bool got = false;
  std::string got_payload;
  std::string got_peer;

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  server.set_curve_identity(server_pub, server_sec);
  server.set_allow_connection([&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });
  server.register_handler("vote_ob", arqmq::CategoryAcl::ServiceNode, [&](const arqmq::InboundRequest& req) {
    {
      std::lock_guard<std::mutex> lock{mu};
      got_payload = req.payload;
      got_peer = req.peer_pubkey;
      got = true;
    }
    cv.notify_one();
    return std::string{"ok"};
  });
  ASSERT_FALSE(server.bind_curve("tcp://127.0.0.1:0"));
  const std::string endpoint = server.last_curve_endpoint();
  ASSERT_FALSE(endpoint.empty());

  arqmq::SocketStack client;
  ASSERT_FALSE(client.start());
  client.set_curve_identity(client_pub, client_sec);
  client.set_allow_connection([](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  client.peers().note_peer(server_pub, endpoint, true);

  EXPECT_FALSE(client.send_to_peer(server_pub, "vote_ob", "vote-payload"));

  {
    std::unique_lock<std::mutex> lock{mu};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return got; }));
  }
  EXPECT_EQ("vote-payload", got_payload);
  EXPECT_EQ(client_pub, got_peer);

  client.stop();
  server.stop();
}

TEST(arqmq_curve_zap, curve_ping_replies_pong_to_sender)
{
  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  std::mutex mu;
  std::condition_variable cv;
  bool got_pong = false;
  std::string pong_peer;

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  server.set_curve_identity(server_pub, server_sec);
  server.set_allow_connection([&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });
  ASSERT_FALSE(server.bind_curve("tcp://127.0.0.1:0"));
  const std::string endpoint = server.last_curve_endpoint();
  ASSERT_FALSE(endpoint.empty());

  arqmq::SocketStack client;
  ASSERT_FALSE(client.start());
  client.set_curve_identity(client_pub, client_sec);
  client.set_allow_connection([](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  client.peers().note_peer(server_pub, endpoint, true);
  client.register_handler("pong", arqmq::CategoryAcl::ServiceNode, [&](const arqmq::InboundRequest& req) {
    {
      std::lock_guard<std::mutex> lock{mu};
      pong_peer = req.peer_pubkey;
      got_pong = true;
    }
    cv.notify_one();
    return std::string{};
  });

  EXPECT_FALSE(client.send_to_peer(server_pub, "ping", ""));

  {
    std::unique_lock<std::mutex> lock{mu};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_pong; }));
  }
  EXPECT_EQ(server_pub, pong_peer);

  client.stop();
  server.stop();
}

TEST(arqmq_curve_zap, native_mesh_implementation_gate_is_open)
{
  EXPECT_TRUE(arqmq::native_mesh_ready());
  EXPECT_TRUE(arqmq::native_mesh_implementation_ready());
  EXPECT_STREQ("none", arqmq::native_mesh_blocker());
}

TEST(arqmq_curve_zap, native_mesh_live_requires_curve_stack)
{
  EXPECT_TRUE(arqmq::native_mesh_ready_at(arqmq::k_hf_native_arqnet_mesh));
  EXPECT_FALSE(arqmq::native_mesh_live_at(arqmq::k_hf_native_arqnet_mesh));
  EXPECT_TRUE(arqmq::peer_mesh_is_snnetwork());
  EXPECT_STREQ(arqmq::k_transport_snnetwork, arqmq::live_mesh_transport_name(arqmq::k_hf_native_arqnet_mesh));
}

TEST(arqmq_curve_zap, shadow_relay_opt_in_default_off)
{
  EXPECT_FALSE(arqmq::native_mesh_shadow_relay_enabled());
  arqmq::set_native_mesh_shadow_relay_enabled(true);
  EXPECT_TRUE(arqmq::native_mesh_shadow_relay_enabled());
  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::native_mesh_shadow_relay_enabled());
  // No active stack / not configured → no-op.
  arqmq::shadow_send_to_peer(make_pubkey('S'), "vote_ob", "x", "tcp://127.0.0.1:1");
}

TEST(arqmq_curve_zap, shadow_dual_write_delivers_vote_ob_via_facade)
{
  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  std::mutex mu;
  std::condition_variable cv;
  bool got = false;
  std::string got_payload;

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  server.set_curve_identity(server_pub, server_sec);
  server.set_allow_connection([&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });
  server.register_handler("vote_ob", arqmq::CategoryAcl::ServiceNode, [&](const arqmq::InboundRequest& req) {
    {
      std::lock_guard<std::mutex> lock{mu};
      got_payload = req.payload;
      got = true;
    }
    cv.notify_one();
    return std::string{"ok"};
  });
  ASSERT_FALSE(server.bind_curve("tcp://127.0.0.1:0"));
  const std::string endpoint = server.last_curve_endpoint();
  ASSERT_FALSE(endpoint.empty());

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });

  arqmq::set_native_mesh_shadow_relay_enabled(true);
  arqmq::note_live_mesh_relay("vote_ob");
  arqmq::shadow_send_to_peer(server_pub, "vote_ob", "dual-write-payload", endpoint);

  {
    std::unique_lock<std::mutex> lock{mu};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return got; }));
  }
  EXPECT_EQ("dual-write-payload", got_payload);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_EQ(1u, stats.live_relays);
  EXPECT_EQ(1u, stats.vote_ob_live);
  EXPECT_GE(stats.ok, 1u);
  EXPECT_GE(stats.vote_ob_shadow_ok, 1u);
  EXPECT_GE(arqmq::native_mesh_shadow_ok_rate_bps(), 1u);
  EXPECT_FALSE(arqmq::native_mesh_shadow_parity_sample_ok(32, 9500)); // sample too small
  EXPECT_FALSE(arqmq::native_mesh_shadow_parity_sample_ok(1, 9000));  // outbound only; inbound parse required

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, parity_sample_requires_vote_ob_volume)
{
  arqmq::set_native_mesh_shadow_relay_enabled(true); // resets counters
  EXPECT_FALSE(arqmq::native_mesh_shadow_parity_sample_ok(1, 1));
  arqmq::note_live_mesh_relay("ping");
  EXPECT_FALSE(arqmq::native_mesh_shadow_parity_sample_ok(1, 1)); // not vote_ob
  arqmq::set_native_mesh_shadow_relay_enabled(false);
}

TEST(arqmq_curve_zap, endpoint_port_offset_rewrites_tcp)
{
  EXPECT_EQ("tcp://10.0.0.1:29996",
            arqmq::endpoint_with_port_offset("tcp://10.0.0.1:19996", arqmq::k_mesh_shadow_port_offset));
  EXPECT_EQ("tcp://127.0.0.1:0", arqmq::endpoint_with_port_offset("tcp://127.0.0.1:0", 0));
  EXPECT_TRUE(arqmq::endpoint_with_port_offset("not-an-endpoint", 1).empty());
  EXPECT_TRUE(arqmq::endpoint_with_port_offset("tcp://host:70000", 1).empty());
}

TEST(arqmq_curve_zap, primary_mesh_send_safe_without_stack)
{
  EXPECT_TRUE(arqmq::native_mesh_ready());
  EXPECT_TRUE(arqmq::peer_mesh_is_snnetwork());
  EXPECT_STREQ(arqmq::k_transport_snnetwork, arqmq::mesh_transport_name());
  arqmq::primary_mesh_send_to_peer(make_pubkey('Z'), "vote_ob", "x", "tcp://127.0.0.1:19996");
}

TEST(arqmq_curve_zap, primary_mesh_send_delivers_vote_ob_after_cutover)
{
  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  std::mutex mu;
  std::condition_variable cv;
  bool got = false;
  std::string got_payload;

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  server.set_curve_identity(server_pub, server_sec);
  server.set_allow_connection([&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });
  server.register_handler("vote_ob", arqmq::CategoryAcl::ServiceNode, [&](const arqmq::InboundRequest& req) {
    {
      std::lock_guard<std::mutex> lock{mu};
      got_payload = req.payload;
      got = true;
    }
    cv.notify_one();
    return std::string{"ok"};
  });
  ASSERT_FALSE(server.bind_curve("tcp://127.0.0.1:0"));
  const std::string endpoint = server.last_curve_endpoint();
  ASSERT_FALSE(endpoint.empty());

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  client->peers().note_peer(server_pub, endpoint, true);

  EXPECT_TRUE(arqmq::native_mesh_live_at(arqmq::k_hf_native_arqnet_mesh));
  EXPECT_STREQ(arqmq::k_transport_arqmq, arqmq::live_mesh_transport_name(arqmq::k_hf_native_arqnet_mesh));
  EXPECT_FALSE(arqmq::native_mesh_live_at(static_cast<uint8_t>(19)));

  // Empty hint: do not apply ANET+10000 rewrite; send to the bound CURVE port.
  arqmq::primary_mesh_send_to_peer(server_pub, "vote_ob", "cutover-payload", "");

  {
    std::unique_lock<std::mutex> lock{mu};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return got; }));
  }
  EXPECT_EQ("cutover-payload", got_payload);

  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, shadow_listener_receives_offset_dual_write)
{
  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  std::mutex mu;
  std::condition_variable cv;
  bool got = false;

  // Server listens only on the shadow offset port (simulates soak peer).
  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  server.set_curve_identity(server_pub, server_sec);
  server.set_allow_connection([&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });
  server.register_handler("vote_ob", arqmq::CategoryAcl::ServiceNode, [&](const arqmq::InboundRequest&) {
    {
      std::lock_guard<std::mutex> lock{mu};
      got = true;
    }
    cv.notify_one();
    return std::string{"ok"};
  });
  ASSERT_FALSE(server.bind_curve("tcp://127.0.0.1:0"));
  const std::string shadow_ep = server.last_curve_endpoint();
  ASSERT_FALSE(shadow_ep.empty());

  // Derive a fake live hint that rewrites to shadow_ep via +0 by using the
  // shadow endpoint directly after enabling offset rewrite through start_mesh_shadow_listener.
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  // Bind client shadow listener on an ephemeral live bind rewritten by offset.
  // Use a high live port so live+10000 stays valid; listener itself is unused here.
  ASSERT_FALSE(arqmq::start_mesh_shadow_listener(*client, "tcp://127.0.0.1:45000"));
  EXPECT_FALSE(arqmq::native_mesh_shadow_endpoint().empty());

  // Peer published live port = shadow_ep.port - offset; dual-write rewrites up.
  const auto colon = shadow_ep.rfind(':');
  ASSERT_NE(std::string::npos, colon);
  const int shadow_port = std::stoi(shadow_ep.substr(colon + 1));
  const int live_port = shadow_port - arqmq::k_mesh_shadow_port_offset;
  ASSERT_GT(live_port, 0);
  const std::string live_hint = "tcp://127.0.0.1:" + std::to_string(live_port);

  arqmq::note_live_mesh_relay("vote_ob");
  arqmq::shadow_send_to_peer(server_pub, "vote_ob", "offset-payload", live_hint);

  {
    std::unique_lock<std::mutex> lock{mu};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return got; }));
  }
  EXPECT_GE(arqmq::native_mesh_shadow_stats().vote_ob_shadow_ok, 1u);

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, vote_ob_wire_payload_rejects_garbage)
{
  arqmq::set_vote_ob_payload_validator(nullptr);
  EXPECT_FALSE(arqmq::vote_ob_wire_payload_ok(""));
  EXPECT_FALSE(arqmq::vote_ob_wire_payload_ok("vote-payload"));
  EXPECT_FALSE(arqmq::vote_ob_wire_payload_ok("dual-write-payload"));
  EXPECT_TRUE(arqmq::vote_ob_wire_payload_ok(make_obligation_vote_payload()));
}

TEST(arqmq_curve_zap, soak_inbound_parses_bt_vote_ob)
{
  arqmq::set_vote_ob_payload_validator(nullptr);

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  arqmq::configure_mesh_shadow(server, server_pub, server_sec, [&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  std::string live_ep;
  ASSERT_TRUE(start_shadow_listener(server, live_ep, 46000));
  const std::string shadow_ep = arqmq::native_mesh_shadow_endpoint();
  ASSERT_FALSE(shadow_ep.empty());

  const std::string payload = make_obligation_vote_payload();
  ASSERT_TRUE(arqmq::vote_ob_wire_payload_ok(payload));

  arqmq::note_live_mesh_relay("vote_ob");
  arqmq::shadow_send_to_peer(server_pub, "vote_ob", payload, live_ep);

  bool parsed = false;
  for (int i = 0; i < 80; ++i)
  {
    // Wait on parse_ok (not merely inbound) so ASan/weak memory cannot observe
    // vote_ob_shadow_in before the matching parse counter is published.
    if (arqmq::native_mesh_shadow_stats().vote_ob_shadow_parse_ok >= 1)
    {
      parsed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(parsed);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_GE(stats.vote_ob_shadow_ok, 1u);
  EXPECT_GE(stats.vote_ob_shadow_in, 1u);
  EXPECT_GE(stats.vote_ob_shadow_parse_ok, 1u);
  EXPECT_EQ(0u, stats.vote_ob_shadow_parse_fail);
  EXPECT_TRUE(arqmq::native_mesh_shadow_parity_sample_ok(1, 9000));

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, soak_inbound_counts_unparseable_vote_ob)
{
  arqmq::set_vote_ob_payload_validator(nullptr);

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  arqmq::configure_mesh_shadow(server, server_pub, server_sec, [&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  std::string live_ep;
  ASSERT_TRUE(start_shadow_listener(server, live_ep, 46100));
  arqmq::note_live_mesh_relay("vote_ob");
  arqmq::shadow_send_to_peer(server_pub, "vote_ob", "not-a-vote", live_ep);

  bool rejected = false;
  for (int i = 0; i < 80; ++i)
  {
    if (arqmq::native_mesh_shadow_stats().vote_ob_shadow_parse_fail >= 1)
    {
      rejected = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(rejected);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_GE(stats.vote_ob_shadow_parse_fail, 1u);
  EXPECT_EQ(0u, stats.vote_ob_shadow_parse_ok);
  EXPECT_FALSE(arqmq::native_mesh_shadow_parity_sample_ok(1, 9000));

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, pulse_rnd_wire_payload_rejects_garbage)
{
  arqmq::set_pulse_rnd_payload_validator(nullptr);
  EXPECT_FALSE(arqmq::pulse_rnd_wire_payload_ok(""));
  EXPECT_FALSE(arqmq::pulse_rnd_wire_payload_ok("not-a-vote"));
  std::string bad(service_nodes::pulse::k_relay_vote_bytes, '\0');
  EXPECT_FALSE(arqmq::pulse_rnd_wire_payload_ok(bad));
  service_nodes::pulse::RelayVote vote{};
  std::string v1(114, '\0');
  v1[0] = 1;
  EXPECT_FALSE(arqmq::pulse_rnd_wire_payload_ok(v1));
  EXPECT_FALSE(service_nodes::pulse::decode_relay_vote(v1, vote));
  vote.height = 240;
  std::string blob;
  ASSERT_TRUE(service_nodes::pulse::encode_relay_vote(vote, blob));
  EXPECT_TRUE(arqmq::pulse_rnd_wire_payload_ok(blob));
}

TEST(arqmq_curve_zap, soak_inbound_parses_pulse_rnd)
{
  arqmq::set_pulse_rnd_payload_validator(nullptr);

  service_nodes::pulse::RelayVote vote{};
  vote.height = 240;
  vote.round = 1;
  vote.leader_index = 2;
  vote.validator_index = 3;
  std::string payload;
  ASSERT_TRUE(service_nodes::pulse::encode_relay_vote(vote, payload));
  ASSERT_TRUE(arqmq::pulse_rnd_wire_payload_ok(payload));

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  arqmq::configure_mesh_shadow(server, server_pub, server_sec, [&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  std::string live_ep;
  ASSERT_TRUE(start_shadow_listener(server, live_ep, 46200));

  arqmq::note_live_mesh_relay("pulse_rnd");
  arqmq::shadow_send_to_peer(server_pub, "pulse_rnd", payload, live_ep);

  bool parsed = false;
  for (int i = 0; i < 80; ++i)
  {
    if (arqmq::native_mesh_shadow_stats().pulse_rnd_shadow_parse_ok >= 1)
    {
      parsed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(parsed);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_GE(stats.pulse_rnd_shadow_ok, 1u);
  EXPECT_GE(stats.pulse_rnd_shadow_in, 1u);
  EXPECT_GE(stats.pulse_rnd_shadow_parse_ok, 1u);
  EXPECT_EQ(0u, stats.pulse_rnd_shadow_parse_fail);
  EXPECT_GE(stats.pulse_rnd_live, 1u);

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, soak_inbound_counts_unparseable_pulse_rnd)
{
  arqmq::set_pulse_rnd_payload_validator(nullptr);

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  arqmq::configure_mesh_shadow(server, server_pub, server_sec, [&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  std::string live_ep;
  ASSERT_TRUE(start_shadow_listener(server, live_ep, 46300));
  arqmq::note_live_mesh_relay("pulse_rnd");
  arqmq::shadow_send_to_peer(server_pub, "pulse_rnd", "not-a-pulse-vote", live_ep);

  bool rejected = false;
  for (int i = 0; i < 80; ++i)
  {
    if (arqmq::native_mesh_shadow_stats().pulse_rnd_shadow_parse_fail >= 1)
    {
      rejected = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(rejected);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_GE(stats.pulse_rnd_shadow_parse_fail, 1u);
  EXPECT_EQ(0u, stats.pulse_rnd_shadow_parse_ok);

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, soak_local_pulse_rnd_roundtrip)
{
  service_nodes::pulse::RelayVote vote{};
  vote.height = 240;
  vote.round = 2;
  vote.leader_index = 4;
  vote.validator_index = 1;
  vote.prev_id.data[0] = 11;
  std::string blob;
  ASSERT_TRUE(service_nodes::pulse::encode_relay_vote(vote, blob));

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  std::mutex mu;
  std::condition_variable cv;
  bool got = false;
  service_nodes::pulse::RelayVote decoded{};

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  server.set_curve_identity(server_pub, server_sec);
  server.set_allow_connection([&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });
  server.register_handler("pulse_rnd", arqmq::CategoryAcl::ServiceNode, [&](const arqmq::InboundRequest& req) {
    service_nodes::pulse::RelayVote parsed{};
    const bool ok = service_nodes::pulse::decode_relay_vote(req.payload, parsed);
    {
      std::lock_guard<std::mutex> lock{mu};
      if (ok)
        decoded = parsed;
      got = ok;
    }
    cv.notify_one();
    return std::string{"ok"};
  });
  ASSERT_FALSE(server.bind_curve("tcp://127.0.0.1:0"));
  const std::string shadow_ep = server.last_curve_endpoint();
  ASSERT_FALSE(shadow_ep.empty());

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);
  ASSERT_FALSE(arqmq::start_mesh_shadow_listener(*client, "tcp://127.0.0.1:47000"));

  const auto colon = shadow_ep.rfind(':');
  ASSERT_NE(std::string::npos, colon);
  const int shadow_port = std::stoi(shadow_ep.substr(colon + 1));
  const int live_port = shadow_port - arqmq::k_mesh_shadow_port_offset;
  ASSERT_GT(live_port, 0);
  const std::string live_hint = "tcp://127.0.0.1:" + std::to_string(live_port);

  arqmq::note_live_mesh_relay("pulse_rnd");
  arqmq::shadow_send_to_peer(server_pub, "pulse_rnd", blob, live_hint);

  {
    std::unique_lock<std::mutex> lock{mu};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return got; }));
  }
  EXPECT_EQ(vote.height, decoded.height);
  EXPECT_EQ(vote.round, decoded.round);
  EXPECT_EQ(vote.leader_index, decoded.leader_index);
  EXPECT_EQ(vote.validator_index, decoded.validator_index);
  EXPECT_GE(arqmq::native_mesh_shadow_stats().pulse_rnd_shadow_ok, 1u);

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}


TEST(arqmq_curve_zap, blink_tx_wire_payload_rejects_garbage)
{
  arqmq::set_blink_tx_payload_validator(nullptr);
  EXPECT_FALSE(arqmq::blink_tx_wire_payload_ok(""));
  EXPECT_FALSE(arqmq::blink_tx_wire_payload_ok("not-a-blink-vote"));
  std::string bad(arq_blink::k_relay_vote_bytes, '\0');
  EXPECT_FALSE(arqmq::blink_tx_wire_payload_ok(bad));
  std::string v1(arq_blink::k_relay_vote_bytes, '\0');
  v1[0] = static_cast<char>(arq_blink::k_relay_vote_version);
  EXPECT_TRUE(arqmq::blink_tx_wire_payload_ok(v1));
  arq_blink::RelayVote vote{};
  vote.height = 77;
  vote.validator_index = 3;
  vote.txid.data[0] = 9;
  std::string blob;
  ASSERT_TRUE(arq_blink::encode_relay_vote(vote, blob));
  EXPECT_TRUE(arqmq::blink_tx_wire_payload_ok(blob));
}

TEST(arqmq_curve_zap, soak_inbound_parses_blink_tx)
{
  arqmq::set_blink_tx_payload_validator(nullptr);

  arq_blink::RelayVote vote{};
  vote.height = 88;
  vote.validator_index = 2;
  vote.txid.data[0] = 7;
  std::string payload;
  ASSERT_TRUE(arq_blink::encode_relay_vote(vote, payload));
  ASSERT_TRUE(arqmq::blink_tx_wire_payload_ok(payload));

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  arqmq::configure_mesh_shadow(server, server_pub, server_sec, [&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  std::string live_ep;
  ASSERT_TRUE(start_shadow_listener(server, live_ep, 46400));

  arqmq::note_live_mesh_relay("blink_tx");
  arqmq::shadow_send_to_peer(server_pub, "blink_tx", payload, live_ep);

  bool parsed = false;
  for (int i = 0; i < 80; ++i)
  {
    if (arqmq::native_mesh_shadow_stats().blink_tx_shadow_parse_ok >= 1)
    {
      parsed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(parsed);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_GE(stats.blink_tx_shadow_ok, 1u);
  EXPECT_GE(stats.blink_tx_shadow_in, 1u);
  EXPECT_GE(stats.blink_tx_shadow_parse_ok, 1u);
  EXPECT_EQ(0u, stats.blink_tx_shadow_parse_fail);
  EXPECT_GE(stats.blink_tx_live, 1u);

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}

TEST(arqmq_curve_zap, soak_inbound_counts_unparseable_blink_tx)
{
  arqmq::set_blink_tx_payload_validator(nullptr);

  std::string server_pub;
  std::string server_sec;
  std::string client_pub;
  std::string client_sec;
  ASSERT_TRUE(make_curve_keypair(server_pub, server_sec));
  ASSERT_TRUE(make_curve_keypair(client_pub, client_sec));

  arqmq::SocketStack server;
  ASSERT_FALSE(server.start());
  arqmq::configure_mesh_shadow(server, server_pub, server_sec, [&](const std::string&, const std::string& pk) {
    return pk == client_pub ? arqmq::CurvePeerAllow::ServiceNode : arqmq::CurvePeerAllow::Denied;
  });

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::ServiceNode}));
  auto* client = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, client);
  arqmq::configure_mesh_shadow(*client, client_pub, client_sec,
                               [](const std::string&, const std::string&) { return arqmq::CurvePeerAllow::Denied; });
  arqmq::set_native_mesh_shadow_relay_enabled(true);

  std::string live_ep;
  ASSERT_TRUE(start_shadow_listener(server, live_ep, 46500));
  arqmq::note_live_mesh_relay("blink_tx");
  arqmq::shadow_send_to_peer(server_pub, "blink_tx", "not-a-blink-vote", live_ep);

  bool rejected = false;
  for (int i = 0; i < 80; ++i)
  {
    if (arqmq::native_mesh_shadow_stats().blink_tx_shadow_parse_fail >= 1)
    {
      rejected = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(rejected);

  const auto stats = arqmq::native_mesh_shadow_stats();
  EXPECT_GE(stats.blink_tx_shadow_parse_fail, 1u);
  EXPECT_EQ(0u, stats.blink_tx_shadow_parse_ok);

  arqmq::set_native_mesh_shadow_relay_enabled(false);
  EXPECT_FALSE(arqmq::shutdown());
  server.stop();
}
