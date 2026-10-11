// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "router_server.h"
#include "router_service.h"

#include "arq_messaging/identity.hpp"
#include "arq_storage/storage_endpoint.h"

#include <boost/program_options.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>

namespace {
std::error_code load_or_create_identity(const std::string& data_dir, arq_messaging::Identity& id)
{
  std::error_code fs_ec;
  std::filesystem::create_directories(data_dir, fs_ec);
  const auto path = std::filesystem::path{data_dir} / "identity";
  if (!arq_messaging::load_identity(path, id))
    return {};
  if (arq_messaging::generate_identity(id))
    return std::make_error_code(std::errc::io_error);
  return arq_messaging::save_identity(path, id);
}
} // namespace

int main(int argc, char** argv)
{
  namespace po = boost::program_options;
  arq_router::RouterConfig cfg;
  cfg.enabled = true;
  cfg.listen = "127.0.0.1:1090";
  cfg.data_dir = "arq-router";
  std::string storage_url;
  std::string token;
  po::options_description desc{"arqma-router"};
  desc.add_options()("help,h", "show help")("listen", po::value<std::string>(&cfg.listen)->default_value(cfg.listen),
                                            "host:port HTTP status")(
      "data-dir", po::value<std::string>(&cfg.data_dir)->default_value(cfg.data_dir), "router data directory")(
      "storage-url", po::value<std::string>(&storage_url), "arqma-storage base URL for POST /v1/store (last hop)")(
      "token", po::value<std::string>(&token), "optional stack token (or ARQMA_STACK_TOKEN)");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout
        << desc
        << "\nPrivacy-router process (separate from arqmad). POST /v1/peel peels one onion hop.\n"
           "POST /v1/store?ns=&key=&ttl= peels then either forwards to the next hop or PUTs into --storage-url.\n";
    return 0;
  }

  arq_router::RouterService service{cfg};
  if (const auto ec = service.init()) {
    std::cerr << "init failed: " << ec.message() << "\n";
    return 1;
  }
  if (const auto ec = service.start()) {
    std::cerr << "start failed: " << ec.message() << "\n";
    return 1;
  }

  arq_messaging::Identity hop{};
  if (const auto ec = load_or_create_identity(cfg.data_dir, hop)) {
    std::cerr << "identity failed: " << ec.message() << "\n";
    return 1;
  }

  std::string host;
  std::uint16_t port = 0;
  if (!arq_storage::parse_listen_address(cfg.listen, host, port)) {
    std::cerr << "listen must be host:port or [ipv6]:port\n";
    return 1;
  }

  arq_router::RouterServer server;
  server.set_identity(hop);
  server.set_storage_url(storage_url);
  if (token.empty()) {
    if (const char* env = std::getenv("ARQMA_STACK_TOKEN"); env && *env)
      token = env;
  }
  if (!token.empty())
    server.set_token(token);
  if (const auto ec = server.listen(host, port)) {
    std::cerr << "bind failed: " << ec.message() << "\n";
    return 1;
  }
  std::cout << "arqma-router " << service.state_name() << " on " << server.base_url() << std::endl;
  while (server.running())
    std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}
