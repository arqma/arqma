#include "net/net_helper.h"

namespace epee
{
namespace net_utils
{
	std::future<boost::asio::ip::tcp::socket>
	direct_connect::operator()(const std::string& addr, const std::string& port, boost::asio::steady_timer& timeout) const
	{
		// Get a list of endpoints corresponding to the server name.
		//////////////////////////////////////////////////////////////////////////
		boost::asio::ip::tcp::resolver resolver(GET_IO_SERVICE(timeout));

		bool try_ipv6 = false;
		boost::system::error_code resolve_error;
		boost::asio::ip::tcp::resolver::results_type results;

		try
		{
		  results = resolver.resolve(boost::asio::ip::tcp::v4(), addr, port, boost::asio::ip::tcp::resolver::canonical_name, resolve_error);
			if(!results.size()) // Documentation states that successful call is guaranteed to be non-empty
		  {
		    try_ipv6 = true;
		  }
		}
		catch (const boost::system::system_error& e)
		{
		  if (resolve_error != boost::asio::error::host_not_found &&
		      resolve_error != boost::asio::error::host_not_found_try_again)
		  {
		    throw;
		  }
		  try_ipv6 = true;
		}

		if (try_ipv6)
		{
		  results = resolver.resolve(boost::asio::ip::tcp::v6(), addr, port, boost::asio::ip::tcp::resolver::canonical_name, resolve_error);
			if (!results.size())
		    throw boost::system::system_error{boost::asio::error::fault, "Failed to resolve " + addr};
		}

		struct new_connection
		{
			std::promise<boost::asio::ip::tcp::socket> result_;
			boost::asio::ip::tcp::socket socket_;

			explicit new_connection(boost::asio::io_service& io_service)
			  : result_(), socket_(io_service)
			{}
		};

		const auto shared = std::make_shared<new_connection>(GET_IO_SERVICE(timeout));
		timeout.async_wait([shared] (boost::system::error_code error)
		{
			if (error != boost::system::errc::operation_canceled && shared && shared->socket_.is_open())
			{
				shared->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
				shared->socket_.close();
			}
		});
		shared->socket_.async_connect(*results.begin(), [shared] (boost::system::error_code error)
		{
			if (shared)
			{
				if (error)
				{
				  try { throw boost::system::system_error{error}; }
					catch (...) { shared->result_.set_exception(std::current_exception()); }
			  }
				else
					shared->result_.set_value(std::move(shared->socket_));
			}
		});
		return shared->result_.get_future();
	}
}
}
