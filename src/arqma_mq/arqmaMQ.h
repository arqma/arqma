#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <zmq.hpp>
#include <map>
#include <iterator>
#include "INotifier.h"
#include <boost/utility/string_ref.hpp>

#include "cryptonote_basic/cryptonote_basic_impl.h"

#include "rpc/daemon_handler.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace cryptonote;
using namespace rpc;


namespace arqmaMQ {
    constexpr auto QUIT = "QUIT";
    constexpr auto EVICT = "EVICT";

    class ArqmaNotifier: public INotifier {
        public:
            ArqmaNotifier(DaemonHandler& h);
            ~ArqmaNotifier();
            ArqmaNotifier(const ArqmaNotifier&) = delete;
            ArqmaNotifier& operator=(const ArqmaNotifier&) = delete;
            ArqmaNotifier(ArqmaNotifier&&) = delete;
            ArqmaNotifier& operator=(ArqmaNotifier&&) = delete;
            void notify(std::string &&data);
            bool addTCPSocket(boost::string_ref address, boost::string_ref port, uint16_t max_clients);
            void run();
        private:
            std::thread proxy_thread;
			DaemonHandler& handler;
            zmq::context_t context;
            zmq::socket_t listener{context, ZMQ_ROUTER};
            zmq::socket_t producer{context, ZMQ_PAIR};
            zmq::socket_t subscriber{context, ZMQ_PAIR};
            zmq::message_t create_message(std::string &&data);
            void proxy_loop();
            std::map<std::string, std::string> remotes;
            std::string bind_address = "tcp://";
            uint16_t max_clients = 2;
    };
}

