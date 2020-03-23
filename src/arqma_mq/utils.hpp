// Copyright (c)2020, The Arqma Network
// Copyright (c)2020, Gary Rusher
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
//
// Parts of this file are copyright (c) 2014-2020 The Arqma Network
// Parts of this file are copyright (c) 2020-2021 Gary Rusher

#include <zmq.hpp>
#include <string>
#include <iostream>

namespace arqmaMQ 
{
    extern "C" void message_buffer_destroy(void*, void* hint) {
        delete reinterpret_cast<std::string*>(hint);
    }

    inline static int
    s_send(void *socket, const char *string, int flags = 0) {
        int rc;
        zmq_msg_t message;
        zmq_msg_init_size(&message, strlen(string));
        memcpy(zmq_msg_data(&message), string, strlen(string));
        rc = zmq_msg_send(&message, socket, flags);
        assert(-1 != rc);
        zmq_msg_close(&message);
        return (rc);
    }

    inline static bool
    s_send (zmq::socket_t & socket, const std::string & string, int flags = 0) {

        zmq::message_t message(string.size());
        memcpy (message.data(), string.data(), string.size());
        bool rc = socket.send (message, flags);
        return (rc);
    }

    inline static bool 
    s_sendmore (zmq::socket_t & socket, const std::string & string) {
        zmq::message_t message(string.size());
        memcpy (message.data(), string.data(), string.size());
        bool rc = socket.send (message, ZMQ_SNDMORE);
        return (rc);
    }

	inline static zmq::message_t
	create_message(std::string &&data)
	{
		auto *buffer = new std::string(std::move(data));
		return zmq::message_t(&(*buffer)[0], buffer->size(), message_buffer_destroy, buffer);
	}
}
