// Copyright (c) 2019-2022, The Arqma Network
// Copyright (c) 2019, The Monero Project
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

#include "socks_connect.h"

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <memory>
#include <system_error>

#include "net/error.h"
#include "net/net_utils_base.h"
#include "net/socks.h"
#include "string_tools.h"
#include "string_tools_lexical.h"

namespace net
{
namespace socks
{
    std::future<boost::asio::ip::tcp::socket>
    connector::operator()(const std::string& remote_host, const std::string& remote_port, boost::asio::steady_timer& timeout) const
    {
        struct future_socket
        {
            std::promise<boost::asio::ip::tcp::socket> result_;

            void operator()(boost::system::error_code error, boost::asio::ip::tcp::socket&& socket)
            {
                if (error)
                {
                  try { throw boost::system::system_error{error}; }
                  catch (...) { result_.set_exception(std::current_exception()); }
                }
                else
                    result_.set_value(std::move(socket));
            }
        };

        std::future<boost::asio::ip::tcp::socket> out{};
        {
            std::uint16_t port = 0;
            if (!epee::string_tools::get_xtype_from_string(port, remote_port))
                throw std::system_error{net::error::invalid_port, "Remote port for socks proxy"};

            bool is_set = false;
            std::uint32_t ip_address = 0;
            std::promise<boost::asio::ip::tcp::socket> result{};
            out = result.get_future();
            const auto proxy = net::socks::make_connect_client(
                boost::asio::ip::tcp::socket{GET_IO_SERVICE(timeout)}, net::socks::version::v4a, future_socket{std::move(result)}
            );

            if (epee::string_tools::get_ip_int32_from_string(ip_address, remote_host))
                is_set = proxy->set_connect_command(epee::net_utils::ipv4_network_address{ip_address, port});
            else
                is_set = proxy->set_connect_command(remote_host, port);

            if (!is_set || !net::socks::client::connect_and_send(proxy, proxy_address))
                throw std::system_error{net::error::invalid_host, "Address for socks proxy"};

            timeout.async_wait(net::socks::client::async_close{std::move(proxy)});
        }

        return out;
    }
} // socks
} // net
