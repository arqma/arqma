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

#include "arq_messaging/contacts.hpp"
#include "arq_messaging/message_envelope.hpp"
#include "arq_messaging/onion_forward.hpp"
#include "arq_messaging/onion_layer.hpp"
#include "arq_messaging/sealed_sender.hpp"
#include "arq_messaging/stack_env.hpp"

#include <filesystem>

TEST(arq_messaging_envelope, roundtrip_binary_encoding)
{
  arq_messaging::MessageEnvelope envelope{};
  envelope.version = arq_messaging::MessageEnvelope::current_version;
  envelope.ttl_seconds = 180;
  envelope.payload = {0xde, 0xad, 0xbe, 0xef};

  for (std::size_t i = 0; i < envelope.recipient_x25519.data.size(); ++i)
    envelope.recipient_x25519.data[i] = static_cast<std::uint8_t>(i);

  const auto encoded = arq_messaging::encode_message_envelope(envelope);

  arq_messaging::MessageEnvelope decoded{};
  ASSERT_TRUE(arq_messaging::decode_message_envelope(
      std::string_view{reinterpret_cast<const char*>(encoded.data()), encoded.size()}, decoded));

  EXPECT_EQ(envelope.version, decoded.version);
  EXPECT_EQ(envelope.recipient_x25519.data, decoded.recipient_x25519.data);
  EXPECT_EQ(envelope.ttl_seconds, decoded.ttl_seconds);
  EXPECT_EQ(envelope.payload, decoded.payload);
}

TEST(arq_messaging_envelope, rejects_truncated_payload)
{
  arq_messaging::MessageEnvelope envelope{};
  envelope.payload = {1, 2, 3};

  auto encoded = arq_messaging::encode_message_envelope(envelope);
  encoded.pop_back();

  arq_messaging::MessageEnvelope decoded{};
  EXPECT_FALSE(arq_messaging::decode_message_envelope(
      std::string_view{reinterpret_cast<const char*>(encoded.data()), encoded.size()}, decoded));
}

TEST(arq_messaging_envelope, sealed_sender_marker_and_ttl_bounds)
{
  arq_messaging::MessageEnvelope envelope{};
  envelope.ttl_seconds = 60;
  envelope.payload = {0x11, 0x22};
  EXPECT_TRUE(arq_messaging::validate_envelope_ttl(envelope));

  envelope.ttl_seconds = arq_messaging::max_envelope_ttl_seconds + 1;
  EXPECT_FALSE(arq_messaging::validate_envelope_ttl(envelope));

  envelope.ttl_seconds = 60;
  envelope = arq_messaging::with_sealed_sender_marker(std::move(envelope), true);
  EXPECT_TRUE(arq_messaging::has_sealed_sender_marker(envelope));
  EXPECT_EQ(arq_messaging::SealedSenderTag::marker, envelope.payload.front());
}

TEST(arq_messaging_envelope, onion_multi_hop_peel_roundtrip)
{
  constexpr int hops = 3;
  std::vector<arq_messaging::Identity> ids(hops);
  std::vector<arq_messaging::X25519PublicKey> pubs;
  for (auto& id : ids) {
    ASSERT_FALSE(arq_messaging::generate_identity(id));
    pubs.push_back(id.public_key);
  }

  const std::vector<std::uint8_t> payload = {0x01, 0x02, 0x03, 0xca, 0xfe};
  std::vector<std::uint8_t> onion;
  ASSERT_FALSE(arq_messaging::build_onion(pubs, payload, onion));
  EXPECT_GT(onion.size(), payload.size());

  std::vector<std::uint8_t> recovered;
  ASSERT_FALSE(arq_messaging::peel_onion(ids, onion, recovered));
  EXPECT_EQ(payload, recovered);
}

TEST(arq_messaging_envelope, onion_rejects_oversized_payload)
{
  arq_messaging::Identity id{};
  ASSERT_FALSE(arq_messaging::generate_identity(id));
  std::vector<std::uint8_t> huge(arq_messaging::max_onion_payload_bytes + 1, 0xab);
  std::vector<std::uint8_t> onion;
  EXPECT_TRUE(arq_messaging::build_onion({id.public_key}, huge, onion));
}

TEST(arq_messaging_envelope, onion_forward_frame_roundtrip)
{
  const std::string url = "http://127.0.0.1:1091";
  const std::vector<std::uint8_t> inner{0x11, 0x22, 0x33};
  std::vector<std::uint8_t> framed;
  ASSERT_TRUE(arq_messaging::encode_onion_forward(url, inner, framed));
  std::string next;
  std::vector<std::uint8_t> recovered;
  ASSERT_TRUE(arq_messaging::decode_onion_forward(
      std::string_view{reinterpret_cast<const char*>(framed.data()), framed.size()}, next, recovered));
  EXPECT_EQ(url, next);
  EXPECT_EQ(inner, recovered);
  EXPECT_FALSE(arq_messaging::encode_onion_forward("", inner, framed));
  EXPECT_FALSE(arq_messaging::decode_onion_forward("nope", next, recovered));
}

TEST(arq_messaging_envelope, compose_onion_route_peels_forward_frame)
{
  arq_messaging::Identity hop0{};
  arq_messaging::Identity hop1{};
  ASSERT_FALSE(arq_messaging::generate_identity(hop0));
  ASSERT_FALSE(arq_messaging::generate_identity(hop1));
  const std::vector<std::uint8_t> payload{0xca, 0xfe, 0xba, 0xbe};
  const std::string url0 = "http://127.0.0.1:1090";
  const std::string url1 = "http://127.0.0.1:1091";
  std::vector<std::uint8_t> onion;
  ASSERT_FALSE(arq_messaging::compose_onion_route({hop0.public_key, hop1.public_key}, {url0, url1}, payload, onion));

  std::vector<std::uint8_t> after0;
  ASSERT_FALSE(arq_messaging::peel_onion_layer(hop0, onion, after0));
  std::string next;
  std::vector<std::uint8_t> rest;
  ASSERT_TRUE(arq_messaging::decode_onion_forward(
      std::string_view{reinterpret_cast<const char*>(after0.data()), after0.size()}, next, rest));
  EXPECT_EQ(url1, next);

  std::vector<std::uint8_t> recovered;
  ASSERT_FALSE(arq_messaging::peel_onion_layer(hop1, rest, recovered));
  EXPECT_EQ(payload, recovered);
}

TEST(arq_messaging_envelope, parse_stack_env_storage_and_router)
{
  const auto env = arq_messaging::parse_stack_env("# comment\n"
                                                  "ARQMA_STORAGE_URL=http://127.0.0.1:22021\n"
                                                  "ARQMA_ROUTER_URL = http://127.0.0.1:1090\n"
                                                  "OTHER=ignore\n");
  EXPECT_EQ("http://127.0.0.1:22021", env.storage_url);
  EXPECT_EQ("http://127.0.0.1:1090", env.router_url);
  EXPECT_TRUE(env.token.empty());
  const auto with_token = arq_messaging::parse_stack_env("ARQMA_STACK_TOKEN=secret-token\n");
  EXPECT_EQ("secret-token", with_token.token);
  const auto empty = arq_messaging::parse_stack_env("");
  EXPECT_EQ("http://127.0.0.1:22021", empty.storage_url);
  EXPECT_TRUE(empty.router_url.empty());
}

TEST(arq_messaging_envelope, parse_recipient_hex_name_and_remember)
{
  const std::string hex(64, 'a');
  std::string name;
  std::string got;
  ASSERT_TRUE(arq_messaging::parse_recipient_token(hex, name, got));
  EXPECT_TRUE(name.empty());
  EXPECT_EQ(hex, got);
  ASSERT_TRUE(arq_messaging::parse_recipient_token("bob=" + hex, name, got));
  EXPECT_EQ("bob", name);
  EXPECT_EQ(hex, got);
  ASSERT_TRUE(arq_messaging::parse_recipient_token("alice", name, got));
  EXPECT_EQ("alice", name);
  EXPECT_TRUE(got.empty());
  EXPECT_FALSE(arq_messaging::parse_recipient_token("nope!", name, got));
  EXPECT_FALSE(arq_messaging::parse_recipient_token("bad=zz", name, got));
  EXPECT_FALSE(arq_messaging::is_contact_name(hex));
}

TEST(arq_messaging_envelope, contacts_upsert_lookup_and_unknown)
{
  const auto dir = std::filesystem::temp_directory_path() / "arqma-contacts-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const auto path = dir / "contacts";
  const std::string hex(64, 'b');
  std::string got;
  EXPECT_FALSE(arq_messaging::resolve_recipient("bob", path, got));
  ASSERT_TRUE(arq_messaging::resolve_recipient("bob=" + hex, path, got));
  EXPECT_EQ(hex, got);
  got.clear();
  ASSERT_TRUE(arq_messaging::resolve_recipient("bob", path, got));
  EXPECT_EQ(hex, got);
  EXPECT_TRUE(static_cast<bool>(arq_messaging::upsert_contact(path, "bad name", hex)));
  std::filesystem::remove_all(dir);
}

TEST(arq_messaging_envelope, contacts_file_sits_next_to_identity)
{
  EXPECT_EQ(arq_messaging::default_identity_path().parent_path() / "contacts", arq_messaging::default_contacts_path());
}
