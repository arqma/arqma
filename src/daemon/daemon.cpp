// Copyright (c) 2018 - 2026, The Arqma Network
// Copyright (c) 2014-2018, The Monero Project
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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <memory>
#include <stdexcept>
#include <boost/algorithm/string/split.hpp>
#include "misc_log_ex.h"
#include "daemon/daemon.h"
#include "rpc/daemon_handler.h"
#include "rpc/zmq_server.h"
#include "cryptonote_protocol/arqnet.h"
#include "arqmq/arqmq.h"
#include "arqmq/backend_policy.hpp"
#include "arqmq/mesh_bridge.hpp"
#include "arq_router/router_service.h"
#include "arq_storage/storage_client.h"

#include "common/password.h"
#include "common/util.h"
#include "daemon/core.h"
#include "daemon/p2p.h"
#include "daemon/protocol.h"
#include "daemon/rpc.h"
#include "daemon/command_server.h"
#include "daemon/command_line_args.h"
#include "net/net_ssl.h"
#include "version.h"

using namespace epee;
using namespace std::literals;

#include <functional>

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "daemon"

namespace daemonize {

struct t_internals {
private:
  t_protocol protocol;
public:
  t_core core;
  t_p2p p2p;
  std::vector<std::unique_ptr<t_rpc>> rpcs;
  std::unique_ptr<arq_router::RouterService> router_service;

  t_internals(
      boost::program_options::variables_map const & vm
    )
    : core{vm}
    , protocol{vm, core, command_line::get_arg(vm, cryptonote::arg_offline)}
    , p2p{vm, protocol}
  {
    // Handle circular dependencies
    protocol.set_p2p_endpoint(p2p.get());
    core.set_protocol(protocol.get());
    arqnet::init_core_callbacks();

    {
      const bool testnet = command_line::get_arg(vm, cryptonote::arg_testnet_on);
      const bool stagenet = command_line::get_arg(vm, cryptonote::arg_stagenet_on);
      const arqmq::NetworkClass net =
          testnet ? arqmq::NetworkClass::Testnet
                  : stagenet ? arqmq::NetworkClass::Stagenet
                             : arqmq::NetworkClass::Mainnet;

      const auto backend_arg = command_line::get_arg(vm, daemon_args::arg_arqnet_backend);
      const bool allow_experimental =
          command_line::get_arg(vm, daemon_args::arg_arqnet_allow_experimental);
      const auto selection =
          arqmq::resolve_backend(backend_arg, net, allow_experimental);

      if (selection.overridden)
        MWARNING("Arq-Net backend selection overridden for mainnet safety: " << selection.reason);
      else
        MINFO("Arq-Net backend policy: " << selection.reason);

      arqmq::Config mq_cfg{};
      mq_cfg.backend = selection.backend;

      const auto mq_ec = arqmq::init(mq_cfg);
      if (mq_ec)
      {
        MWARNING("ArqMQ backend init failed (" << mq_ec.message()
                                   << "); continuing with legacy Arq-Net path for compatibility");
        arqmq::Config fallback{};
        fallback.backend = arqmq::Backend::LegacyArqNet;
        (void)arqmq::init(fallback);
      }
      else
      {
        MINFO("Arq-Net messaging facade backend: " << arqmq::to_string(arqmq::current_backend())
                                                   << " (transport=" << arqmq::transport_name()
                                                   << ", mesh=" << arqmq::mesh_transport_name()
                                                   << ", native=" << (arqmq::native_transport_active() ? "yes" : "no")
                                                   << ")");
        if (arqmq::current_backend() == arqmq::Backend::ArqMq && !arqmq::native_transport_active())
          MWARNING("ArqMQ backend selected but dedicated socket stack is not active");
        if (!arqmq::peer_mesh_is_snnetwork())
          MWARNING("Peer mesh is not SNNetwork; verify dual-run cutover readiness before mainnet use");
      }

      const bool mesh_shadow_requested =
          command_line::get_arg(vm, daemon_args::arg_arqnet_mesh_shadow);
      const auto shadow_sel =
          arqmq::resolve_mesh_shadow(mesh_shadow_requested, net, allow_experimental,
                                     arqmq::native_transport_active());
      arqmq::set_native_mesh_shadow_relay_enabled(shadow_sel.enabled);
      if (shadow_sel.overridden)
        MWARNING("Arq-Net mesh shadow selection overridden: " << shadow_sel.reason);
      else if (shadow_sel.enabled)
        MINFO("Arq-Net mesh shadow: " << shadow_sel.reason);
      else if (mesh_shadow_requested)
        MINFO("Arq-Net mesh shadow: " << shadow_sel.reason);
    }

    {
      arq_storage::Config storage_cfg{};
      storage_cfg.backend = arq_storage::Backend::Remote;
      storage_cfg.base_url = command_line::get_arg(vm, daemon_args::arg_storage_client_url);
      if (!storage_cfg.base_url.empty() && !arq_storage::parse_endpoint(storage_cfg.base_url))
      {
        MWARNING("Invalid --storage-client-url '" << storage_cfg.base_url
                                                  << "'; reachability probes disabled");
        storage_cfg.base_url.clear();
      }
      arq_storage::configure_daemon_client(storage_cfg);
      if (!storage_cfg.base_url.empty())
        MINFO("Storage client reachability URL: " << storage_cfg.base_url);
    }

    if (command_line::get_arg(vm, daemon_args::arg_arq_router))
    {
      arq_router::RouterConfig router_cfg;
      router_cfg.enabled = true;
      router_cfg.data_dir = (boost::filesystem::path(command_line::get_arg(vm, cryptonote::arg_data_dir)) / "arq-router").string();
      router_cfg.listen = "127.0.0.1:1090";
      router_service = std::make_unique<arq_router::RouterService>(router_cfg);
      const auto router_ec = router_service->init();
      if (router_ec)
      {
        MWARNING("Arq router scaffold init failed: " << router_ec.message());
        router_service.reset();
      }
      else
      {
        (void)router_service->start();
        MINFO("Arq privacy router experimental lifecycle started (state=" << router_service->state_name()
              << ", data_dir=" << router_cfg.data_dir << "; no production onion routing yet)");
      }
    }

    const auto restricted = command_line::get_arg(vm, cryptonote::core_rpc_server::arg_restricted_rpc);
    const auto main_rpc_port = command_line::get_arg(vm, cryptonote::core_rpc_server::arg_rpc_bind_port);
    rpcs.emplace_back(new t_rpc{vm, core, p2p, restricted, main_rpc_port, "core"});

    auto restricted_rpc_port_arg = cryptonote::core_rpc_server::arg_rpc_restricted_bind_port;
    if(!command_line::is_arg_defaulted(vm, restricted_rpc_port_arg))
    {
      auto restricted_rpc_port = command_line::get_arg(vm, restricted_rpc_port_arg);
      rpcs.emplace_back(new t_rpc{vm, core, p2p, true, restricted_rpc_port, "restricted"});
    }
  }

  ~t_internals()
  {
    if (router_service)
    {
      (void)router_service->stop();
      router_service.reset();
    }
    (void)arqmq::shutdown();
  }
};

void t_daemon::init_options(boost::program_options::options_description & option_spec)
{
  t_core::init_options(option_spec);
  t_p2p::init_options(option_spec);
  t_rpc::init_options(option_spec);
}

t_daemon::t_daemon(
    boost::program_options::variables_map const & vm, uint16_t public_rpc_port
  )
  : mp_internals{new t_internals{vm}}
  , public_rpc_port(public_rpc_port)
{
  zmq_rpc_bind_port = command_line::get_arg(vm, daemon_args::arg_zmq_rpc_bind_port);
  zmq_rpc_bind_address = command_line::get_arg(vm, daemon_args::arg_zmq_rpc_bind_ip);
  zmq_rpc_enabled = command_line::get_arg(vm, daemon_args::arg_zmq_rpc_enabled);
}

t_daemon::~t_daemon() = default;

// MSVC is brain-dead and can't default this...
t_daemon::t_daemon(t_daemon && other)
{
  if (this != &other)
  {
    mp_internals = std::move(other.mp_internals);
    other.mp_internals.reset(nullptr);
    public_rpc_port = other.public_rpc_port;
  }
}

// or this
t_daemon & t_daemon::operator=(t_daemon && other)
{
  if (this != &other)
  {
    mp_internals = std::move(other.mp_internals);
    other.mp_internals.reset(nullptr);
    public_rpc_port = other.public_rpc_port;
  }
  return *this;
}

bool t_daemon::run(bool interactive)
{
  if (nullptr == mp_internals)
  {
    throw std::runtime_error{"Can't run stopped daemon"};
  }

  std::atomic<bool> stop(false), shutdown(false);
  std::thread stop_thread{[&stop, &shutdown, this] {
    while (!stop)
      std::this_thread::sleep_for(100ms);
    if (shutdown)
      this->stop_p2p();
  }};
  epee::misc_utils::auto_scope_leave_caller scope_exit_handler = epee::misc_utils::create_scope_leave_handler([&](){
    stop = true;
    stop_thread.join();
  });
  tools::signal_handler::install([&stop, &shutdown](int){ stop = shutdown = true; });

  try
  {
    if (!mp_internals->core.run())
      return false;

    for(auto& rpc: mp_internals->rpcs)
      rpc->run();

    std::unique_ptr<daemonize::t_command_server> rpc_commands;
    if (interactive && mp_internals->rpcs.size())
    {
      // The first three variables are not used when the fourth is false
      rpc_commands.reset(new daemonize::t_command_server(0, 0, boost::none, epee::net_utils::ssl_support_t::e_ssl_support_disabled, false, mp_internals->rpcs.front()->get_server()));
      rpc_commands->start_handling(std::bind(&daemonize::t_daemon::stop_p2p, this));
    }

    cryptonote::rpc::DaemonHandler rpc_daemon_handler(mp_internals->core.get(), mp_internals->p2p.get());
    cryptonote::rpc::ZmqServer zmq_server(rpc_daemon_handler);

    if (zmq_rpc_enabled)
    {
      if (!zmq_server.addTCPSocket(zmq_rpc_bind_address, zmq_rpc_bind_port))
      {
        LOG_ERROR(std::string("Failed to add TCP Socket (") + zmq_rpc_bind_address
            + ":" + zmq_rpc_bind_port + ") to Arqma ZMQ RPC Server");

        if (rpc_commands)
          rpc_commands->stop_handling();

        for(auto& rpc : mp_internals->rpcs)
          rpc->stop();

        return false;
      }

      MGINFO("Starting Arqma ZMQ Server...");
      zmq_server.run();

      MGINFO("Arqma ZMQ Server started at " << zmq_rpc_bind_address << ":" << zmq_rpc_bind_port);
    }
    else
      MGINFO("Arqma ZMQ Server disabled");

    if (public_rpc_port > 0)
    {
      MGINFO("Public RPC Port " << public_rpc_port << " will be advertise to other peers over P2P");
      mp_internals->p2p.get().set_rpc_port(public_rpc_port);
    }

    mp_internals->p2p.run(); // blocks until p2p goes down

    if(rpc_commands)
      rpc_commands->stop_handling();

    if (zmq_rpc_enabled)
      zmq_server.stop();

    for(auto& rpc : mp_internals->rpcs)
      rpc->stop();
    MGINFO("Node stopped.");
    return true;
  }
  catch (std::exception const & ex)
  {
    MFATAL("Uncaught exception! " << ex.what());
    return false;
  }
  catch (...)
  {
    MFATAL("Uncaught exception!");
    return false;
  }
}

void t_daemon::stop()
{
  if (nullptr == mp_internals)
  {
    throw std::runtime_error{"Can't stop stopped daemon"};
  }
  mp_internals->p2p.stop();
  for(auto& rpc : mp_internals->rpcs)
    rpc->stop();

  mp_internals.reset(nullptr); // Ensure resources are cleaned up before we return
}

void t_daemon::stop_p2p()
{
  if (nullptr == mp_internals)
  {
    throw std::runtime_error{"Can't send stop signal to a stopped daemon"};
  }
  mp_internals->p2p.get().send_stop_signal();
}

} // namespace daemonize
