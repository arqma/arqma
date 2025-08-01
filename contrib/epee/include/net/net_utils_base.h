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



#ifndef _NET_UTILS_BASE_H_
#define _NET_UTILS_BASE_H_

#include <boost/uuid/uuid.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <typeinfo>
#include <type_traits>
#include "byte_slice.h"
#include "enums.h"
#include "misc_log_ex.h"
#include "serialization/keyvalue_serialization.h"
#include "int-util.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "net"

#ifndef MAKE_IP
  #define MAKE_IP( a1, a2, a3, a4 )	(a1|(a2<<8)|(a3<<16)|(a4<<24))
#endif

namespace boost::asio {
  using io_service = io_context;
}

#if BOOST_VERSION >= 107000
  #define GET_IO_SERVICE(s) ((boost::asio::io_context&)(s).get_executor().context())
#else
  #define GET_IO_SERVICE(s) ((s).get_io_service())
#endif

namespace net
{
	class tor_address;
	class i2p_address;
}


namespace epee
{
namespace net_utils
{
	class ipv4_network_address
	{
		uint32_t m_ip;
		uint16_t m_port;

	public:
		constexpr ipv4_network_address() noexcept
		  : ipv4_network_address(0, 0)
		{}

		constexpr ipv4_network_address(uint32_t ip, uint16_t port) noexcept
			: m_ip(ip), m_port(port) {}

		bool equal(const ipv4_network_address& other) const noexcept;
		bool less(const ipv4_network_address& other) const noexcept;
		constexpr bool is_same_host(const ipv4_network_address& other) const noexcept
		{ return ip() == other.ip(); }

		constexpr uint32_t ip() const noexcept { return m_ip; }
		constexpr uint16_t port() const noexcept { return m_port; }
		std::string str() const;
		std::string host_str() const;
		bool is_loopback() const;
		bool is_local() const;
		static constexpr address_type get_type_id() noexcept { return address_type::ipv4; }
		static constexpr zone get_zone() noexcept { return zone::public_; }
		static constexpr bool is_blockable() noexcept { return true; }

		BEGIN_KV_SERIALIZE_MAP()
			KV_SERIALIZE(m_ip)
			KV_SERIALIZE(m_port)
		END_KV_SERIALIZE_MAP()
	};

	inline bool operator==(const ipv4_network_address& lhs, const ipv4_network_address& rhs) noexcept
	{ return lhs.equal(rhs); }
	inline bool operator!=(const ipv4_network_address& lhs, const ipv4_network_address& rhs) noexcept
	{ return !lhs.equal(rhs); }
	inline bool operator<(const ipv4_network_address& lhs, const ipv4_network_address& rhs) noexcept
	{ return lhs.less(rhs); }
	inline bool operator<=(const ipv4_network_address& lhs, const ipv4_network_address& rhs) noexcept
	{ return !rhs.less(lhs); }
	inline bool operator>(const ipv4_network_address& lhs, const ipv4_network_address& rhs) noexcept
	{ return rhs.less(lhs); }
	inline bool operator>=(const ipv4_network_address& lhs, const ipv4_network_address& rhs) noexcept
	{ return !lhs.less(rhs); }

	class ipv6_network_address
	{
	protected:
	  boost::asio::ip::address_v6 m_address;
	  uint16_t m_port;

	public:
	  ipv6_network_address()
	    : ipv6_network_address(boost::asio::ip::address_v6::loopback(), 0)
	  {}

	  ipv6_network_address(const boost::asio::ip::address_v6& ip, uint16_t port)
	    : m_address(ip), m_port(port)
	  {
	  }

	  bool equal(const ipv6_network_address& other) const noexcept;
	  bool less(const ipv6_network_address& other) const noexcept;
	  bool is_same_host(const ipv6_network_address& other) const noexcept { return m_address == other.m_address; }

	  boost::asio::ip::address_v6 ip() const noexcept { return m_address; }
	  uint16_t port() const noexcept { return m_port; }
	  std::string str() const;
	  std::string host_str() const;
	  bool is_loopback() const;
	  bool is_local() const;
	  static constexpr address_type get_type_id() noexcept { return address_type::ipv6; }
	  static constexpr zone get_zone() noexcept { return zone::public_; }
	  static constexpr bool is_blockable() noexcept { return true; }

	  static const uint8_t ID = 2;
	  BEGIN_KV_SERIALIZE_MAP()
	    boost::asio::ip::address_v6::bytes_type bytes = this_ref.m_address.to_bytes();
	    epee::serialization::selector<is_store>::serialize_t_val_as_blob(bytes, stg, hparent_section, "addr");
	    const_cast<boost::asio::ip::address_v6&>(this_ref.m_address) = boost::asio::ip::address_v6(bytes);
	    KV_SERIALIZE(m_port)
	  END_KV_SERIALIZE_MAP()
	};

	inline bool operator==(const ipv6_network_address& lhs, const ipv6_network_address& rhs) noexcept
	{ return lhs.equal(rhs); }
	inline bool operator!=(const ipv6_network_address& lhs, const ipv6_network_address& rhs) noexcept
	{ return !lhs.equal(rhs); }
	inline bool operator<(const ipv6_network_address& lhs, const ipv6_network_address& rhs) noexcept
	{ return lhs.less(rhs); }
	inline bool operator<=(const ipv6_network_address& lhs, const ipv6_network_address& rhs) noexcept
	{ return !rhs.less(lhs); }
	inline bool operator>(const ipv6_network_address& lhs, const ipv6_network_address& rhs) noexcept
	{ return rhs.less(lhs); }
	inline bool operator>=(const ipv6_network_address& lhs, const ipv6_network_address& rhs) noexcept
	{ return !lhs.less(rhs); }

	class network_address
	{
		struct interface
		{
			virtual ~interface() {};

			virtual bool equal(const interface&) const = 0;
			virtual bool less(const interface&) const = 0;
			virtual bool is_same_host(const interface&) const = 0;

			virtual std::string str() const = 0;
			virtual std::string host_str() const = 0;
			virtual bool is_loopback() const = 0;
			virtual bool is_local() const = 0;
			virtual address_type get_type_id() const = 0;
			virtual zone get_zone() const = 0;
			virtual bool is_blockable() const = 0;
		};

		template<typename T>
		struct implementation final : interface
		{
			T value;

			implementation(const T& src) : value(src) {}
			~implementation() = default;

			// Type-checks for cast are done in cpp
			static const T& cast(const interface& src) noexcept
			{ return static_cast<const implementation<T>&>(src).value; }

			virtual bool equal(const interface& other) const override
			{ return value.equal(cast(other)); }

			virtual bool less(const interface& other) const override
			{ return value.less(cast(other)); }

			virtual bool is_same_host(const interface& other) const override
			{ return value.is_same_host(cast(other)); }

			virtual std::string str() const override { return value.str(); }
			virtual std::string host_str() const override { return value.host_str(); }
			virtual bool is_loopback() const override { return value.is_loopback(); }
			virtual bool is_local() const override { return value.is_local(); }
			virtual address_type get_type_id() const override { return value.get_type_id(); }
			virtual zone get_zone() const override { return value.get_zone(); }
			virtual bool is_blockable() const override { return value.is_blockable(); }
		};

		std::shared_ptr<interface> self;

		template<typename Type>
		Type& as_mutable() const
		{
			// types `implmentation<Type>` and `implementation<const Type>` are unique
			using Type_ = typename std::remove_const<Type>::type;
			network_address::interface* const self_ = self.get(); // avoid clang warning in typeid
			if (!self_ || typeid(implementation<Type_>) != typeid(*self_))
				throw std::bad_cast{};
			return static_cast<implementation<Type_>*>(self_)->value;
		}

		template<typename T, typename t_storage>
		bool serialize_addr(std::false_type, t_storage& stg, typename t_storage::hsection hparent)
		{
			T addr{};
			if (!epee::serialization::selector<false>::serialize(addr, stg, hparent, "addr"))
				return false;
			*this = std::move(addr);
			return true;
		}

		template<typename T, typename t_storage>
		bool serialize_addr(std::true_type, t_storage& stg, typename t_storage::hsection hparent) const
		{
			return epee::serialization::selector<true>::serialize(as<T>(), stg, hparent, "addr");
		}

	public:
		network_address() : self(nullptr) {}
		template<typename T>
		network_address(const T& src)
			: self(std::make_shared<implementation<T>>(src)) {}
		bool equal(const network_address &other) const;
		bool less(const network_address &other) const;
		bool is_same_host(const network_address &other) const;
		std::string str() const { return self ? self->str() : "<none>"; }
		std::string host_str() const { return self ? self->host_str() : "<none>"; }
		bool is_loopback() const { return self ? self->is_loopback() : false; }
		bool is_local() const { return self ? self->is_local() : false; }
		address_type get_type_id() const { return self ? self->get_type_id() : address_type::invalid; }
		zone get_zone() const { return self ? self->get_zone() : zone::invalid; }
		bool is_blockable() const { return self ? self->is_blockable() : false; }
		template<typename Type> const Type &as() const { return as_mutable<const Type>(); }

		BEGIN_KV_SERIALIZE_MAP()
			// need to `#include "net/[i2p|tor]_address.h"` when serializing `network_address`
			static constexpr std::integral_constant<bool, is_store> is_store_{};

			std::uint8_t type = std::uint8_t(is_store ? this_ref.get_type_id() : address_type::invalid);
			if (!epee::serialization::selector<is_store>::serialize(type, stg, hparent_section, "type"))
				return false;

			switch (address_type(type))
			{
				case address_type::ipv4:
				  return this_ref.template serialize_addr<ipv4_network_address>(is_store_, stg, hparent_section);
				case address_type::ipv6:
				  return this_ref.template serialize_addr<ipv6_network_address>(is_store_, stg, hparent_section);
				case address_type::tor:
				  return this_ref.template serialize_addr<net::tor_address>(is_store_, stg, hparent_section);
				case address_type::i2p:
				  return this_ref.template serialize_addr<net::i2p_address>(is_store_, stg, hparent_section);
			  case address_type::invalid:
				default:
					break;
			}

			MERROR("Unsupported network address type: " << (unsigned)type);
			return false;
		END_KV_SERIALIZE_MAP()
	};

	inline bool operator==(const network_address& lhs, const network_address& rhs)
	{ return lhs.equal(rhs); }
	inline bool operator!=(const network_address& lhs, const network_address& rhs)
	{ return !lhs.equal(rhs); }
	inline bool operator<(const network_address& lhs, const network_address& rhs)
	{ return lhs.less(rhs); }
	inline bool operator<=(const network_address& lhs, const network_address& rhs)
	{ return !rhs.less(lhs); }
	inline bool operator>(const network_address& lhs, const network_address& rhs)
	{ return rhs.less(lhs); }
	inline bool operator>=(const network_address& lhs, const network_address& rhs)
	{ return !lhs.less(rhs); }

	/************************************************************************/
	/*                                                                      */
	/************************************************************************/
	struct connection_context_base
	{
    const boost::uuids::uuid m_connection_id;
    const network_address m_remote_address;
    const bool     m_is_income;
    std::chrono::steady_clock::time_point m_started;
    const bool     m_ssl;
    std::chrono::steady_clock::time_point m_last_recv;
    std::chrono::steady_clock::time_point m_last_send;
    uint64_t m_recv_cnt;
    uint64_t m_send_cnt;
    double m_current_speed_down;
    double m_current_speed_up;
    double m_max_speed_down;
    double m_max_speed_up;

    connection_context_base(boost::uuids::uuid connection_id,
                            const network_address &remote_address, bool is_income, bool ssl,
                            std::chrono::steady_clock::time_point last_recv = std::chrono::steady_clock::time_point::min(),
                            std::chrono::steady_clock::time_point last_send = std::chrono::steady_clock::time_point::min(),
                            uint64_t recv_cnt = 0, uint64_t send_cnt = 0):
                                            m_connection_id(connection_id),
                                            m_remote_address(remote_address),
                                            m_is_income(is_income),
                                            m_started(std::chrono::steady_clock::now()),
                                            m_ssl(ssl),
                                            m_last_recv(last_recv),
                                            m_last_send(last_send),
                                            m_recv_cnt(recv_cnt),
                                            m_send_cnt(send_cnt),
                                            m_current_speed_down(0),
                                            m_current_speed_up(0),
                                            m_max_speed_down(0),
                                            m_max_speed_up(0)
    {}

    connection_context_base(): m_connection_id(),
                               m_remote_address(),
                               m_is_income(false),
                               m_started(std::chrono::steady_clock::now()),
                               m_ssl(false),
                               m_last_recv(std::chrono::steady_clock::time_point::min()),
                               m_last_send(std::chrono::steady_clock::time_point::min()),
                               m_recv_cnt(0),
                               m_send_cnt(0),
                               m_current_speed_down(0),
                               m_current_speed_up(0),
                               m_max_speed_down(0),
                               m_max_speed_up(0)
    {}
    
    connection_context_base(const connection_context_base& a): connection_context_base()
    {
      set_details(a.m_connection_id, a.m_remote_address, a.m_is_income, a.m_ssl);
    }

    connection_context_base& operator=(const connection_context_base& a)
    {
      set_details(a.m_connection_id, a.m_remote_address, a.m_is_income, a.m_ssl);
      return *this;
    }

  private:
    template<class t_protocol_handler>
    friend class connection;
    void set_details(boost::uuids::uuid connection_id, const network_address &remote_address, bool is_income, bool ssl)
    {
      this->~connection_context_base();
      new(this) connection_context_base(connection_id, remote_address, is_income, ssl);
    }

	};

	/************************************************************************/
	/*                                                                      */
	/************************************************************************/
	struct i_service_endpoint
	{
    virtual bool do_send(byte_slice message) = 0;
    virtual bool close() = 0;
    virtual bool send_done() = 0;
    virtual bool call_run_once_service_io() = 0;
    virtual bool request_callback() = 0;
    virtual boost::asio::io_service& get_io_service() = 0;
    //protect from deletion connection object(with protocol instance) during external call "invoke"
    virtual bool add_ref() = 0;
    virtual bool release() = 0;
  protected:
    virtual ~i_service_endpoint() noexcept(false) {}
	};


  //some helpers


  std::string print_connection_context(const connection_context_base& ctx);
  std::string print_connection_context_short(const connection_context_base& ctx);

inline MAKE_LOGGABLE(connection_context_base, ct, os)
{
  os << "[" << epee::net_utils::print_connection_context_short(ct) << "] ";
  return os;
}

#define LOG_ERROR_CC(ct, message) MERROR(ct << message)
#define LOG_WARNING_CC(ct, message) MWARNING(ct << message)
#define LOG_INFO_CC(ct, message) MINFO(ct << message)
#define LOG_DEBUG_CC(ct, message) MDEBUG(ct << message)
#define LOG_TRACE_CC(ct, message) MTRACE(ct << message)
#define LOG_CC(level, ct, message) MLOG(level, ct << message)

#define LOG_PRINT_CC_L0(ct, message) LOG_PRINT_L0(ct << message)
#define LOG_PRINT_CC_L1(ct, message) LOG_PRINT_L1(ct << message)
#define LOG_PRINT_CC_L2(ct, message) LOG_PRINT_L2(ct << message)
#define LOG_PRINT_CC_L3(ct, message) LOG_PRINT_L3(ct << message)
#define LOG_PRINT_CC_L4(ct, message) LOG_PRINT_L4(ct << message)

#define LOG_PRINT_CCONTEXT_L0(message) LOG_PRINT_CC_L0(context, message)
#define LOG_PRINT_CCONTEXT_L1(message) LOG_PRINT_CC_L1(context, message)
#define LOG_PRINT_CCONTEXT_L2(message) LOG_PRINT_CC_L2(context, message)
#define LOG_PRINT_CCONTEXT_L3(message) LOG_PRINT_CC_L3(context, message)
#define LOG_ERROR_CCONTEXT(message)    LOG_ERROR_CC(context, message)

#define CHECK_AND_ASSERT_MES_CC(condition, return_val, err_message) CHECK_AND_ASSERT_MES(condition, return_val, "[" << epee::net_utils::print_connection_context_short(context) << "]" << err_message)

}
}

#endif //_NET_UTILS_BASE_H_
