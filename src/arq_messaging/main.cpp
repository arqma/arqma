// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "arq_messaging/contacts.hpp"
#include "arq_messaging/identity.hpp"
#include "arq_messaging/message_envelope.hpp"
#include "arq_messaging/onion_layer.hpp"
#include "arq_messaging/onion_request.hpp"
#include "arq_messaging/sealed_sender.hpp"
#include "arq_messaging/seen.hpp"
#include "arq_messaging/stack_env.hpp"
#include "arq_storage/http_io.h"
#include "arq_storage/storage_client.h"

#include <boost/program_options.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {
std::string to_hex(const std::uint8_t* data, std::size_t n)
{
  static const char* digits = "0123456789abcdef";
  std::string out(n * 2, '0');
  for (std::size_t i = 0; i < n; ++i) {
    out[i * 2] = digits[data[i] >> 4];
    out[i * 2 + 1] = digits[data[i] & 0xf];
  }
  return out;
}

bool from_hex(const std::string& hex, std::uint8_t* out, std::size_t n)
{
  if (hex.size() != n * 2)
    return false;
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
      return 10 + (c - 'A');
    return -1;
  };
  for (std::size_t i = 0; i < n; ++i) {
    const int hi = nib(hex[i * 2]);
    const int lo = nib(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0)
      return false;
    out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  return true;
}

std::error_code load_or_create_local_identity(arq_messaging::Identity& id, const bool create)
{
  const auto path = arq_messaging::default_identity_path();
  if (!arq_messaging::load_identity(path, id))
    return {};
  if (!create)
    return std::make_error_code(std::errc::no_such_file_or_directory);
  std::error_code fs_ec;
  std::filesystem::create_directories(path.parent_path(), fs_ec);
  if (arq_messaging::generate_identity(id))
    return std::make_error_code(std::errc::io_error);
  return arq_messaging::save_identity(path, id);
}

bool inbox_to_hex(const std::string& to, std::string& out)
{
  if (to.size() == 64) {
    out = to;
    return true;
  }
  arq_messaging::Identity id{};
  if (load_or_create_local_identity(id, false))
    return false;
  out = to_hex(id.public_key.data.data(), 32);
  return true;
}
} // namespace

int main(int argc, char** argv)
{
  namespace po = boost::program_options;
  std::string cmd;
  std::string url = "http://127.0.0.1:22021";
  std::string to;
  std::vector<std::string> text_parts;
  std::string key;
  std::string secret;
  std::vector<std::string> routers;
  std::vector<std::string> snodes;
  std::uint32_t ttl = 3600;
  po::options_description desc{"arqma-msg"};
  desc.add_options()("help,h", "show help")("url", po::value<std::string>(&url), "storage base URL")(
      "to", po::value<std::string>(&to), "recipient (64-hex, name, or name=hex)")(
      "ttl", po::value<std::uint32_t>(&ttl), "envelope TTL seconds")("key", po::value<std::string>(&key),
                                                                     "storage key (get/open)")(
      "text", po::value<std::vector<std::string>>(&text_parts)->composing(),
      "plaintext (send)")("secret", po::value<std::string>(&secret), "recipient private key hex (open)")(
      "router", po::value<std::vector<std::string>>(&routers)->composing(),
      "arqma-router URL (repeat, outermost first, max 3)")(
      "snode", po::value<std::vector<std::string>>(&snodes)->composing(), "storage member URL (swarm announce)");
  po::options_description hidden;
  hidden.add_options()("cmd", po::value<std::string>(&cmd));
  po::positional_options_description pos;
  pos.add("cmd", 1);
  pos.add("to", 1);
  pos.add("text", -1);
  po::variables_map vm;
  po::options_description all;
  all.add(desc).add(hidden);
  po::store(po::command_line_parser(argc, argv).options(all).positional(pos).run(), vm);
  po::notify(vm);
  const auto stack = arq_messaging::load_stack_env();
  if (!vm.count("url"))
    url = stack.storage_url;
  const bool explicit_router = vm.count("router") != 0;
  if (!explicit_router && !stack.router_url.empty())
    routers.push_back(stack.router_url);
  std::string text;
  for (const auto& word : text_parts) {
    if (!text.empty())
      text.push_back(' ');
    text += word;
  }
  if (vm.count("help") || cmd.empty()) {
    std::cout << "Usage: arqma-msg gen|send|inbox|open|contacts|name|status|unread [options]\n"
              << desc
              << "\nEveryday (start utils/arqma-stack.sh first):\n"
                 "  arqma-msg gen\n"
                 "  arqma-msg send <hex> hello\n"
                 "  arqma-msg inbox\n"
                 "  arqma-msg open\n"
                 "  arqma-msg contacts\n"
                 "  arqma-msg name alice <hex>\n"
                 "  arqma-msg status\n"
                 "  arqma-msg unread\n"
                 "\nGUI look (Arqma-GUI-MM black + muted gold):\n"
                 "  utils/arqma-msg-ui.py   # http://127.0.0.1:8787/\n"
                 "\nOperator knobs: --url, --to, --secret, --key, --router, name=hex contacts, swarm --snode.\n";
    return vm.count("help") ? 0 : 1;
  }

  if (cmd == "contacts") {
    const auto contacts = arq_messaging::load_contacts(arq_messaging::default_contacts_path());
    for (const auto& kv : contacts)
      std::cout << kv.first << ' ' << kv.second << "\n";
    return 0;
  }

  if (cmd == "name") {
    if (to.empty() || text_parts.empty()) {
      std::cerr << "usage: arqma-msg name <nick> <64-hex>\n";
      return 1;
    }
    if (const auto ec = arq_messaging::upsert_contact(arq_messaging::default_contacts_path(), to, text_parts.front())) {
      std::cerr << "name failed: " << ec.message() << "\n";
      return 1;
    }
    return 0;
  }

  if (cmd == "status") {
    arq_messaging::Identity id{};
    const bool have_id = !load_or_create_local_identity(id, false);
    std::cout << "storage=" << url << "\n";
    std::cout << "router=" << (routers.empty() ? std::string{"-"} : routers.front()) << "\n";
    std::cout << "token=" << (stack.token.empty() ? "missing" : "set") << "\n";
    if (have_id)
      std::cout << "identity=" << to_hex(id.public_key.data.data(), 32) << "\n";
    else
      std::cout << "identity=-\n";
    std::cout << "contacts=" << arq_messaging::load_contacts(arq_messaging::default_contacts_path()).size() << "\n";
    return 0;
  }

  if (cmd == "gen") {
    arq_messaging::Identity id{};
    if (const auto ec = load_or_create_local_identity(id, true)) {
      std::cerr << "identity failed: " << ec.message() << "\n";
      return 1;
    }
    std::cout << to_hex(id.public_key.data.data(), 32) << "\n";
    arq_messaging::upsert_contact(arq_messaging::default_contacts_path(), "me", to_hex(id.public_key.data.data(), 32));
    return 0;
  }

  arq_storage::Config cfg;
  cfg.backend = arq_storage::Backend::Remote;
  cfg.base_url = url;
  cfg.connect_timeout = std::chrono::milliseconds{2000};
  cfg.token = stack.token;
  arq_storage::StorageClient client{cfg};

  if (cmd == "send") {
    if (to.empty() || text.empty()) {
      std::cerr << "send requires a recipient and text\n";
      return 1;
    }
    std::string hex;
    if (!arq_messaging::resolve_recipient(to, arq_messaging::default_contacts_path(), hex)) {
      std::cerr << "unknown recipient (64-hex, saved name, or name=hex)\n";
      return 1;
    }
    arq_messaging::X25519PublicKey pub{};
    if (!from_hex(hex, pub.data.data(), 32))
      return 1;
    std::vector<std::uint8_t> plain(text.begin(), text.end());
    std::vector<std::uint8_t> sealed;
    if (arq_messaging::seal_payload(pub, plain, sealed)) {
      std::cerr << "seal failed\n";
      return 1;
    }
    arq_messaging::MessageEnvelope env{};
    env.recipient_x25519 = pub;
    env.ttl_seconds = ttl == 0 ? 3600 : ttl;
    env.payload = std::move(sealed);
    if (!arq_messaging::validate_envelope_ttl(env)) {
      std::cerr << "ttl out of range\n";
      return 1;
    }
    const auto blob = arq_messaging::encode_message_envelope(env);
    const auto store_key = to_hex(blob.data(), std::min<std::size_t>(blob.size(), 16));
    if (explicit_router && routers.size() > arq_messaging::OnionRequest::hop_count) {
      std::cerr << "at most 3 --router hops\n";
      return 1;
    }
    auto send_via_router = [&]() -> bool {
      if (routers.size() > arq_messaging::OnionRequest::hop_count)
        return false;
      std::vector<arq_messaging::X25519PublicKey> pubs;
      pubs.reserve(routers.size());
      for (const auto& router : routers) {
        const auto ep = arq_storage::parse_endpoint(router);
        const auto pubget =
            arq_storage::http_exchange(ep, "GET", arq_storage::with_token("/v1/pubkey", stack.token), {});
        if (!pubget || pubget.body.size() < 64)
          return false;
        arq_messaging::X25519PublicKey rpub{};
        if (!from_hex(pubget.body.substr(0, 64), rpub.data.data(), 32))
          return false;
        pubs.push_back(rpub);
      }
      std::vector<std::uint8_t> onion;
      if (arq_messaging::compose_onion_route(pubs, routers, blob, onion))
        return false;
      std::string rpath = "/v1/store?ns=" + arq_storage::inbox_namespace(hex) + "&key=" + store_key;
      if (env.ttl_seconds != 0)
        rpath += "&ttl=" + std::to_string(env.ttl_seconds);
      const auto ep = arq_storage::parse_endpoint(routers.front());
      return static_cast<bool>(arq_storage::http_exchange(ep, "POST", arq_storage::with_token(rpath, stack.token),
                                                          std::string(onion.begin(), onion.end())));
    };
    if (!routers.empty()) {
      if (send_via_router()) {
        std::cout << store_key << "\n";
        return 0;
      }
      if (explicit_router) {
        std::cerr << "router store failed\n";
        return 1;
      }
    }
    arq_storage::StoreRequest req{arq_storage::inbox_namespace(hex), store_key, std::string(blob.begin(), blob.end())};
    req.ttl_seconds = env.ttl_seconds;
    if (const auto ec = client.store(req)) {
      std::cerr << "store failed: " << ec.message() << "\n";
      return 1;
    }
    std::cout << store_key << "\n";
    return 0;
  }

  if (cmd == "get") {
    if (to.size() != 64 || key.empty()) {
      std::cerr << "get requires --to <recipient hex> and --key\n";
      return 1;
    }
    const auto got = client.retrieve(arq_storage::inbox_namespace(to), key);
    if (!got) {
      std::cerr << "retrieve failed: " << got.error.message() << "\n";
      return 1;
    }
    std::cout << got.value << "\n";
    return 0;
  }

  if (cmd == "inbox") {
    std::string inbox_to;
    if (!inbox_to_hex(to, inbox_to)) {
      std::cerr << "run arqma-msg gen first\n";
      return 1;
    }
    const auto keys = client.list_keys(arq_storage::inbox_namespace(inbox_to));
    if (!keys) {
      std::cerr << "list failed: " << keys.error.message() << "\n";
      return 1;
    }
    for (const auto& k : keys.value)
      std::cout << k << "\n";
    return 0;
  }

  if (cmd == "open") {
    arq_messaging::Identity id{};
    std::string inbox_to = to;
    if (to.size() == 64 && secret.size() == 64) {
      if (!from_hex(to, id.public_key.data.data(), 32) || !from_hex(secret, id.private_key.data.data(), 32))
        return 1;
    } else if (load_or_create_local_identity(id, false)) {
      std::cerr << "run arqma-msg gen first\n";
      return 1;
    } else {
      inbox_to = to_hex(id.public_key.data.data(), 32);
    }
    std::vector<std::string> keys;
    if (!key.empty()) {
      keys.push_back(key);
    } else {
      const auto listed = client.list_keys(arq_storage::inbox_namespace(inbox_to));
      if (!listed) {
        std::cerr << "list failed: " << listed.error.message() << "\n";
        return 1;
      }
      keys = listed.value;
    }
    for (const auto& item : keys) {
      const auto got = client.retrieve(arq_storage::inbox_namespace(inbox_to), item);
      if (!got) {
        std::cerr << "retrieve failed: " << got.error.message() << "\n";
        return 1;
      }
      arq_messaging::MessageEnvelope env{};
      if (!arq_messaging::decode_message_envelope(got.value, env)) {
        std::cerr << "bad envelope\n";
        return 1;
      }
      std::vector<std::uint8_t> plain;
      if (arq_messaging::open_payload(id, env.payload, plain)) {
        std::cerr << "open failed\n";
        return 1;
      }
      if (key.empty())
        std::cout << item << "\n";
      std::cout << std::string(plain.begin(), plain.end()) << "\n";
      (void)arq_messaging::mark_seen(arq_messaging::default_seen_path(), item);
    }
    return 0;
  }

  if (cmd == "unread") {
    std::string inbox_to;
    if (!inbox_to_hex(to, inbox_to)) {
      std::cerr << "run arqma-msg gen first\n";
      return 1;
    }
    const auto keys = client.list_keys(arq_storage::inbox_namespace(inbox_to));
    if (!keys) {
      std::cerr << "list failed: " << keys.error.message() << "\n";
      return 1;
    }
    const auto seen = arq_messaging::load_seen(arq_messaging::default_seen_path());
    for (const auto& k : keys.value) {
      if (!seen.count(k))
        std::cout << k << "\n";
    }
    return 0;
  }

  if (cmd == "swarm") {
    if (to.size() != 64) {
      std::cerr << "swarm requires --to <recipient hex>\n";
      return 1;
    }
    const auto swarm_id = arq_storage::inbox_opaque_id(to);
    if (!snodes.empty())
      client.set_snodes_for_pubkey(swarm_id, snodes);
    const auto got = client.get_swarm(swarm_id);
    if (!got) {
      std::cerr << "swarm failed: " << got.error.message() << "\n";
      return 1;
    }
    std::cout << got.value.first << "\n";
    for (const auto& member : got.value.second)
      std::cout << member << "\n";
    return 0;
  }

  std::cerr << "unknown command " << cmd << "\n";
  return 1;
}
