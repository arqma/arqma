#pragma once

#include <iostream>
#include <string>
//#include <zmq.hpp>
#include "INotifier.h"


namespace arqmaMQ {

    class ArqmaNotifier: public INotifier {
        public:
            ArqmaNotifier();
            ~ArqmaNotifier();
            void notify(std::string &&data);

        private:
//            zmq::socket_t socket;
    };
}

