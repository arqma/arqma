// Copyright (c)2020, The Arqma Network
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
//
// Parts of this file are copyright (c) 2014-2019 The Monero Project
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers


#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <zmq.hpp>
#include <map>
#include <iterator>
#include "INotifier.h"
#include <boost/utility/string_ref.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/bind.hpp>
#include "common/command_line.h"

#include "cryptonote_basic/cryptonote_basic_impl.h"

#include "zmq_handler.h"
#include "daemon/command_line_args.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace cryptonote;
using namespace rpc;


namespace arqmaMQ {

    constexpr auto QUIT = "QUIT";
    constexpr auto EVICT = "EVICT";

    template<class K, class V>
    class ClientMap
    {
        public:
        size_t max_size = 0;
        std::map<K, V> clients;
        bool addRemote(K key, V value) {
            auto it = clients.find(key);
			bool result = false;
            if ( !(it != clients.end()) && (clients.size() + 1) <= max_size ) {
                clients.insert( it, std::make_pair(key,value) );
				result = true;
            }
			return result;
        }

        bool addRemote(std::pair<K, V> remote) {
            auto it = clients.find(remote.first);
			bool result = false;
            if ( !(it != clients.end()) && (clients.size() + 1) <= max_size ) {
                clients.insert( it, remote );
				result = true;
            }
			return result;
        }

        void erase(K key) {
            clients.erase(key);
        }

		void erase(typename std::map<K, V>::const_iterator iterator) {
			clients.erase(iterator);
		}

		size_t size() {
			return clients.size();
		}

        typename std::map<K, V>::iterator begin() {return clients.begin();}
        typename std::map<K, V>::iterator end() {return clients.end();}
        typename std::map<K, V>::const_iterator begin() const {return clients.begin();}
        typename std::map<K, V>::const_iterator end() const {return clients.end();}
        typename std::map<K, V>::const_iterator cbegin() const {return clients.cbegin();}
        typename std::map<K, V>::const_iterator cend() const {return clients.cend();}
    };


    class ArqmaNotifier: public INotifier {
        public:
            ArqmaNotifier(ZmqHandler& h);
            ~ArqmaNotifier();
            ArqmaNotifier(const ArqmaNotifier&) = delete;
            ArqmaNotifier& operator=(const ArqmaNotifier&) = delete;
            ArqmaNotifier(ArqmaNotifier&&) = delete;
            ArqmaNotifier& operator=(ArqmaNotifier&&) = delete;
            bool addTCPSocket(boost::string_ref address, boost::string_ref port, uint16_t max_clients);
            void run();
			void stop();
        private:
            std::thread proxy_thread;
			ZmqHandler& handler;
            zmq::context_t context;
            zmq::socket_t listener{context, ZMQ_ROUTER};
            zmq::socket_t producer{context, ZMQ_PAIR};
            zmq::socket_t subscriber{context, ZMQ_PAIR};
            void proxy_loop();
            ClientMap<std::string, std::string> remotes;
            std::string bind_address = "tcp://";
            uint16_t max_clients = 0;
            bool m_enabled = false;
    };
}

