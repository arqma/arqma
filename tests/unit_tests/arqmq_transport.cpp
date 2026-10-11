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
#include "arqmq/message_limits.hpp"
#include "arqmq/socket_stack.hpp"
#include "arqmq/transport.hpp"

TEST(arqmq_transport, socket_stack_ping_through_worker)
{
  arqmq::SocketStack stack;
  ASSERT_FALSE(stack.start());
  ASSERT_TRUE(stack.running());
  EXPECT_STREQ(arqmq::k_transport_arqmq, stack.name());
  EXPECT_TRUE(stack.has_handler("ping"));

  arqmq::InboundRequest req;
  req.command = "ping";
  req.peer_acl = arqmq::CategoryAcl::Basic;
  req.payload = "hi";
  std::string reply;
  EXPECT_FALSE(stack.dispatch(req, &reply));
  EXPECT_EQ("pong", reply);
  stack.stop();
  EXPECT_FALSE(stack.running());
}

TEST(arqmq_transport, socket_stack_denies_unauthorized_and_oversized)
{
  arqmq::SocketStack stack;
  ASSERT_FALSE(stack.start());

  arqmq::InboundRequest denied;
  denied.command = "vote_ob";
  denied.peer_acl = arqmq::CategoryAcl::Basic;
  denied.payload = "x";
  std::string reply;
  EXPECT_EQ(std::errc::permission_denied, stack.dispatch(denied, &reply));

  // vote_ob has no default handler; ServiceNode ACL still needs a handler.
  stack.register_handler("vote_ob", arqmq::CategoryAcl::ServiceNode,
                         [](const arqmq::InboundRequest&) { return std::string{"ok"}; });
  denied.peer_acl = arqmq::CategoryAcl::ServiceNode;
  EXPECT_FALSE(stack.dispatch(denied, &reply));
  EXPECT_EQ("ok", reply);

  arqmq::InboundRequest huge = denied;
  huge.payload.assign(arqmq::max_message_bytes + 1, 'x');
  EXPECT_EQ(std::errc::permission_denied, stack.dispatch(huge, &reply));

  stack.stop();
}

TEST(arqmq_transport, facade_arqmq_exposes_running_stack)
{
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::ArqMq, arqmq::CategoryAcl::Basic}));
  auto* stack = arqmq::active_socket_stack();
  ASSERT_NE(nullptr, stack);
  ASSERT_TRUE(stack->running());

  arqmq::InboundRequest req;
  req.command = "ping";
  req.peer_acl = arqmq::CategoryAcl::ServiceNode;
  std::string reply;
  EXPECT_FALSE(stack->dispatch(req, &reply));
  EXPECT_EQ("pong", reply);

  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_EQ(nullptr, arqmq::active_socket_stack());
}

TEST(arqmq_transport, legacy_backend_keeps_snnetwork_transport_name)
{
  EXPECT_FALSE(arqmq::shutdown());
  EXPECT_FALSE(arqmq::init(arqmq::Config{arqmq::Backend::LegacyArqNet, arqmq::CategoryAcl::Basic}));
  EXPECT_FALSE(arqmq::native_transport_active());
  EXPECT_STREQ(arqmq::k_transport_snnetwork, arqmq::transport_name());
  EXPECT_EQ(nullptr, arqmq::active_socket_stack());
  EXPECT_FALSE(arqmq::shutdown());
}
