// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//




#pragma once

#include <atomic>
#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/system/error_code.hpp>
#include <future>
#include <functional>
#include "net/net_utils_base.h"
#include "net/net_ssl.h"
#include "misc_language.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "net"

#if BOOST_VERSION >= 108700
namespace boost::asio {
  typedef io_context io_service;
}
#endif

namespace epee
{
namespace net_utils
{
    struct direct_connect
    {
      std::future<boost::asio::ip::tcp::socket> operator()(const std::string& addr, const std::string& port, boost::asio::steady_timer&) const;
    };

    class blocked_mode_client
	{
    enum try_connect_result_t
    {
      CONNECT_SUCCESS,
      CONNECT_FAILURE,
      CONNECT_NO_SSL,
    };



				struct handler_obj
				{
					handler_obj(boost::system::error_code& error,	size_t& bytes_transferred):ref_error(error), ref_bytes_transferred(bytes_transferred)
					{}
					handler_obj(const handler_obj& other_obj):ref_error(other_obj.ref_error), ref_bytes_transferred(other_obj.ref_bytes_transferred)
					{}

					boost::system::error_code& ref_error;
					size_t& ref_bytes_transferred;

					void operator()(const boost::system::error_code& error, // Result of operation.
						std::size_t bytes_transferred           // Number of bytes read.
						)
					{
						ref_error = error;
						ref_bytes_transferred = bytes_transferred;
					}
				};

	public:
    inline
      blocked_mode_client() :
          m_io_service(),
          m_ctx(boost::asio::ssl::context::tlsv12),
          m_ssl_socket(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(m_io_service, m_ctx)),
          m_connector(direct_connect{}),
          m_ssl_options(epee::net_utils::ssl_support_t::e_ssl_support_autodetect),
          m_connected(false),
          m_deadline(m_io_service, std::chrono::steady_clock::time_point::max()),
          m_shutdowned(false),
          m_bytes_sent(0),
          m_bytes_received(0)
    {
      check_deadline();
    }

    /*! The first/second parameters are host/port respectively. The third
        parameter is for setting the timeout callback - the timer is
        already set by the caller, the callee only needs to set the behavior.

        Additional asynchronous operations should be queued using the
        `io_service` from the timer. The implementation should assume
		    multi-threaded I/O processing.

        If the callee cannot start an asynchronous operation, an exception
        should be thrown to signal an immediate failure.

        The return value is a future to a connected socket. Asynchronous
        failures should use the `set_exception` method. */
    using connect_func = std::function<std::future<boost::asio::ip::tcp::socket>(const std::string&, const std::string&, boost::asio::steady_timer&)>;

		inline
		~blocked_mode_client()
		{
			//profile_tools::local_coast lc("~blocked_mode_client()", 3);
			try { shutdown(); }
			catch(...) { /* ignore */ }
		}

    inline void set_ssl(ssl_options_t ssl_options)
    {
			if(ssl_options)
				m_ctx = ssl_options.create_context();
			else
				m_ctx = boost::asio::ssl::context(boost::asio::ssl::context::tlsv12);
			m_ssl_options = std::move(ssl_options);
		}

    inline
    try_connect_result_t try_connect(const std::string& addr, const std::string& port, std::chrono::milliseconds timeout)
    {
		  m_deadline.expires_after(timeout);
      auto connection = m_connector(addr, port, m_deadline);
      do {
	      m_io_service.restart();
		    m_io_service.run_one();
		  } while (connection.wait_for(std::chrono::seconds{0}) != std::future_status::ready);

      m_ssl_socket->next_layer() = connection.get();
		  m_deadline.cancel();
		  if(m_ssl_socket->next_layer().is_open())
			{
					m_connected = true;
					m_deadline.expires_at(std::chrono::steady_clock::time_point::max());
					// SSL Options
					if(m_ssl_options.support == epee::net_utils::ssl_support_t::e_ssl_support_enabled || m_ssl_options.support == epee::net_utils::ssl_support_t::e_ssl_support_autodetect)
					{
						if(!m_ssl_options.handshake(*m_ssl_socket, boost::asio::ssl::stream_base::client, {}, addr, timeout))
						{
							if(m_ssl_options.support == epee::net_utils::ssl_support_t::e_ssl_support_autodetect)
							{
								boost::system::error_code ignored_ec;
								m_ssl_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
								m_ssl_socket->next_layer().close();
								m_connected = false;
								return CONNECT_NO_SSL;
							}
							else
							{
								MWARNING("Failed to establish SSL connection");
								m_connected = false;
								return CONNECT_FAILURE;
							}
						}
					}

					return CONNECT_SUCCESS;
				}
        else
				{
					MWARNING("Some problems at connect, expected open socket");
					return CONNECT_FAILURE;
				}

		}

    inline
		bool connect(const std::string& addr, const std::string& port, std::chrono::milliseconds timeout)
		{
			m_connected = false;
      try
      {
          m_ssl_socket->next_layer().close();

          // Set SSL options
          // disable sslv2
          m_ssl_socket.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(m_io_service, m_ctx));

          // Get a list of endpoints corresponding to the server name.

          try_connect_result_t try_connect_result = try_connect(addr, port, timeout);
          if(try_connect_result == CONNECT_FAILURE)
            return false;
          if(m_ssl_options.support == epee::net_utils::ssl_support_t::e_ssl_support_autodetect)
          {
            if(try_connect_result == CONNECT_NO_SSL)
            {
              MERROR("SSL handshake failed on an autodetect connection, reconnecting without SSL");
              m_ssl_options.support = epee::net_utils::ssl_support_t::e_ssl_support_disabled;
              if(try_connect(addr, port, timeout) != CONNECT_SUCCESS)
                return false;
            }
          }
        }
        catch(const boost::system::system_error& er)
        {
          MDEBUG("Some problems at connect, message: " << er.what());
          return false;
        }
        catch(...)
        {
          MDEBUG("Some fatal problems.");
          return false;
        }
        return true;
    }
    //! Change the connection routine (proxy, etc.)
    void set_connector(connect_func connector)
		{
			m_connector = std::move(connector);
		}

		inline
		bool disconnect()
		{
			try
			{
				if(m_connected)
				{
					m_connected = false;
                    if(m_ssl_options)
                        shutdown_ssl();
                    m_ssl_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
				}
			}
			catch(const boost::system::system_error& /*er*/)
			{
				//LOG_ERROR("Some problems at disconnect, message: " << er.what());
				return false;
			}
			catch(...)
			{
				//LOG_ERROR("Some fatal problems.");
                m_connected = false;
				return false;
			}
			return true;
		}


		inline
		bool send(const std::string& buff, std::chrono::milliseconds timeout)
		{
			try
			{
				m_deadline.expires_after(timeout);

				// Set up the variable that receives the result of the asynchronous
				// operation. The error code is set to would_block to signal that the
				// operation is incomplete. Asio guarantees that its asynchronous
				// operations will never fail with would_block, so any other value in
				// ec indicates completion.
				boost::system::error_code ec = boost::asio::error::would_block;

				async_write(buff.c_str(), buff.size(), ec);

				// Block until the asynchronous operation has completed.
				while (ec == boost::asio::error::would_block)
				{
          m_io_service.restart();
					m_io_service.run_one();
				}

				if(ec)
				{
					LOG_PRINT_L3("Problems at write: " << ec.message());
                    m_connected = false;
					return false;
				}
                else
				{
					m_deadline.expires_at(std::chrono::steady_clock::time_point::max());
                    m_bytes_sent += buff.size();
				}
			}

			catch(const boost::system::system_error& er)
			{
				LOG_ERROR("Some problems at connect, message: " << er.what());
				return false;
			}
			catch(...)
			{
				LOG_ERROR("Some fatal problems.");
				return false;
			}

			return true;
		}

		bool is_connected(bool *ssl = NULL)
		{
          if(!m_connected || !m_ssl_socket->next_layer().is_open())
            return false;
          if(ssl)
            *ssl = m_ssl_options.support != ssl_support_t::e_ssl_support_disabled;
          return true;
		}

		inline
		bool recv(std::string& buff, std::chrono::milliseconds timeout)
		{

			try
			{
				// Set a deadline for the asynchronous operation. Since this function uses
				// a composed operation (async_read_until), the deadline applies to the
				// entire operation, rather than individual reads from the socket.
				m_deadline.expires_after(timeout);

				// Set up the variable that receives the result of the asynchronous
				// operation. The error code is set to would_block to signal that the
				// operation is incomplete. Asio guarantees that its asynchronous
				// operations will never fail with would_block, so any other value in
				// ec indicates completion.
				//boost::system::error_code ec = boost::asio::error::would_block;

				boost::system::error_code ec = boost::asio::error::would_block;
				size_t bytes_transferred = 0;

				handler_obj hndlr(ec, bytes_transferred);

				static const size_t max_size = 16384;
				buff.resize(max_size);

				async_read(&buff[0], max_size, boost::asio::transfer_at_least(1), hndlr);

				// Block until the asynchronous operation has completed.
				while (ec == boost::asio::error::would_block && !m_shutdowned)
				{
				    m_io_service.restart();
					m_io_service.run_one();
				}


				if(ec)
				{
                  MTRACE("READ ENDS: Connection err_code " << ec.value());
                  if(ec == boost::asio::error::eof)
                  {
                    MTRACE("Connection err_code eof.");
                    //connection closed there, empty
                    buff.clear();
                    return true;
                  }

                    MDEBUG("Problems at read: " << ec.message());
                    m_connected = false;
					return false;
				}
                else
				{
                    MTRACE("READ ENDS: Success. bytes_tr: " << bytes_transferred);
					m_deadline.expires_at(std::chrono::steady_clock::time_point::max());
				}

				/*if(!bytes_transferred)
					return false;*/

				m_bytes_received += bytes_transferred;
                buff.resize(bytes_transferred);
				return true;
			}

			catch(const boost::system::system_error& er)
			{
				LOG_ERROR("Some problems at read, message: " << er.what());
                m_connected = false;
				return false;
			}
			catch(...)
			{
				LOG_ERROR("Some fatal problems at read.");
				return false;
			}



			return false;

		}

		bool shutdown()
		{
			m_deadline.cancel();
            boost::system::error_code ec;
			if(m_ssl_options)
				shutdown_ssl();
            m_ssl_socket->next_layer().cancel(ec);
            if(ec)
				MDEBUG("Problems at cancel: " << ec.message());
			m_ssl_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if(ec)
				MDEBUG("Problems at shutdown: " << ec.message());
			m_ssl_socket->next_layer().close(ec);
			if(ec)
				MDEBUG("Problems at close: " << ec.message());
			m_shutdowned = true;
      m_connected = false;
			return true;
		}

    uint64_t get_bytes_sent() const
		{
			return m_bytes_sent;
		}

		uint64_t get_bytes_received() const
		{
			return m_bytes_received;
		}

	private:

		void check_deadline()
		{
			// Check whether the deadline has passed. We compare the deadline against
			// the current time since a new asynchronous operation may have moved the
			// deadline before this actor had a chance to run.
			if(m_deadline.expiry() <= std::chrono::steady_clock::now())
			{
				// The deadline has passed. The socket is closed so that any outstanding
				// asynchronous operations are cancelled. This allows the blocked
				// connect(), read_line() or write_line() functions to return.
				LOG_PRINT_L3("Timed out socket");
                m_connected = false;
				m_ssl_socket->next_layer().close();

				// There is no longer an active deadline. The expiry is set to positive
				// infinity so that the actor takes no action until a new deadline is set.
				m_deadline.expires_at(std::chrono::steady_clock::time_point::max());
			}

			// Put the actor back to sleep.
			m_deadline.async_wait([this] (const boost::system::error_code&) { check_deadline(); });
		}

		void shutdown_ssl()
    {
			// ssl socket shutdown blocks if server doesn't respond. We close after 2 secs
			boost::system::error_code ec = boost::asio::error::would_block;
			m_deadline.expires_after(std::chrono::milliseconds(2000));
			m_ssl_socket->async_shutdown([&ec](const boost::system::error_code& e) { ec = e; });
			while (ec == boost::asio::error::would_block)
			{
                m_io_service.restart();
				m_io_service.run_one();
			}
			// Ignore "short read" error
			if(ec.category() == boost::asio::error::get_ssl_category() && ec.value() !=
			    boost::asio::ssl::error::stream_truncated
			    )
				MDEBUG("Problems at ssl shutdown: " << ec.message());
		}

	protected:
	  void async_write(const void* data, size_t sz, boost::system::error_code& ec)
		{
		  auto handler = [&ec](const boost::system::error_code& e, size_t) { ec = e; };
			if(m_ssl_options.support != ssl_support_t::e_ssl_support_disabled)
                boost::asio::async_write(*m_ssl_socket, boost::asio::buffer(data, sz), std::move(handler));
			else
				boost::asio::async_write(m_ssl_socket->next_layer(), boost::asio::buffer(data, sz), std::move(handler));
		}

		void async_read(char* buff, size_t sz, boost::asio::detail::transfer_at_least_t transfer_at_least, handler_obj& hndlr)
		{
			if(m_ssl_options.support == ssl_support_t::e_ssl_support_disabled)
                boost::asio::async_read(m_ssl_socket->next_layer(), boost::asio::buffer(buff, sz), transfer_at_least, hndlr);
			else
				boost::asio::async_read(*m_ssl_socket, boost::asio::buffer(buff, sz), transfer_at_least, hndlr);

		}

	protected:
		boost::asio::io_service m_io_service;
    boost::asio::ssl::context m_ctx;
		std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_ssl_socket;
    connect_func m_connector;
		ssl_options_t m_ssl_options;
		bool m_connected;
		boost::asio::steady_timer m_deadline;
		std::atomic<bool> m_shutdowned;
    std::atomic<uint64_t> m_bytes_sent;
    std::atomic<uint64_t> m_bytes_received;
	};
}
}
