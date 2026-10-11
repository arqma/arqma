// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#include "router_server.h"
#include "router_http.h"
#include "arq_storage/http_io.h"
#include "arq_storage/storage_endpoint.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <sstream>
#include <thread>

namespace arq_router {
struct RouterServer::Impl
{
  boost::asio::io_context io;
  boost::asio::ip::tcp::acceptor acceptor{io};
  std::thread thread;
  std::atomic<bool> running{false};
  std::atomic<std::uint16_t> port{0};
  std::string host{"127.0.0.1"};
  arq_messaging::Identity hop{};
  std::string storage_url;
  std::string token;

  void serve(boost::asio::ip::tcp::socket socket)
  {
    try {
      boost::asio::streambuf buf{arq_storage::max_http_header_bytes};
      boost::system::error_code ec;
      boost::asio::read_until(socket, buf, "\r\n\r\n", ec);
      if (ec)
        return;
      std::istream is{&buf};
      std::string request_line;
      std::getline(is, request_line);
      if (!request_line.empty() && request_line.back() == '\r')
        request_line.pop_back();
      std::string method;
      std::string path;
      {
        std::istringstream ls{request_line};
        std::string ver;
        ls >> method >> path >> ver;
      }
      std::size_t content_length = 0;
      std::string header;
      while (std::getline(is, header)) {
        if (!header.empty() && header.back() == '\r')
          header.pop_back();
        if (header.empty())
          break;
        const auto colon = header.find(':');
        if (colon == std::string::npos)
          continue;
        std::string name = header.substr(0, colon);
        for (char& c : name)
          c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        if (name == "content-length")
          content_length = static_cast<std::size_t>(std::strtoul(header.c_str() + colon + 1, nullptr, 10));
      }
      if (content_length > arq_storage::max_http_body_bytes) {
        const char* too_large = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        boost::asio::write(socket, boost::asio::buffer(too_large, std::strlen(too_large)), ec);
        return;
      }
      std::string body(std::istreambuf_iterator<char>(is), {});
      if (content_length > body.size()) {
        std::string rest(content_length - body.size(), '\0');
        boost::asio::read(socket, boost::asio::buffer(rest), boost::asio::transfer_exactly(rest.size()), ec);
        if (!ec)
          body.append(rest);
      }
      const auto response = handle_http(method, path, body, hop, storage_url, token);
      boost::asio::write(socket, boost::asio::buffer(response), ec);
    } catch (...) {
    }
  }

  void run()
  {
    while (running.load()) {
      boost::system::error_code ec;
      boost::asio::ip::tcp::socket socket{io};
      acceptor.accept(socket, ec);
      if (!running.load())
        break;
      if (ec)
        continue;
      serve(std::move(socket));
    }
  }
};

RouterServer::RouterServer() : impl_(std::make_unique<Impl>()) {}

RouterServer::~RouterServer()
{
  stop();
}

void RouterServer::set_identity(arq_messaging::Identity hop)
{
  impl_->hop = std::move(hop);
}

void RouterServer::set_storage_url(std::string url)
{
  impl_->storage_url = std::move(url);
}

void RouterServer::set_token(std::string token)
{
  impl_->token = std::move(token);
}

std::error_code RouterServer::listen(const std::string& host, const std::uint16_t port)
{
  stop();
  try {
    const auto addr = boost::asio::ip::make_address(host);
    boost::asio::ip::tcp::endpoint ep{addr, port};
    impl_->acceptor = boost::asio::ip::tcp::acceptor{impl_->io};
    impl_->acceptor.open(ep.protocol());
    impl_->acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    impl_->acceptor.bind(ep);
    impl_->acceptor.listen();
    impl_->port = impl_->acceptor.local_endpoint().port();
    impl_->host = host;
    impl_->running = true;
    impl_->thread = std::thread([this] { impl_->run(); });
    return {};
  } catch (...) {
    return std::make_error_code(std::errc::address_in_use);
  }
}

void RouterServer::stop()
{
  if (!impl_)
    return;
  // Same wakeup pattern as arq_storage::StorageServer::stop — sync accept()
  // is not reliably interrupted by close() alone across threads.
  const auto port = impl_->port.load();
  const std::string host = impl_->host;
  impl_->running = false;
  if (port != 0) {
    try {
      boost::asio::io_context wake_io;
      boost::asio::ip::tcp::socket wake{wake_io};
      boost::system::error_code wake_ec;
      wake.connect({boost::asio::ip::make_address(host), port}, wake_ec);
      wake.close(wake_ec);
    } catch (...) {
    }
  }
  boost::system::error_code ec;
  impl_->acceptor.cancel(ec);
  impl_->acceptor.close(ec);
  impl_->io.stop();
  if (impl_->thread.joinable())
    impl_->thread.join();
  impl_->io.restart();
  impl_->port = 0;
}

std::uint16_t RouterServer::port() const noexcept
{
  return impl_ ? impl_->port.load() : 0;
}

bool RouterServer::running() const noexcept
{
  return impl_ && impl_->running.load();
}

std::string RouterServer::base_url() const
{
  if (!running())
    return {};
  return "http://" + arq_storage::format_http_authority(impl_->host, port());
}
} // namespace arq_router
