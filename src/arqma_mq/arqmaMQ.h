#pragma once

#include <iostream>
#include <string>
#include <thread>
//#include <memory>
#include <zmq.hpp>
#include "INotifier.h"

namespace arqmaMQ {

    class ArqmaNotifier: public INotifier {
        public:
            ArqmaNotifier();
            ~ArqmaNotifier();
            ArqmaNotifier(const ArqmaNotifier&) = delete;
            ArqmaNotifier& operator=(const ArqmaNotifier&) = delete;
            ArqmaNotifier(ArqmaNotifier&&) = delete;
            ArqmaNotifier& operator=(ArqmaNotifier&&) = delete;
            void notify(std::string &&data);
        private:
            std::thread proxy_thread;
            zmq::context_t context;
            zmq::socket_t listener{context, ZMQ_ROUTER};
            zmq::socket_t producer{context, ZMQ_PAIR};
            zmq::socket_t subscriber{context, ZMQ_PAIR};
            zmq::message_t create_message(std::string &&data);
            void proxy_loop();
    };
}

