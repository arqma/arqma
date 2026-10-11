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

#include "socket_stack.hpp"

#include "command_registry.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arqmq {
namespace {
constexpr const char* k_quit = "QUIT";
constexpr const char* k_bind = "BIND";
constexpr const char* k_bind_curve = "BIND_CURVE";
constexpr const char* k_send = "SEND";

std::atomic<uint64_t> g_stack_id{1};

bool recv_all_parts(zmq::socket_t& sock, std::vector<zmq::message_t>& parts, const zmq::recv_flags flags)
{
  parts.clear();
  zmq::message_t msg;
  const auto first = sock.recv(msg, flags);
  if (!first)
    return false;
  parts.push_back(std::move(msg));
  while (parts.back().more()) {
    zmq::message_t next;
    if (!sock.recv(next, zmq::recv_flags::none))
      break;
    parts.push_back(std::move(next));
  }
  return true;
}

std::string_view as_view(const zmq::message_t& msg)
{
  return {static_cast<const char*>(msg.data()), msg.size()};
}

void send_pointer(zmq::socket_t& sock, void* ptr)
{
  const auto value = reinterpret_cast<uintptr_t>(ptr);
  zmq::message_t msg{sizeof(value)};
  std::memcpy(msg.data(), &value, sizeof(value));
  sock.send(msg, zmq::send_flags::none);
}

void* recv_pointer(zmq::socket_t& sock)
{
  zmq::message_t msg;
  const auto result = sock.recv(msg, zmq::recv_flags::none);
  if (!result || msg.size() != sizeof(uintptr_t))
    return nullptr;
  uintptr_t value = 0;
  std::memcpy(&value, msg.data(), sizeof(value));
  return reinterpret_cast<void*>(value);
}

std::string default_ping_handler(const InboundRequest&)
{
  return "pong";
}

CategoryAcl acl_from_user_id(const char* user_id)
{
  if (!user_id || !user_id[0])
    return CategoryAcl::Denied;
  if (user_id[0] == 'S' && user_id[1] == ':')
    return CategoryAcl::ServiceNode;
  if (user_id[0] == 'C' && user_id[1] == ':')
    return CategoryAcl::Basic;
  return CategoryAcl::Denied;
}
} // namespace

SocketStack::SocketStack() : context_(1)
{
  const auto id = g_stack_id.fetch_add(1, std::memory_order_relaxed);
  jobs_endpoint_ = "inproc://arqmq.jobs." + std::to_string(id);
  ctrl_endpoint_ = "inproc://arqmq.ctrl." + std::to_string(id);
  register_handler("ping", CategoryAcl::Basic, default_ping_handler);
  register_handler("pong", CategoryAcl::Basic, [](const InboundRequest&) { return std::string{}; });
}

SocketStack::~SocketStack()
{
  stop();
}

std::error_code SocketStack::start()
{
  if (running_.load())
    return {};

  stop_requested_.store(false);
  try {
    worker_ = std::thread([this] { worker_main(); });
  } catch (...) {
    return std::make_error_code(std::errc::resource_unavailable_try_again);
  }

  for (int i = 0; i < 500 && !running_.load(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

  if (!running_.load()) {
    stop();
    return std::make_error_code(std::errc::timed_out);
  }
  return {};
}

std::error_code SocketStack::bind(const std::string& endpoint)
{
  if (!running_.load() || endpoint.empty())
    return std::make_error_code(std::errc::invalid_argument);

  try {
    zmq::socket_t push{context_, zmq::socket_type::push};
    push.connect(ctrl_endpoint_);
    zmq::message_t cmd{k_bind, std::strlen(k_bind)};
    push.send(cmd, zmq::send_flags::sndmore);
    zmq::message_t ep{endpoint.data(), endpoint.size()};
    push.send(ep, zmq::send_flags::none);
  } catch (...) {
    return std::make_error_code(std::errc::io_error);
  }

  for (int i = 0; i < 500; ++i) {
    {
      std::lock_guard<std::mutex> lock{bind_mu_};
      for (const auto& bound : bind_endpoints_) {
        if (bound == endpoint)
          return {};
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return std::make_error_code(std::errc::io_error);
}

void SocketStack::set_curve_identity(std::string public_key, std::string secret_key)
{
  std::lock_guard<std::mutex> lock{curve_mu_};
  curve_public_key_ = std::move(public_key);
  curve_secret_key_ = std::move(secret_key);
}

void SocketStack::set_allow_connection(AllowConnection allow)
{
  std::lock_guard<std::mutex> lock{curve_mu_};
  allow_connection_ = std::move(allow);
}

bool SocketStack::curve_zap_configured() const noexcept
{
  std::lock_guard<std::mutex> lock{curve_mu_};
  return curve_public_key_.size() == 32 && curve_secret_key_.size() == 32 && static_cast<bool>(allow_connection_);
}

std::error_code SocketStack::bind_curve(const std::string& endpoint)
{
  if (!running_.load() || endpoint.empty() || !curve_zap_configured())
    return std::make_error_code(std::errc::invalid_argument);

  const auto before = [&] {
    std::lock_guard<std::mutex> lock{bind_mu_};
    return std::make_pair(curve_bind_endpoints_.size(), curve_bind_attempts_);
  }();

  try {
    zmq::socket_t push{context_, zmq::socket_type::push};
    push.connect(ctrl_endpoint_);
    zmq::message_t cmd{k_bind_curve, std::strlen(k_bind_curve)};
    push.send(cmd, zmq::send_flags::sndmore);
    zmq::message_t ep{endpoint.data(), endpoint.size()};
    push.send(ep, zmq::send_flags::none);
  } catch (...) {
    return std::make_error_code(std::errc::io_error);
  }

  for (int i = 0; i < 500; ++i) {
    {
      std::lock_guard<std::mutex> lock{bind_mu_};
      if (curve_bind_attempts_ > before.second) {
        if (curve_bind_endpoints_.size() > before.first)
          return {};
        return std::make_error_code(std::errc::address_in_use);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return std::make_error_code(std::errc::io_error);
}

std::string SocketStack::last_curve_endpoint() const
{
  std::lock_guard<std::mutex> lock{bind_mu_};
  if (curve_bind_endpoints_.empty())
    return {};
  return curve_bind_endpoints_.back();
}

std::error_code SocketStack::send_to_peer(const std::string_view pubkey, const std::string_view command,
                                          const std::string_view payload, const std::string_view hint)
{
  if (!running_.load() || pubkey.size() != 32 || command.empty())
    return std::make_error_code(std::errc::invalid_argument);

  std::string resolved_hint{hint};
  if (resolved_hint.empty()) {
    if (const auto peer = peers_.find(pubkey))
      resolved_hint = peer->hint;
  }
  if (resolved_hint.empty())
    return std::make_error_code(std::errc::no_such_file_or_directory);

  {
    std::lock_guard<std::mutex> lock{curve_mu_};
    if (curve_public_key_.size() != 32 || curve_secret_key_.size() != 32)
      return std::make_error_code(std::errc::invalid_argument);
  }

  std::error_code ec;
  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  SendJob job{
      std::string{pubkey}, std::string{command}, std::string{payload}, std::move(resolved_hint), &ec, &mu, &cv, &done};

  try {
    zmq::socket_t push{context_, zmq::socket_type::push};
    push.connect(ctrl_endpoint_);
    zmq::message_t cmd{k_send, std::strlen(k_send)};
    push.send(cmd, zmq::send_flags::sndmore);
    send_pointer(push, &job);
  } catch (...) {
    return std::make_error_code(std::errc::io_error);
  }

  std::unique_lock<std::mutex> lock{mu};
  if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return done; }))
    return std::make_error_code(std::errc::timed_out);
  return ec;
}

void SocketStack::register_handler(std::string command, CategoryAcl required, CommandHandler handler)
{
  std::lock_guard<std::mutex> lock{handlers_mu_};
  handlers_[std::move(command)] = HandlerEntry{required, std::move(handler)};
}

std::error_code SocketStack::dispatch(const InboundRequest& request, std::string* reply)
{
  if (!running_.load())
    return std::make_error_code(std::errc::not_connected);

  std::error_code ec;
  std::string local_reply;
  std::string* reply_ptr = reply ? reply : &local_reply;
  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  Job job{request, reply_ptr, &ec, &mu, &cv, &done};

  try {
    zmq::socket_t push{context_, zmq::socket_type::push};
    push.connect(jobs_endpoint_);
    send_pointer(push, &job);
  } catch (...) {
    return std::make_error_code(std::errc::io_error);
  }

  std::unique_lock<std::mutex> lock{mu};
  if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return done; }))
    return std::make_error_code(std::errc::timed_out);
  return ec;
}

size_t SocketStack::handler_count() const
{
  std::lock_guard<std::mutex> lock{handlers_mu_};
  return handlers_.size();
}

bool SocketStack::has_handler(std::string_view command) const
{
  std::lock_guard<std::mutex> lock{handlers_mu_};
  return handlers_.find(std::string{command}) != handlers_.end();
}

const char* SocketStack::name() const noexcept
{
  return k_transport_arqmq;
}

bool SocketStack::running() const noexcept
{
  return running_.load();
}

void SocketStack::stop() noexcept
{
  if (!worker_.joinable()) {
    running_.store(false);
    return;
  }

  stop_requested_.store(true);
  try {
    zmq::socket_t push{context_, zmq::socket_type::push};
    push.connect(ctrl_endpoint_);
    zmq::message_t quit{k_quit, std::strlen(k_quit)};
    push.send(quit, zmq::send_flags::none);
  } catch (...) {
  }

  if (worker_.joinable())
    worker_.join();
  running_.store(false);
  stop_requested_.store(false);

  {
    std::lock_guard<std::mutex> lock{bind_mu_};
    bind_endpoints_.clear();
    curve_bind_endpoints_.clear();
  }
  peers_.clear();
}

void SocketStack::process_zap_requests(zmq::socket_t& zap_auth)
{
  AllowConnection allow;
  {
    std::lock_guard<std::mutex> lock{curve_mu_};
    allow = allow_connection_;
  }

  std::vector<zmq::message_t> frames;
  while (recv_all_parts(zap_auth, frames, zmq::recv_flags::dontwait)) {
    std::vector<std::string_view> views;
    views.reserve(frames.size());
    for (const auto& frame : frames)
      views.push_back(as_view(frame));
    const ZapReply reply = evaluate_curve_zap_frames(views, allow);

    try {
      zmq::message_t version{reply.version.data(), reply.version.size()};
      zap_auth.send(version, zmq::send_flags::sndmore);
      zmq::message_t req_id{reply.request_id.data(), reply.request_id.size()};
      zap_auth.send(req_id, zmq::send_flags::sndmore);
      zmq::message_t code{reply.status_code.data(), reply.status_code.size()};
      zap_auth.send(code, zmq::send_flags::sndmore);
      zmq::message_t text{reply.status_text.data(), reply.status_text.size()};
      zap_auth.send(text, zmq::send_flags::sndmore);
      zmq::message_t user{reply.user_id.data(), reply.user_id.size()};
      zap_auth.send(user, zmq::send_flags::sndmore);
      zmq::message_t meta{reply.metadata.data(), reply.metadata.size()};
      zap_auth.send(meta, zmq::send_flags::none);
    } catch (...) {
      break;
    }
  }
}

void SocketStack::process_listener_messages(zmq::socket_t& listener)
{
  std::vector<zmq::message_t> parts;
  while (recv_all_parts(listener, parts, zmq::recv_flags::dontwait)) {
    // ROUTER: [routing-id][command][payload…]
    if (parts.size() < 2)
      continue;

    CategoryAcl acl = CategoryAcl::Denied;
    try {
      acl = acl_from_user_id(parts.back().gets("User-Id"));
    } catch (...) {
      acl = CategoryAcl::Denied;
    }

    InboundRequest request;
    request.command.assign(static_cast<const char*>(parts[1].data()), parts[1].size());
    request.peer_acl = acl;
    if (parts[0].size() == 32)
      request.peer_pubkey.assign(static_cast<const char*>(parts[0].data()), parts[0].size());
    if (parts.size() >= 3)
      request.payload.assign(static_cast<const char*>(parts[2].data()), parts[2].size());

    std::string reply;
    (void)handle_job(request, &reply);
    // Request/response: ping handler returns "pong" as the reply command.
    if (request.command != "ping" || reply.empty())
      continue;
    try {
      zmq::message_t rid{parts[0].data(), parts[0].size()};
      listener.send(rid, zmq::send_flags::sndmore);
      zmq::message_t pong{reply.data(), reply.size()};
      listener.send(pong, zmq::send_flags::none);
    } catch (...) {
    }
  }
}

void SocketStack::process_dealer_messages(zmq::socket_t& dealer, const std::string& peer_pubkey)
{
  std::vector<zmq::message_t> parts;
  while (recv_all_parts(dealer, parts, zmq::recv_flags::dontwait)) {
    if (parts.empty())
      continue;
    InboundRequest request;
    request.command.assign(static_cast<const char*>(parts[0].data()), parts[0].size());
    request.peer_acl = CategoryAcl::ServiceNode;
    request.peer_pubkey = peer_pubkey;
    if (parts.size() >= 2)
      request.payload.assign(static_cast<const char*>(parts[1].data()), parts[1].size());
    (void)handle_job(request, nullptr);
  }
}

std::error_code SocketStack::worker_send_to_peer(SendJob& job, std::unordered_map<std::string, zmq::socket_t>& outgoing)
{
  std::string pub;
  std::string sec;
  {
    std::lock_guard<std::mutex> lock{curve_mu_};
    pub = curve_public_key_;
    sec = curve_secret_key_;
  }
  if (pub.size() != 32 || sec.size() != 32)
    return std::make_error_code(std::errc::invalid_argument);

  try {
    auto it = outgoing.find(job.pubkey);
    if (it == outgoing.end()) {
      zmq::socket_t dealer{context_, zmq::socket_type::dealer};
      dealer.set(zmq::sockopt::linger, 0);
      dealer.set(zmq::sockopt::curve_serverkey, zmq::buffer(job.pubkey));
      dealer.set(zmq::sockopt::curve_publickey, zmq::buffer(pub));
      dealer.set(zmq::sockopt::curve_secretkey, zmq::buffer(sec));
      dealer.set(zmq::sockopt::routing_id, zmq::buffer(pub));
      dealer.connect(job.hint);
      it = outgoing.emplace(job.pubkey, std::move(dealer)).first;
      // Give CURVE handshake a moment on localhost CI.
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    zmq::message_t cmd{job.command.data(), job.command.size()};
    if (job.payload.empty()) {
      it->second.send(cmd, zmq::send_flags::none);
    } else {
      it->second.send(cmd, zmq::send_flags::sndmore);
      zmq::message_t body{job.payload.data(), job.payload.size()};
      it->second.send(body, zmq::send_flags::none);
    }
    peers_.note_peer(job.pubkey, job.hint, true);
    return {};
  } catch (...) {
    outgoing.erase(job.pubkey);
    return std::make_error_code(std::errc::io_error);
  }
}

std::error_code SocketStack::handle_job(const InboundRequest& request, std::string* reply)
{
  if (!authorize_request(request.command, request.peer_acl, request.payload.size(), 1))
    return std::make_error_code(std::errc::permission_denied);

  HandlerEntry entry;
  {
    std::lock_guard<std::mutex> lock{handlers_mu_};
    const auto it = handlers_.find(request.command);
    if (it == handlers_.end() || !it->second.handler)
      return std::make_error_code(std::errc::operation_not_supported);
    entry = it->second;
  }

  try {
    const std::string out = entry.handler(request);
    if (reply)
      *reply = out;
    return {};
  } catch (...) {
    return std::make_error_code(std::errc::io_error);
  }
}

void SocketStack::worker_main()
{
  zmq::socket_t jobs{context_, zmq::socket_type::pull};
  zmq::socket_t ctrl{context_, zmq::socket_type::pull};
  zmq::socket_t zap_auth{context_, zmq::socket_type::rep};
  std::vector<std::unique_ptr<zmq::socket_t>> listeners;
  std::unordered_map<std::string, zmq::socket_t> outgoing;

  try {
    jobs.bind(jobs_endpoint_);
    ctrl.bind(ctrl_endpoint_);
    zap_auth.set(zmq::sockopt::linger, 0);
    zap_auth.bind(k_zap_endpoint);
  } catch (...) {
    running_.store(false);
    return;
  }

  running_.store(true);

  while (!stop_requested_.load()) {
    std::vector<zmq::pollitem_t> items;
    items.push_back({jobs.handle(), 0, ZMQ_POLLIN, 0});
    items.push_back({ctrl.handle(), 0, ZMQ_POLLIN, 0});
    items.push_back({zap_auth.handle(), 0, ZMQ_POLLIN, 0});
    const size_t listener_offset = items.size();
    for (auto& listener : listeners)
      items.push_back({listener->handle(), 0, ZMQ_POLLIN, 0});
    const size_t outgoing_offset = items.size();
    std::vector<std::string> outgoing_keys;
    outgoing_keys.reserve(outgoing.size());
    for (auto& kv : outgoing) {
      outgoing_keys.push_back(kv.first);
      items.push_back({kv.second.handle(), 0, ZMQ_POLLIN, 0});
    }

    zmq::poll(items.data(), items.size(), std::chrono::milliseconds{100});

    if (items[2].revents & ZMQ_POLLIN)
      process_zap_requests(zap_auth);

    for (size_t i = 0; i < listeners.size(); ++i) {
      if (items[listener_offset + i].revents & ZMQ_POLLIN)
        process_listener_messages(*listeners[i]);
    }

    for (size_t i = 0; i < outgoing_keys.size(); ++i) {
      if (!(items[outgoing_offset + i].revents & ZMQ_POLLIN))
        continue;
      const auto it = outgoing.find(outgoing_keys[i]);
      if (it != outgoing.end())
        process_dealer_messages(it->second, it->first);
    }

    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t cmd;
      if (!ctrl.recv(cmd, zmq::recv_flags::none))
        continue;
      const std::string command{static_cast<char*>(cmd.data()), cmd.size()};
      if (command == k_quit)
        break;
      if (command == k_send) {
        auto* send_job = static_cast<SendJob*>(recv_pointer(ctrl));
        if (!send_job)
          continue;
        std::error_code ec = worker_send_to_peer(*send_job, outgoing);
        if (send_job->ec)
          *send_job->ec = ec;
        if (send_job->mu && send_job->cv && send_job->done) {
          std::lock_guard<std::mutex> lock{*send_job->mu};
          *send_job->done = true;
          send_job->cv->notify_one();
        }
        continue;
      }
      if (command == k_bind || command == k_bind_curve) {
        zmq::message_t ep_msg;
        if (!ctrl.recv(ep_msg, zmq::recv_flags::none))
          continue;
        const std::string endpoint{static_cast<char*>(ep_msg.data()), ep_msg.size()};
        const bool curve = command == k_bind_curve;
        try {
          auto sock = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::router);
          sock->set(zmq::sockopt::linger, 0);
          if (curve) {
            std::string pub;
            std::string sec;
            {
              std::lock_guard<std::mutex> lock{curve_mu_};
              pub = curve_public_key_;
              sec = curve_secret_key_;
            }
            sock->set(zmq::sockopt::zap_domain, k_zap_auth_domain_sn);
            sock->set(zmq::sockopt::curve_server, true);
            sock->set(zmq::sockopt::curve_publickey, zmq::buffer(pub));
            sock->set(zmq::sockopt::curve_secretkey, zmq::buffer(sec));
            sock->set(zmq::sockopt::router_handover, true);
            sock->set(zmq::sockopt::router_mandatory, true);
          }
          sock->bind(endpoint);
          std::string recorded = endpoint;
          try {
            recorded = sock->get(zmq::sockopt::last_endpoint);
          } catch (...) {
          }
          listeners.push_back(std::move(sock));
          std::lock_guard<std::mutex> lock{bind_mu_};
          if (curve) {
            curve_bind_endpoints_.push_back(std::move(recorded));
            ++curve_bind_attempts_;
          } else {
            bind_endpoints_.push_back(std::move(recorded));
          }
        } catch (...) {
          if (curve) {
            std::lock_guard<std::mutex> lock{bind_mu_};
            ++curve_bind_attempts_;
          }
        }
      }
    }

    if (items[0].revents & ZMQ_POLLIN) {
      auto* raw = recv_pointer(jobs);
      auto* job = static_cast<Job*>(raw);
      if (!job)
        continue;
      std::error_code ec = handle_job(job->request, job->reply);
      if (job->ec)
        *job->ec = ec;
      if (job->mu && job->cv && job->done) {
        std::lock_guard<std::mutex> lock{*job->mu};
        *job->done = true;
        job->cv->notify_one();
      }
    }
  }

  outgoing.clear();
  listeners.clear();
  running_.store(false);
}
} // namespace arqmq
