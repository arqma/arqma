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
#include "arqmq/command_registry.hpp"
#include "arqmq/socket_stack.hpp"

TEST(arqmq_facade, backend_names_match_scaffold)
{
  EXPECT_STREQ("legacy-arqnet", arqmq::to_string(arqmq::Backend::LegacyArqNet));
  EXPECT_STREQ("arqmq", arqmq::to_string(arqmq::Backend::ArqMq));
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_STREQ("snnetwork", arqmq::transport_name());
}

TEST(arqmq_facade, acl_names_match_scaffold)
{
  EXPECT_STREQ("denied", arqmq::to_string(arqmq::CategoryAcl::Denied));
  EXPECT_STREQ("basic", arqmq::to_string(arqmq::CategoryAcl::Basic));
  EXPECT_STREQ("service-node", arqmq::to_string(arqmq::CategoryAcl::ServiceNode));
  EXPECT_STREQ("admin", arqmq::to_string(arqmq::CategoryAcl::Admin));
}

TEST(arqmq_facade, legacy_backend_initializes)
{
  EXPECT_FALSE(arqmq::shutdown());

  const arqmq::Config config{arqmq::Backend::LegacyArqNet, arqmq::CategoryAcl::ServiceNode};
  EXPECT_FALSE(arqmq::init(config));
  EXPECT_EQ(arqmq::Backend::LegacyArqNet, arqmq::current_backend());
  EXPECT_TRUE(arqmq::is_initialized());
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::is_initialized());
}

TEST(arqmq_facade, arqmq_backend_starts_dedicated_socket_stack)
{
  EXPECT_FALSE(arqmq::shutdown());

  const arqmq::Config config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::Admin};
  EXPECT_FALSE(arqmq::init(config));
  EXPECT_EQ(arqmq::Backend::ArqMq, arqmq::current_backend());
  EXPECT_TRUE(arqmq::is_initialized());
  EXPECT_TRUE(arqmq::native_transport_active());
  EXPECT_STREQ("arqmq", arqmq::transport_name());
  ASSERT_NE(nullptr, arqmq::active_socket_stack());
  EXPECT_TRUE(arqmq::active_socket_stack()->running());
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::native_transport_active());
  EXPECT_STREQ("snnetwork", arqmq::transport_name());
}

TEST(arqmq_facade, acl_allows_respects_privilege_order)
{
  EXPECT_FALSE(arqmq::allows(arqmq::CategoryAcl::Basic, arqmq::CategoryAcl::Denied));
  EXPECT_TRUE(arqmq::allows(arqmq::CategoryAcl::Basic, arqmq::CategoryAcl::Basic));
  EXPECT_TRUE(arqmq::allows(arqmq::CategoryAcl::Basic, arqmq::CategoryAcl::ServiceNode));
  EXPECT_TRUE(arqmq::allows(arqmq::CategoryAcl::ServiceNode, arqmq::CategoryAcl::Admin));
  EXPECT_FALSE(arqmq::allows(arqmq::CategoryAcl::Admin, arqmq::CategoryAcl::ServiceNode));
}

TEST(arqmq_facade, default_acl_is_retained_after_init)
{
  EXPECT_FALSE(arqmq::shutdown());
  const arqmq::Config config{arqmq::Backend::LegacyArqNet, arqmq::CategoryAcl::ServiceNode};
  EXPECT_FALSE(arqmq::init(config));
  EXPECT_EQ(arqmq::CategoryAcl::ServiceNode, arqmq::default_acl());
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_EQ(arqmq::CategoryAcl::Denied, arqmq::default_acl());
}

TEST(arqmq_facade, command_registry_maps_builtin_acls)
{
  EXPECT_EQ(arqmq::CategoryAcl::ServiceNode, arqmq::required_acl_for("vote_ob"));
  EXPECT_EQ(arqmq::CategoryAcl::ServiceNode, arqmq::required_acl_for("pulse_rnd"));
  EXPECT_EQ(arqmq::CategoryAcl::ServiceNode, arqmq::required_acl_for("blink_tx"));
  EXPECT_EQ(arqmq::CategoryAcl::Basic, arqmq::required_acl_for("arqnet_status"));
  EXPECT_EQ(arqmq::CategoryAcl::Admin, arqmq::required_acl_for("admin_shutdown"));
  EXPECT_EQ(arqmq::CategoryAcl::Denied, arqmq::required_acl_for("unknown.command"));

  EXPECT_TRUE(arqmq::authorize("ping", arqmq::CategoryAcl::ServiceNode));
  EXPECT_FALSE(arqmq::authorize("admin_shutdown", arqmq::CategoryAcl::ServiceNode));
  EXPECT_FALSE(arqmq::authorize("unknown.command", arqmq::CategoryAcl::Admin));
}

TEST(arqmq_facade, message_limits_reject_oversized_requests)
{
  EXPECT_TRUE(arqmq::accept_request("ping", 32));
  EXPECT_FALSE(arqmq::accept_request("", 32));
  EXPECT_FALSE(arqmq::accept_request("ping", arqmq::max_message_bytes + 1));
  EXPECT_FALSE(arqmq::accept_request("ping", 32, arqmq::max_payload_frames + 1));
  EXPECT_FALSE(arqmq::accept_request(std::string(arqmq::max_command_name_bytes + 1, 'x'), 1));

  EXPECT_TRUE(arqmq::authorize_request("ping", arqmq::CategoryAcl::Basic, 16));
  EXPECT_FALSE(arqmq::authorize_request("ping", arqmq::CategoryAcl::Basic, arqmq::max_message_bytes + 1));
  EXPECT_FALSE(arqmq::authorize_request("admin_shutdown", arqmq::CategoryAcl::ServiceNode, 16));
}
