// Copyright (c)2020, The Arqma Network
// Copyright (c)2020, Gary Rusher
// Portions of this software are available under BSD-3 license. Please see ORIGINAL-LICENSE for details

// All rights reserved.

// Authors and copyright holders give permission for following:

// 1. Redistribution and use in source and binary forms WITHOUT modification.

// 2. Modification of the source form for your own personal use.

// As long as the following conditions are met:

// 3. You must not distribute modified copies of the work to third parties. This includes
//    posting the work online, or hosting copies of the modified work for download.

// 4. Any derivative version of this work is also covered by this license, including point 8.

// 5. Neither the name of the copyright holders nor the names of the authors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.

// 6. You agree that this licence is governed by and shall be construed in accordance
//    with the laws of England and Wales.

// 7. You agree to submit all disputes arising out of or in connection with this licence
//    to the exclusive jurisdiction of the Courts of England and Wales.

// Authors and copyright holders agree that:

// 8. This licence expires and the work covered by it is released into the
//    public domain on 1st of March 2021

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "arqmaMQ.h"
#include <cstdint>
#include <system_error>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/preprocessor/stringize.hpp>
#include "include_base_utils.h"
#include "string_tools.h"
using namespace epee;

#include "common/command_line.h"
#include "common/util.h"
#include "int-util.h"
#include "p2p/net_node.h"
#include "version.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "daemon.zmq"

/*
namespace cryptonote
{
  void arqma_zmq_server::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_zmq_enable);
    command_line::add_arg(desc, arg_zmq_bind_ip);
    command_line::add_arg(desc, arg_zmq_bind_port);
    command_line::add_arg(desc, arg_zmq_max_clients);
  }
  //-
  arqma_zmq_server::arqma_zmq_server(
      core& cr
    , nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core>>& p2p
    )
    : m_core(cr)
    , m_p2p(p2p)
  {}
  //-
  bool arqma_zmq_server::init(const boost::program_options::variables_map& vm, const bool enabled)
  {
    m_enabled = enabled;
    m_net_server.set_threads_prefix("ZMQ");
  }
} // namespace cryptonote
*/


namespace arqmaMQ 
{

	 extern "C" void message_buffer_destroy(void*, void* hint) {
 		delete reinterpret_cast<std::string*>(hint);
	}

	zmq::message_t ArqmaNotifier::create_message(std::string &&data)
	{
		auto *buffer = new std::string(std::move(data));
		return zmq::message_t(&(*buffer)[0], buffer->size(), message_buffer_destroy, buffer);
	}

    ArqmaNotifier::ArqmaNotifier(ZmqHandler& h): handler(h)
    {}

    ArqmaNotifier::~ArqmaNotifier()
    {}

	void ArqmaNotifier::stop()
	{
		producer.send(create_message(std::move("QUIT")), 0);
		proxy_thread.join();
		zmq_close(&producer);
		zmq_close(&subscriber);
        zmq_term(&context);
	}

    void ArqmaNotifier::run()
    {
        producer.bind("inproc://backend");
        proxy_thread = std::thread{&ArqmaNotifier::proxy_loop, this};
    }

    bool ArqmaNotifier::addTCPSocket(boost::string_ref address, boost::string_ref port, uint16_t clients)
    {
        if(address.empty())
            address = "*";
        if(port.empty())
            port = "*";

        bind_address.append(address.data(), address.size());
        bind_address += ":";
        bind_address.append(port.data(), port.size());

        max_clients = clients;
		remotes.max_size = clients;
        return true;
    }
    void ArqmaNotifier::proxy_loop()
    {
        subscriber.connect("inproc://backend");
        listener.setsockopt<int>(ZMQ_ROUTER_HANDOVER, 1);
        listener.setsockopt<int>(ZMQ_ROUTER_MANDATORY, 1);
        listener.bind(bind_address);
        zmq::pollitem_t items[2];
        items[0].socket = (void*)subscriber;
        items[0].fd = 0;
        items[0].events = ZMQ_POLLIN;
        items[1].socket = (void*)listener;
        items[1].fd = 0;
        items[1].events = ZMQ_POLLIN;


        while (true) 
        {
			std::string  block_hash;
            int rc = zmq::poll(items, 2, 1);

            if (items[1].revents & ZMQ_POLLIN)
   	        {
       	        zmq::message_t envelope1;
           	    listener.recv(&envelope1);
           	 	std::string remote_identifier = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
				if (remote_identifier.compare("block") == 0)
				{
               		listener.recv(&envelope1);
               		listener.recv(&envelope1);
               		block_hash = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
                	LOG_PRINT_L1("received from blockchain " <<  block_hash);
                    for (auto iterator = remotes.cbegin(); iterator != remotes.cend();)
                    {
						try
						{
		    				std::string response = handler.handle(iterator->second);
                    		LOG_PRINT_L1("sending client " << iterator->first << " " << response);
							listener.send(create_message(std::string(iterator->first)), ZMQ_SNDMORE);
							listener.send(create_message(std::move(response)), ZMQ_DONTWAIT);
							++iterator;

						}
						catch(const zmq::error_t &error)
						{
							if (error.num() == EHOSTUNREACH)
							{
    	                        LOG_PRINT_L1("evicting client " << iterator->first);
	                            remotes.erase(iterator++);
							}
						}
                    }
				}
				else 
				{
               		listener.recv(&envelope1);
               		listener.recv(&envelope1);
               		std::string request = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
                	LOG_PRINT_L1("received from client " <<  request);
                    if (request.compare(EVICT) == 0)
                    {
                        LOG_PRINT_L1("evicting client " << remote_identifier);
                        remotes.erase(remote_identifier);
                    }
                    else
                    {
						std::pair<std::string, std::string> remote(std::move(remote_identifier), std::move(request));
                        if (remotes.addRemote(remote))
						{
	                        std::string response = handler.handle(remote.second);
    	                    LOG_PRINT_L1("sending client " << remote.first << " " << response);
                            listener.send(create_message(std::string(remote.first)), ZMQ_SNDMORE);
                            listener.send(create_message(std::move(response)), ZMQ_DONTWAIT);
						}
                    }
				}
            }



            if (items[0].revents & ZMQ_POLLIN)
            {
                zmq::message_t envelope;
                subscriber.recv(&envelope);
                std::string stop = std::string(static_cast<char*>(envelope.data()), envelope.size());
                if (stop == QUIT) 
                {
                    LOG_PRINT_L1("closing thread");
                    break;
                }
            }
        }
        zmq_close(&listener);
    }
}
