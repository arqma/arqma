#pragma once

#include "INotifier.h"
#include <string>
#include <zmq.hpp>


namespace arqmaMQ {

    class ArqmaNotifier: public INotifier {
        public:
            ArqmaNotifier();
            ~ArqmaNotifier();
            void notify(std::string &&data);

        private:
            zmq::socket_t socket;
    };
}

