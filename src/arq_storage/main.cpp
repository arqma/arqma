// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "storage_server.h"
#include "storage_endpoint.h"

#include <boost/program_options.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv)
{
  namespace po = boost::program_options;
  std::string listen = "127.0.0.1:22021";
  std::string data_dir;
  std::string token;
  int gossip_interval = 15;
  std::vector<std::string> peers;
  po::options_description desc{"arqma-storage"};
  desc.add_options()("help,h", "show help")("listen", po::value<std::string>(&listen)->default_value(listen),
                                            "host:port (HTTP Storage Server)")(
      "data-dir", po::value<std::string>(&data_dir), "optional on-disk volume for KV / snodes")(
      "token", po::value<std::string>(&token), "optional stack token (or ARQMA_STACK_TOKEN)")(
      "gossip-interval", po::value<int>(&gossip_interval)->default_value(gossip_interval),
      "anti-entropy gossip period in seconds (0 disables)")(
      "peer", po::value<std::vector<std::string>>(&peers)->composing(), "replica base URL (repeatable)");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc
              << "\nCompanion binary for arqmad. Probe with:\n"
                 "  arqmad --storage-client-url=http://"
              << listen << "\n";
    return 0;
  }
  std::string host;
  std::uint16_t port = 0;
  if (!arq_storage::parse_listen_address(listen, host, port)) {
    std::cerr << "listen must be host:port or [ipv6]:port\n";
    return 1;
  }
  arq_storage::StorageServer server;
  if (token.empty()) {
    if (const char* env = std::getenv("ARQMA_STACK_TOKEN"); env && *env)
      token = env;
  }
  if (!token.empty())
    server.set_token(token);
  if (!data_dir.empty())
    server.set_data_dir(data_dir);
  server.set_gossip_interval(std::chrono::seconds{gossip_interval < 0 ? 0 : gossip_interval});
  for (const auto& peer : peers)
    server.add_peer(peer);
  if (const auto ec = server.listen(host, port)) {
    std::cerr << "bind failed: " << ec.message() << "\n";
    return 1;
  }
  std::cout << "arqma-storage listening on " << server.base_url() << std::endl;
  while (server.running())
    std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}
