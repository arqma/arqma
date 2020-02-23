#pragma once

#include <iostream>
#include <string>
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
            zmq::context_t context;
            zmq::socket_t socket{context, ZMQ_PUB};
            zmq::message_t create_message(std::string &&data);
    };
}

