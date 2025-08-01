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

#ifndef _LEVIN_BASE_H_
#define _LEVIN_BASE_H_

#include <cstdint>

#include "net_utils_base.h"
#include "span.h"

#define LEVIN_SIGNATURE  0x0101010101012101LL  //Bender's nightmare

namespace epee
{
namespace levin
{
#pragma pack(push)
#pragma pack(1)
	struct bucket_head
	{
		uint64_t m_signature;
		uint64_t m_cb;
		bool     m_have_to_return_data;
		uint32_t m_command;
		int32_t  m_return_code;
		uint32_t m_reservedA; //probably some flags in future
		uint32_t m_reservedB; //probably some check sum in future
	};
#pragma pack(pop)


#pragma pack(push)
#pragma pack(1)
  struct bucket_head2
  {
    uint64_t m_signature;
    uint64_t m_cb;
    bool     m_have_to_return_data;
    uint32_t m_command;
    int32_t  m_return_code;
    uint32_t m_flags;
    uint32_t m_protocol_version;
  };
#pragma pack(pop)


#define LEVIN_DEFAULT_TIMEOUT_PRECONFIGURED            0
#define LEVIN_INITIAL_MAX_PACKET_SIZE           256*1024
#define LEVIN_DEFAULT_MAX_PACKET_SIZE          100000000      //100MB by default

#define LEVIN_PACKET_REQUEST			            0x00000001
#define LEVIN_PACKET_RESPONSE		              0x00000002
#define LEVIN_PACKET_BEGIN                    0x00000004
#define LEVIN_PACKET_END                      0x00000008

#define LEVIN_PROTOCOL_VER_0                  0
#define LEVIN_PROTOCOL_VER_1                  1

  template<class t_connection_context = net_utils::connection_context_base>
  struct levin_commands_handler
  {
    virtual int invoke(int command, const epee::span<const uint8_t> in_buff, std::string& buff_out, t_connection_context& context)=0;
    virtual int notify(int command, const epee::span<const uint8_t> in_buff, t_connection_context& context)=0;
    virtual void callback(t_connection_context& context){};

    virtual void on_connection_new(t_connection_context& context){};
    virtual void on_connection_close(t_connection_context& context){};

    virtual ~levin_commands_handler(){}
  };

#define LEVIN_OK                                        0
#define LEVIN_ERROR_CONNECTION                         -1
#define LEVIN_ERROR_CONNECTION_NOT_FOUND               -2
#define LEVIN_ERROR_CONNECTION_DESTROYED               -3
#define LEVIN_ERROR_CONNECTION_TIMEDOUT                -4
#define LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL      -5
#define LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED     -6
#define LEVIN_ERROR_FORMAT                             -7

#define DESCRIBE_RET_CODE(code) case code: return #code;
  inline
  const char* get_err_descr(int err)
  {
    switch(err)
    {
      DESCRIBE_RET_CODE(LEVIN_OK);
      DESCRIBE_RET_CODE(LEVIN_ERROR_CONNECTION);
      DESCRIBE_RET_CODE(LEVIN_ERROR_CONNECTION_NOT_FOUND);
      DESCRIBE_RET_CODE(LEVIN_ERROR_CONNECTION_DESTROYED);
      DESCRIBE_RET_CODE(LEVIN_ERROR_CONNECTION_TIMEDOUT);
      DESCRIBE_RET_CODE(LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL);
      DESCRIBE_RET_CODE(LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED);
      DESCRIBE_RET_CODE(LEVIN_ERROR_FORMAT);
    default:
      return "unknown code";
    }
  }

  bucket_head2 make_header(uint32_t command, uint64_t msg_size, uint32_t flags, bool expect_response) noexcept;

  byte_slice make_notify(int command, epee::span<const std::uint8_t> payload);
  byte_slice make_noise_notify(std::size_t noise_bytes);
  byte_slice make_fragmented_notify(const byte_slice& noise, int command, epee::span<const std::uint8_t> payload);

}
}

#endif //_LEVIN_BASE_H_
